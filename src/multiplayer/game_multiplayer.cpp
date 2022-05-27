#include <array>
#include <map>
#include <memory>
#include <queue>
#include <charconv>
#include <utility>
#include <bitset>
#include <algorithm>

#include "game_multiplayer.h"
#include "../output.h"
#include "../game_player.h"
#include "../game_playerother.h"
#include "../sprite_character.h"
#include "../window_base.h"
#include "../drawable_mgr.h"
#include "../scene.h"
#include "../bitmap.h"
#include "../font.h"
#include "../input.h"
#include "../game_map.h"
#include "../game_party.h"
#include "../game_system.h"
#include "../game_screen.h"
#include "../game_switches.h"
#include "../game_variables.h"
#include "../player.h"
#include "../cache.h"
#include "chatname.h"
#include "web_api.h"
#include "yno_connection.h"
#include "yno_messages.h"
#include "yno_packet_limiter.h"

static Game_Multiplayer _instance;

Game_Multiplayer& Game_Multiplayer::Instance() {
	return _instance;
}

Game_Multiplayer::Game_Multiplayer() {
	InitConnection();
}

void Game_Multiplayer::SpawnOtherPlayer(int id) {
	auto& player = Main_Data::game_player;
	auto& nplayer = players[id].ch;
	nplayer = std::make_unique<Game_PlayerOther>();
	nplayer->SetX(player->GetX());
	nplayer->SetY(player->GetY());
	nplayer->SetSpriteGraphic(player->GetSpriteName(), player->GetSpriteIndex());
	nplayer->SetMoveSpeed(player->GetMoveSpeed());
	nplayer->SetMoveFrequency(player->GetMoveFrequency());
	nplayer->SetThrough(true);
	nplayer->SetLayer(player->GetLayer());
	nplayer->SetMultiplayerVisible(false);
	nplayer->SetBaseOpacity(0);

	auto scene_map = Scene::Find(Scene::SceneType::Map);
	if (scene_map == nullptr) {
		Output::Debug("unexpected");
		return;
	}
	auto old_list = &DrawableMgr::GetLocalList();
	DrawableMgr::SetLocalList(&scene_map->GetDrawableList());
	players[id].sprite = std::make_unique<Sprite_Character>(nplayer.get());
	players[id].sprite->SetTone(Main_Data::game_screen->GetTone());
	DrawableMgr::SetLocalList(old_list);
}

//this assumes that the player is stopped
static void MovePlayerToPos(std::unique_ptr<Game_PlayerOther> &player, int x, int y) {
	if (!player->IsStopping()) {
		Output::Debug("MovePlayerToPos unexpected error: the player is busy being animated");
	}
	int dx = x - player->GetX();
	int dy = y - player->GetY();
	int adx = abs(dx);
	int ady = abs(dy);
	if (Game_Map::LoopHorizontal() && adx == Game_Map::GetWidth() - 1) {
		dx = dx > 0 ? -1 : 1;
		adx = 1;
	}
	if (Game_Map::LoopVertical() && ady == Game_Map::GetHeight() - 1) {
		dy = dy > 0 ? -1 : 1;
		ady = 1;
	}
	if (adx > 1 || ady > 1 || (dx == 0 && dy == 0) || !player->IsMultiplayerVisible()) {
		player->SetX(x);
		player->SetY(y);
		return;
	}
	int dir[3][3] = {{Game_Character::Direction::UpLeft, Game_Character::Direction::Up, Game_Character::Direction::UpRight},
					 {Game_Character::Direction::Left, 0, Game_Character::Direction::Right},
					 {Game_Character::Direction::DownLeft, Game_Character::Direction::Down, Game_Character::Direction::DownRight}};
	player->Move(dir[dy+1][dx+1]);
}

void Game_Multiplayer::ResetRepeatingFlash() {
	frame_index = -1;
	last_flash_frame_index = -1;
	last_frame_flash.reset();
	repeating_flashes.clear();
}

static std::string get_room_url(int room_id, std::string_view session_token) {
	auto server_url = Web_API::GetSocketURL();
	std::string room_url = server_url + std::to_string(room_id);
	if (!session_token.empty()) {
		room_url.append("?token=");
		room_url.append(session_token);
	}
	return room_url;
}

void Game_Multiplayer::InitConnection() {
	/*
	conn.RegisterSystemHandler(YNOConnection::SystemMessage::OPEN, [] (Multiplayer::Connection& c) {
	});*/
	connection.RegisterSystemHandler(YNOConnection::SystemMessage::CLOSE, [this] (Multiplayer::Connection& c) {
		ResetRepeatingFlash();
		if (session_active) {
			Web_API::UpdateConnectionStatus(2); // connecting
			auto room_url = get_room_url(room_id, session_token);
			Output::Debug("Reconnecting: {}", room_url);
			c.Open(room_url);
		} else {
			Web_API::UpdateConnectionStatus(0); // disconnected
		}
	});
	using namespace YNO_Messages::S2C;
	connection.RegisterHandler<SyncPlayerDataPacket>("s", [this] (SyncPlayerDataPacket& p) {
		host_id = p.host_id;
		connection.SetKey(std::string(p.key));
		Web_API::UpdateConnectionStatus(1); // connected;
		auto& player = Main_Data::game_player;
		namespace C = YNO_Messages::C2S;
		// SendMainPlayerPos();
		connection.SendPacketAsync<C::MainPlayerPosPacket>(player->GetX(), player->GetY());
		// SendMainPlayerMoveSpeed(player->GetMoveSpeed());
		connection.SendPacketAsync<C::SpeedPacket>(player->GetMoveSpeed());
		// SendMainPlayerSprite(player->GetSpriteName(), player->GetSpriteIndex());
		connection.SendPacketAsync<C::SpritePacket>(player->GetSpriteName(),
					player->GetSpriteIndex());
		// SendMainPlayerName();
		Tone tone = Main_Data::game_screen->GetTone();
		connection.SendPacketAsync<C::TonePacket>(tone.red, tone.green, tone.blue, tone.gray);
		// if session_token is not empty, name shouldn't be sent
		if (session_token.empty() && !host_nickname.empty())
			connection.SendPacketAsync<C::NamePacket>(host_nickname);
		// SendSystemName(Main_Data::game_system->GetSystemName());
		auto sysn = Main_Data::game_system->GetSystemName();
		connection.SendPacketAsync<C::SysNamePacket>(ToString(sysn));
		Web_API::SyncPlayerData(p.uuid, p.rank, p.account_bin, p.badge);
	});
	connection.RegisterHandler<SyncSwitchPacket>("ss", [this] (SyncSwitchPacket& p) {
		int value_bin = (int) Main_Data::game_switches->GetInt(p.switch_id);
		if (p.sync_type != 1) {
			connection.SendPacketAsync<YNO_Messages::C2S::SyncSwitchPacket>(p.switch_id, value_bin);
		}
		if (p.sync_type >= 1) {
			sync_switches.push_back(p.switch_id);
		}
	});
	connection.RegisterHandler<SyncVariablePacket>("sv", [this] (SyncVariablePacket& p) {
		auto value = 0;
		switch (p.var_id) {
			case 10000:
				value = Main_Data::game_party->GetGold();
				break;
			case 10001:
				value = Main_Data::game_party->GetActor(0)->GetBaseMaxHp();
				break;
			default:
				value = (int) Main_Data::game_variables->Get(p.var_id);
				break;
		}
		if (p.sync_type != 1) {
			connection.SendPacketAsync<YNO_Messages::C2S::SyncVariablePacket>(p.var_id, value);
		}
		if (p.sync_type >= 1) {
			sync_vars.push_back(p.var_id);
		}
	});
	connection.RegisterHandler<SyncEventPacket>("sev", [this] (SyncEventPacket& p) {
		if (p.trigger_type != 1) {
			sync_events.push_back(p.event_id);
		}
		if (p.trigger_type >= 1) {
			sync_action_events.push_back(p.event_id);
		}
	});
	connection.RegisterHandler<BadgeUpdatePacket>("b", [] (BadgeUpdatePacket& p) {
		Web_API::OnRequestBadgeUpdate();
	});
	connection.RegisterHandler<GlobalChatPacket>("gsay", [] (GlobalChatPacket& p) {
		Web_API::SyncGlobalPlayerData(p.uuid, p.name, p.sys, p.rank, p.account_bin, p.badge);
		Web_API::OnGChatMessageReceived(p.uuid, p.map_id, p.prev_map_id,
				p.prev_locations, p.x, p.y, p.msg);
	});
	connection.RegisterHandler<PartyChatPacket>("psay", [] (PartyChatPacket& p) {
		Web_API::OnPChatMessageReceived(p.uuid, p.msg);
	});
	connection.RegisterHandler<ConnectPacket>("c", [this] (ConnectPacket& p) {
		if (p.id == host_id) return;
		if (players.find(p.id) == players.end()) SpawnOtherPlayer(p.id);
		Web_API::SyncPlayerData(p.uuid, p.rank, p.account_bin, p.badge, p.id);
	});
	connection.RegisterHandler<DisconnectPacket>("d", [this] (DisconnectPacket& p) {
		if (p.id == host_id) return;
		if (players.find(p.id) == players.end()) SpawnOtherPlayer(p.id);
		auto& player = players[p.id];
		if (player.chat_name) {
			auto scene_map = Scene::Find(Scene::SceneType::Map);
			if (scene_map == nullptr) {
				Output::Debug("unexpected");
				//return;
			}
			auto old_list = &DrawableMgr::GetLocalList();
			DrawableMgr::SetLocalList(&scene_map->GetDrawableList());
			player.chat_name.reset();
			DrawableMgr::SetLocalList(old_list);
		}
		dc_players.push_back(std::move(player));
		players.erase(p.id);
		repeating_flashes.erase(p.id);
		if (Main_Data::game_pictures) {
			Main_Data::game_pictures->EraseAllMultiplayerForPlayer(p.id);
		}

		Web_API::OnPlayerDisconnect(p.id);
	});
	connection.RegisterHandler<ChatPacket>("say", [this] (ChatPacket& p) {
		if (p.id == host_id) Web_API::OnChatMessageReceived(p.msg);
		else {
			if (players.find(p.id) == players.end()) SpawnOtherPlayer(p.id);
			Web_API::OnChatMessageReceived(p.msg, p.id);
		}
	});
	connection.RegisterHandler<MovePacket>("m", [this] (MovePacket& p) {
		if (p.id == host_id) return;
		if (players.find(p.id) == players.end()) SpawnOtherPlayer(p.id);
		auto& player = players[p.id];
		int x = Utils::Clamp(p.x, 0, Game_Map::GetWidth() - 1);
		int y = Utils::Clamp(p.y, 0, Game_Map::GetHeight() - 1);
		player.mvq.push(std::make_pair(x, y));
	});
	connection.RegisterHandler<FacingPacket>("f", [this] (FacingPacket& p) {
		if (p.id == host_id) return;
		if (players.find(p.id) == players.end()) SpawnOtherPlayer(p.id);
		auto& player = players[p.id];
		int facing = Utils::Clamp(p.facing, 0, 3);
		player.ch->SetFacing(facing);
	});
	connection.RegisterHandler<SpeedPacket>("spd", [this] (SpeedPacket& p) {
		if (p.id == host_id) return;
		if (players.find(p.id) == players.end()) SpawnOtherPlayer(p.id);
		auto& player = players[p.id];
		int speed = Utils::Clamp(p.speed, 1, 6);
		player.ch->SetMoveSpeed(speed);
	});
	connection.RegisterHandler<SpritePacket>("spr", [this] (SpritePacket& p) {
		if (p.id == host_id) return;
		if (players.find(p.id) == players.end()) SpawnOtherPlayer(p.id);
		auto& player = players[p.id];
		int idx = Utils::Clamp(p.index, 0, 7);
		player.ch->SetSpriteGraphic(std::string(p.name), idx);
		Web_API::OnPlayerSpriteUpdated(p.name, idx, p.id);
	});
	connection.RegisterHandler<FlashPacket>("fl", [this] (FlashPacket& p) {
		if (p.id == host_id) return;
		if (players.find(p.id) == players.end()) SpawnOtherPlayer(p.id);
		auto& player = players[p.id];
		player.ch->Flash(p.r, p.g, p.b, p.p, p.f);
	});
	connection.RegisterHandler<RepeatingFlashPacket>("rfl", [this] (RepeatingFlashPacket& p) {
		if (p.id == host_id) return;
		if (players.find(p.id) == players.end()) SpawnOtherPlayer(p.id);
		auto& player = players[p.id];
		auto flash_array = std::array<int, 5>{ p.r, p.g, p.b, p.p, p.f };
		repeating_flashes[p.id] = std::array<int, 5>(flash_array);
		player.ch->Flash(p.r, p.g, p.b, p.p, p.f);
	});
	connection.RegisterHandler<RemoveRepeatingFlashPacket>("rrfl", [this] (RemoveRepeatingFlashPacket& p) {
		if (p.id == host_id) return;
		if (players.find(p.id) == players.end()) SpawnOtherPlayer(p.id);
		repeating_flashes.erase(p.id);
	});
	connection.RegisterHandler<TonePacket>("t", [this] (TonePacket& p) {
		if (p.id == host_id) return;
		if (players.find(p.id) == players.end()) SpawnOtherPlayer(p.id);
		auto& player = players[p.id];
		player.sprite->SetTone(Tone(p.red, p.green, p.blue, p.gray));
	});
	connection.RegisterHandler<SystemPacket>("sys", [this] (SystemPacket& p) {
		if (p.id == host_id) return;
		if (players.find(p.id) == players.end()) SpawnOtherPlayer(p.id);
		auto& player = players[p.id];
		auto chat_name = player.chat_name.get();
		if (chat_name) {
			chat_name->SetSystemGraphic(std::string(p.name));
		}
		Web_API::OnPlayerSystemUpdated(p.name, p.id);
	});
	connection.RegisterHandler<SEPacket>("se", [this] (SEPacket& p) {
		if (p.id == host_id) return;
		if (players.find(p.id) == players.end()) SpawnOtherPlayer(p.id);

		if (mp_settings(Option::ENABLE_PLAYER_SOUNDS)) {
			auto& player = players[p.id];

			int px = Main_Data::game_player->GetX();
			int py = Main_Data::game_player->GetY();
			int ox = player.ch->GetX();
			int oy = player.ch->GetY();

			int hmw = Game_Map::GetWidth() / 2;
			int hmh = Game_Map::GetHeight() / 2;

			int rx;
			int ry;
			
			if (Game_Map::LoopHorizontal() && px - ox >= hmw) {
				rx = Game_Map::GetWidth() - (px - ox);
			} else if (Game_Map::LoopHorizontal() && px - ox < hmw * -1) {
				rx = Game_Map::GetWidth() + (px - ox);
			} else {
				rx = px - ox;
			}

			if (Game_Map::LoopVertical() && py - oy >= hmh) {
				ry = Game_Map::GetHeight() - (py - oy);
			} else if (Game_Map::LoopVertical() && py - oy < hmh * -1) {
				ry = Game_Map::GetHeight() + (py - oy);
			} else {
				ry = py - oy;
			}

			int dist = std::sqrt(rx * rx + ry * ry);
			float dist_volume = 75.0f - ((float)dist * 10.0f);
			float sound_volume_multiplier = float(p.snd.volume) / 100.0f;
			int real_volume = std::max((int)(dist_volume * sound_volume_multiplier), 0);

			lcf::rpg::Sound sound;
			sound.name = p.snd.name;
			sound.volume = real_volume;
			sound.tempo = p.snd.tempo;
			sound.balance = p.snd.balance;

			Main_Data::game_system->SePlay(sound);
		}
	});

	auto modify_args = [] (PicturePacket& pa) {
		if (Game_Map::LoopHorizontal()) {
			int alt_map_x = pa.map_x + Game_Map::GetWidth() * TILE_SIZE * TILE_SIZE;
			if (std::abs(pa.map_x - Game_Map::GetPositionX()) > std::abs(alt_map_x - Game_Map::GetPositionX())) {
				pa.map_x = alt_map_x;
			}
		}
		if (Game_Map::LoopVertical()) {
			int alt_map_y = pa.map_y + Game_Map::GetHeight() * TILE_SIZE * TILE_SIZE;
			if (std::abs(pa.map_y - Game_Map::GetPositionY()) > std::abs(alt_map_y - Game_Map::GetPositionY())) {
				pa.map_y = alt_map_y;
			}
		}
		pa.params.position_x += (int)(std::floor((pa.map_x / TILE_SIZE) - (pa.pan_x / (TILE_SIZE * 2))) - std::floor((Game_Map::GetPositionX() / TILE_SIZE) - Main_Data::game_player->GetPanX() / (TILE_SIZE * 2)));
		pa.params.position_y += (int)(std::floor((pa.map_y / TILE_SIZE) - (pa.pan_y / (TILE_SIZE * 2))) - std::floor((Game_Map::GetPositionY() / TILE_SIZE) - Main_Data::game_player->GetPanY() / (TILE_SIZE * 2)));
	};

	connection.RegisterHandler<ShowPicturePacket>("ap", [this, modify_args] (ShowPicturePacket& p) {
		if (p.id == host_id) return;
		if (players.find(p.id) == players.end()) SpawnOtherPlayer(p.id);
		modify_args(p);
		int pic_id = p.pic_id + (p.id + 1) * 50; //offset to avoid conflicting with others using the same picture
		Main_Data::game_pictures->Show(pic_id, p.params);
	});
	connection.RegisterHandler<MovePicturePacket>("mp", [this, modify_args] (MovePicturePacket& p) {
		if (p.id == host_id) return;
		if (players.find(p.id) == players.end()) SpawnOtherPlayer(p.id);
		int pic_id = p.pic_id + (p.id + 1) * 50; //offset to avoid conflicting with others using the same picture
		modify_args(p);
		Main_Data::game_pictures->Move(pic_id, p.params);
	});
	connection.RegisterHandler<ErasePicturePacket>("rp", [this] (ErasePicturePacket& p) {
		if (p.id == host_id) return;
		if (players.find(p.id) == players.end()) SpawnOtherPlayer(p.id);
		int pic_id = p.pic_id + (p.id + 1) * 50; //offset to avoid conflicting with others using the same picture
		Main_Data::game_pictures->Erase(pic_id);
	});
	connection.RegisterHandler<NamePacket>("name", [this] (NamePacket& p) {
		if (p.id == host_id) return;
		if (players.find(p.id) == players.end()) SpawnOtherPlayer(p.id);
		auto& player = players[p.id];
		auto scene_map = Scene::Find(Scene::SceneType::Map);
		if (scene_map == nullptr) {
			Output::Debug("unexpected");
			//return;
		}
		auto old_list = &DrawableMgr::GetLocalList();
		DrawableMgr::SetLocalList(&scene_map->GetDrawableList());
		player.chat_name = std::make_unique<ChatName>(p.id, player, std::string(p.name));
		DrawableMgr::SetLocalList(old_list);

		Web_API::OnPlayerNameUpdated(p.name, p.id);
	});
	connection.SetMonitor(&m_limiter);
}

//this will only be called from outside
using namespace YNO_Messages::C2S;
using Option = Game_Multiplayer::Option;
extern "C" {

void SendChatMessageToServer(const char* msg) {
	auto& i = Game_Multiplayer::Instance();
	if (i.host_nickname == "")
		return;
	i.connection.SendPacket(ChatPacket(msg));
}

void SendGChatMessageToServer(const char* msg) {
	auto& i = Game_Multiplayer::Instance();
	if (i.host_nickname == "") return;
	int enable_loc_bin = i.GetSettingFlags()(Option::ENABLE_GLOBAL_MESSAGE_LOCATION) ? 1 : 0;
	i.connection.SendPacket(GlobalChatPacket(msg, enable_loc_bin));
}

void SendPChatMessageToServer(const char* msg) {
	auto& i = Game_Multiplayer::Instance();
	if (i.host_nickname == "") return;
	i.connection.SendPacket(PartyChatPacket(msg));
}

void SendBanUserRequest(const char* uuid) {
	auto& i = Game_Multiplayer::Instance();
	i.connection.SendPacket(BanUserPacket(uuid));
}

void ChangeName(const char* name) {
	auto& i = Game_Multiplayer::Instance();
	if (i.host_nickname != "") return;
	i.host_nickname = name;
	if (i.session_token.empty())
		i.connection.SendPacketAsync<NamePacket>(i.host_nickname);
}

void SetGameLanguage(const char* lang) {
	Player::translation.SelectLanguage(lang);
}

void ToggleSinglePlayer() {
	auto& i = Game_Multiplayer::Instance();
	i.GetSettingFlags().Toggle(Option::SINGLE_PLAYER);
	if (i.GetSettingFlags()(Option::SINGLE_PLAYER)) {
		i.Quit();
		Web_API::UpdateConnectionStatus(3); // single
	} else {
		i.Connect(i.room_id);
	}
	Web_API::ReceiveInputFeedback(1);
}

void ToggleNametags() {
	Game_Multiplayer::Instance().GetSettingFlags().Toggle(Option::ENABLE_NICKS);
	Web_API::ReceiveInputFeedback(2);
}

void TogglePlayerSounds() {
	Game_Multiplayer::Instance().GetSettingFlags().Toggle(Option::ENABLE_PLAYER_SOUNDS);
	Web_API::ReceiveInputFeedback(3);
}

void ToggleGlobalMessageLocation() {
	Game_Multiplayer::Instance().GetSettingFlags().Toggle(Option::ENABLE_GLOBAL_MESSAGE_LOCATION);
	Web_API::ReceiveInputFeedback(4);
}

void ToggleFloodDefender() {
	auto& i = Game_Multiplayer::Instance();
	i.GetSettingFlags().Toggle(Option::ENABLE_FLOOD_DEFENDER);
	if (i.GetSettingFlags()(Option::ENABLE_FLOOD_DEFENDER)) {
		i.connection.SetMonitor(&i.m_limiter);
	} else {
		i.connection.SetMonitor(nullptr);
	}
	Web_API::ReceiveInputFeedback(5);
}

void SetSessionToken(const char* t) {
	auto& i = Game_Multiplayer::Instance();
	i.session_token.assign(t);
}

}

void Game_Multiplayer::Connect(int map_id) {
	room_id = map_id;
	if (mp_settings(Option::SINGLE_PLAYER)) return;
	Game_Multiplayer::Quit();
	session_active = true;
	dc_players.clear();
	Web_API::UpdateConnectionStatus(2); // connecting
	connection.Open(get_room_url(map_id, session_token));
}

void Game_Multiplayer::Quit() {
	Web_API::UpdateConnectionStatus(0); // disconnected
	session_active = false;
	connection.Close();
	players.clear();
	sync_switches.clear();
	sync_vars.clear();
	sync_events.clear();
	sync_action_events.clear();
	ResetRepeatingFlash();
	if (Main_Data::game_pictures) {
		Main_Data::game_pictures->EraseAllMultiplayer();
	}
}

void Game_Multiplayer::MainPlayerMoved(int dir) {
	auto& p = Main_Data::game_player;
	connection.SendPacketAsync<MainPlayerPosPacket>(p->GetX(), p->GetY());
}

void Game_Multiplayer::MainPlayerFacingChanged(int dir) {
	connection.SendPacketAsync<FacingPacket>(dir);
}

void Game_Multiplayer::MainPlayerChangedMoveSpeed(int spd) {
	connection.SendPacketAsync<SpeedPacket>(spd);
}

void Game_Multiplayer::MainPlayerChangedSpriteGraphic(std::string name, int index) {
	connection.SendPacketAsync<SpritePacket>(name, index);
	Web_API::OnPlayerSpriteUpdated(name, index);
}

void Game_Multiplayer::MainPlayerFlashed(int r, int g, int b, int p, int f) {
	std::array<int, 5> flash_array = std::array<int, 5>{ r, g, b, p, f };
	if (last_flash_frame_index == frame_index - 1 && (last_frame_flash.get() == nullptr || *last_frame_flash == flash_array)) {
		if (last_frame_flash.get() == nullptr) {
			last_frame_flash = std::make_unique<std::array<int, 5>>(flash_array);
			connection.SendPacketAsync<RepeatingFlashPacket>(r, g, b, p, f);
		}
	} else {
		connection.SendPacketAsync<FlashPacket>(r, g, b, p, f);
		last_frame_flash.reset();
	}
	last_flash_frame_index = frame_index;
}

void Game_Multiplayer::MainPlayerChangedTone(Tone tone) {
	connection.SendPacketAsync<TonePacket>(tone.red, tone.green, tone.blue, tone.gray);
}

void Game_Multiplayer::MainPlayerTeleported(int map_id, int x, int y) {
	connection.SendPacketAsync<TeleportPacket>(x, y);
	Web_API::OnPlayerTeleported(map_id, x, y);
}

void Game_Multiplayer::MainPlayerTriggeredEvent(int event_id, bool action) {
	if (action) {
		if (std::find(sync_action_events.begin(), sync_action_events.end(), event_id) != sync_action_events.end()) {
			connection.SendPacketAsync<YNO_Messages::C2S::SyncEventPacket>(event_id, 1);
		}
	} else {
		if (std::find(sync_events.begin(), sync_events.end(), event_id) != sync_events.end()) {
			connection.SendPacketAsync<YNO_Messages::C2S::SyncEventPacket>(event_id, 0);
		}
	}
}

void Game_Multiplayer::SystemGraphicChanged(StringView sys) {
	connection.SendPacketAsync<SysNamePacket>(ToString(sys));
	Web_API::OnUpdateSystemGraphic(ToString(sys));
}

void Game_Multiplayer::SePlayed(lcf::rpg::Sound& sound) {
	if (!Main_Data::game_player->IsMenuCalling()) {
		connection.SendPacketAsync<SEPacket>(sound);
	}
}

void Game_Multiplayer::PictureShown(int pic_id, Game_Pictures::ShowParams& params) {
	auto& p = Main_Data::game_player;
	connection.SendPacketAsync<ShowPicturePacket>(pic_id, params,
		Game_Map::GetPositionX(), Game_Map::GetPositionY(),
		p->GetPanX(), p->GetPanY());
}

void Game_Multiplayer::PictureMoved(int pic_id, Game_Pictures::MoveParams& params) {
	auto& p = Main_Data::game_player;
	connection.SendPacketAsync<MovePicturePacket>(pic_id, params,
		Game_Map::GetPositionX(), Game_Map::GetPositionY(),
		p->GetPanX(), p->GetPanY());
}

void Game_Multiplayer::PictureErased(int pic_id) {
	connection.SendPacketAsync<ErasePicturePacket>(pic_id);
}

void Game_Multiplayer::ApplyFlash(int r, int g, int b, int power, int frames) {
	for (auto& p : players) {
		p.second.ch->Flash(r, g, b, power, frames);
	}
}

void Game_Multiplayer::ApplyRepeatingFlashes() {
	for (auto& rf : repeating_flashes) {
		if (players.find(rf.first) != players.end()) {
			std::array<int, 5> flash_array = rf.second;
			players[rf.first].ch->Flash(flash_array[0], flash_array[1], flash_array[2], flash_array[3], flash_array[4]);
		}
	}
}

void Game_Multiplayer::ApplyTone(Tone tone) {
	for (auto& p : players) {
		p.second.sprite->SetTone(tone);
	}
}

void Game_Multiplayer::SwitchSet(int switch_id, int value_bin) {
	if (std::find(sync_switches.begin(), sync_switches.end(), switch_id) != sync_switches.end()) {
		connection.SendPacketAsync<YNO_Messages::C2S::SyncSwitchPacket>(switch_id, value_bin);
	}
}

void Game_Multiplayer::VariableSet(int var_id, int value) {
	if (std::find(sync_vars.begin(), sync_vars.end(), var_id) != sync_vars.end()) {
		connection.SendPacketAsync<YNO_Messages::C2S::SyncVariablePacket>(var_id, value);
	}
}

void Game_Multiplayer::ApplyScreenTone() {
	ApplyTone(Main_Data::game_screen->GetTone());
}

void Game_Multiplayer::Update() {
	if (session_active) {
		if (last_flash_frame_index > -1 && frame_index > last_flash_frame_index) {
			connection.SendPacketAsync<RemoveRepeatingFlashPacket>();
			last_flash_frame_index = -1;
			last_frame_flash.reset();
		}

		frame_index++;

		for (auto& p : players) {
			auto& q = p.second.mvq;
			auto& ch = p.second.ch;
			if (!q.empty() && ch->IsStopping()) {
				MovePlayerToPos(ch, q.front().first, q.front().second);
				q.pop();
				if (!ch->IsMultiplayerVisible()) {
					ch->SetMultiplayerVisible(true);
				}
			}
			if (ch->IsMultiplayerVisible() && ch->GetBaseOpacity() < 32) {
				ch->SetBaseOpacity(ch->GetBaseOpacity() + 1);
			}
			ch->SetProcessed(false);
			ch->Update();
			p.second.sprite->Update();
		}
	}

	if (!dc_players.empty()) {
		auto scene_map = Scene::Find(Scene::SceneType::Map);
		if (scene_map == nullptr) {
			Output::Debug("unexpected");
			return;
		}

		auto old_list = &DrawableMgr::GetLocalList();
		DrawableMgr::SetLocalList(&scene_map->GetDrawableList());
		
		for (auto dcpi = dc_players.rbegin(); dcpi != dc_players.rend(); dcpi++) {
			auto& ch = (*dcpi).ch;
			if (ch->GetBaseOpacity() > 0) {
				ch->SetBaseOpacity(ch->GetBaseOpacity() - 1);
				ch->SetProcessed(false);
				ch->Update();
				(*dcpi).sprite->Update();
			} else {
				dc_players.erase(dcpi.base() - 1);
			}
		}

		DrawableMgr::SetLocalList(old_list);
	}

	if (session_active)
		connection.FlushQueue();
}

