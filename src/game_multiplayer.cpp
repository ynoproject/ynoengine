#include <map>
#include <memory>
#include <queue>
#include <charconv>
#include <utility>
#include <bitset>

#include "game_multiplayer.h"
#include "output.h"
#include "game_player.h"
#include "game_playerother.h"
#include "sprite_character.h"
#include "window_base.h"
#include "drawable_mgr.h"
#include "scene.h"
#include "bitmap.h"
#include "font.h"
#include "input.h"
#include "game_map.h"
#include "game_system.h"
#include "game_screen.h"
#include "cache.h"
#include "chatname.h"
#include "web_api.h"
#include "yno_connection.h"
#include "yno_messages.h"

using Game_Multiplayer::Option;

namespace {
	YNOConnection initialize_connection();

	Game_Multiplayer::SettingFlags mp_settings;
	YNOConnection connection = initialize_connection();
	bool session_active = false; //if true, it will automatically reconnect when disconnected
	int host_id = -1;
	int room_id = -1;
	std::string host_nickname = "";
	std::map<int, PlayerOther> players;
	std::vector<PlayerOther> dc_players;

	void SpawnOtherPlayer(int id) {
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
	void MovePlayerToPos(std::unique_ptr<Game_PlayerOther> &player, int x, int y) {
		if (!player->IsStopping()) {
			Output::Debug("MovePlayerToPos unexpected error: the player is busy being animated");
		}
		int dx = x - player->GetX();
		int dy = y - player->GetY();
		if (abs(dx) > 1 || abs(dy) > 1 || dx == 0 && dy == 0 || !player->IsMultiplayerVisible()) {
			player->SetX(x);
			player->SetY(y);
			return;
		}
		int dir[3][3] = {{Game_Character::Direction::UpLeft, Game_Character::Direction::Up, Game_Character::Direction::UpRight},
						 {Game_Character::Direction::Left, 0, Game_Character::Direction::Right},
						 {Game_Character::Direction::DownLeft, Game_Character::Direction::Down, Game_Character::Direction::DownRight}};
		player->Move(dir[dy+1][dx+1]);
	}

	std::string get_room_url(int room_id) {
		auto server_url = Web_API::GetSocketURL();
		std::string room_url = server_url + std::to_string(room_id);
		return room_url;
	}

	YNOConnection initialize_connection() {
		using namespace YNO_Messages::C2S;
		YNOConnection conn;
		conn.RegisterSystemHandler(YNOConnection::SystemMessage::OPEN, [] (MultiplayerConnection& c) {
			Web_API::UpdateConnectionStatus(1); // connected;
			session_active = true;
			auto& player = Main_Data::game_player;
			// SendMainPlayerPos();
			c.SendPacketAsync(MainPlayerPosPacket(player->GetX(), player->GetY()));
			// SendMainPlayerMoveSpeed(player->GetMoveSpeed());
			c.SendPacketAsync(SpeedPacket(player->GetMoveSpeed()));
			// SendMainPlayerSprite(player->GetSpriteName(), player->GetSpriteIndex());
			c.SendPacketAsync(SpritePacket(player->GetSpriteName(),
						player->GetSpriteIndex()));
			// SendMainPlayerName();
			if (!host_nickname.empty())
				c.SendPacketAsync(NamePacket(host_nickname));
			// SendSystemName(Main_Data::game_system->GetSystemName());
			auto sysn = Main_Data::game_system->GetSystemName();
			c.SendPacketAsync(SysNamePacket(ToString(sysn)));
		});
		conn.RegisterSystemHandler(YNOConnection::SystemMessage::CLOSE, [] (MultiplayerConnection& c) {
			if (session_active) {
				Web_API::UpdateConnectionStatus(2); // connecting
				auto room_url = get_room_url(room_id);
				Output::Debug("Reconnecting: {}", room_url);
				c.Open(room_url);
			} else {
				Web_API::UpdateConnectionStatus(0); // disconnected
			}
		});
		using PL = MultiplayerConnection::ParameterList;
		conn.RegisterUnconditionalHandler([] (std::string_view name, const PL& v) {
			using A = MultiplayerConnection::Action;
			auto to_int = [&](const auto& str, int& num) {
				int out {};
				auto [ptr, ec] { std::from_chars(str.data(), str.data() + str.size(), out) };
				if (ec == std::errc()) {
					num = out;
					return true;
				}
				return false;
			};

			if (v.empty()) { //no valid commands smaller than 2 segments
				return A::STOP;
			}

			//Output::Debug("msg flagsize {}", v.size());
			if (name == "s") { //set your id command //we need to get our id first otherwise we dont know what commands are us
				if (v.size() < 2) {
					return A::STOP;
				}

				if (!to_int(v[0], host_id)) {
					return A::STOP;
				}

				connection.SetKey(std::string(v[1]));
			}
			else if (name == "say") { //this isn't sent with an id so we do it here
				if (v.size() < 2) {
					return A::STOP;
				}
				Web_API::OnChatMessageReceived(v[0], v[1]);
			}
			else if (name == "gsay") { //support for global messages
				if (v.size() < 5) {
					return A::STOP;
				}
				
				Web_API::OnGChatMessageReceived(v[0], v[1], v[2], v[3], v[4]);
			}
			else { //these are all for actions of other players, they have an id
				int id = 0;
				if (!to_int(v[0], id)) {
					return A::STOP;
				}
				if (id != host_id) { //if the command isn't us
					if (players.count(id) == 0) { //if this is a command for a player we don't know of, spawn him
						SpawnOtherPlayer(id);
					}
					
					PlayerOther& player = players[id];

					if (name == "d") { //disconnect command
						if (v.size() < 1) {
							return A::STOP;
						}
						
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
						players.erase(id);
						if (Main_Data::game_pictures) {
							Main_Data::game_pictures->EraseAllMultiplayerForPlayer(id);
						}

						Web_API::OnPlayerDisconnect(id);
					}
					else if (name == "m") { //move command
						if (v.size() < 3) {
							return A::STOP;
						}

						int x = 0;
						int y = 0;

						if (!to_int(v[1], x)) {
							return A::STOP;
						}
						x = Utils::Clamp(x, 0, Game_Map::GetWidth() - 1);

						if (!to_int(v[2], y)) {
							return A::STOP;
						}
						y = Utils::Clamp(y, 0, Game_Map::GetHeight() - 1);

						player.mvq.push(std::make_pair(x, y));
					}
					else if (name == "f") { //facing command
						if (v.size() < 2) {
							return A::STOP;
						}

						int facing = 0;

						if (!to_int(v[1], facing)) {
							return A::STOP;
						}
						facing = Utils::Clamp(facing, 0, 3);
						
						player.ch->SetFacing(facing);
					}
					else if (name == "spd") { //change move speed command
						if (v.size() < 2) {
							return A::STOP;
						}

						int speed = 0;
						if (!to_int(v[1], speed)) {
							return A::STOP;
						}
						speed = Utils::Clamp(speed, 1, 6);

						player.ch->SetMoveSpeed(speed);
					}
					else if (name == "spr") { //change sprite command
						if (v.size() < 3) {
							return A::STOP;
						}

						int idx = 0;
						if (!to_int(v[2], idx)) {
							return A::STOP;
						}
						idx = Utils::Clamp(idx, 0, 7);

						player.ch->SetSpriteGraphic(std::string(v[1]), idx);

						Web_API::OnPlayerSpriteUpdated(v[1], idx, id);
					}
					else if (name == "sys") { //change system graphic
						if (v.size() < 2) {
							return A::STOP;
						}

						auto chat_name = player.chat_name.get();
						if (chat_name) {
							chat_name->SetSystemGraphic(std::string(v[1]));
						}

						Web_API::OnPlayerSystemUpdated(v[1], id);
					}
					else if (name == "se") { //play sound effect
						if (v.size() < 5) {
							return A::STOP;
						}
						
						if (mp_settings(Option::ENABLE_PLAYER_SOUNDS)) {
							int volume = 0;
							int tempo = 0;
							int balance = 0;

							if (!to_int(v[2], volume) || !to_int(v[3], tempo) || !to_int(v[4], balance)) {
								return A::STOP;
							}

							int px = Main_Data::game_player->GetX();
							int py = Main_Data::game_player->GetY();
							int ox = player.ch->GetX();
							int oy = player.ch->GetY();

							int hmw = Game_Map::GetWidth() / 2;
							int hmh = Game_Map::GetHeight() / 2;

							int rx;
							int ry;
							
							if (Game_Map::LoopHorizontal() && (px < hmw) != (ox < hmw)) {
								rx = px - (Game_Map::GetWidth() - 1) - ox;
							} else {
								rx = px - ox;
							}

							if (Game_Map::LoopVertical() && (py < hmh) != (oy < hmh)) {
								ry = py - (Game_Map::GetHeight() - 1) - oy;
							} else {
								ry = py - oy;
							}

							int dist = std::sqrt(rx * rx + ry * ry);
							float dist_volume = 75.0f - ((float)dist * 10.0f);
							float sound_volume_multiplier = float(volume) / 100.0f;
							int real_volume = std::max((int)(dist_volume * sound_volume_multiplier), 0);

							lcf::rpg::Sound sound;
							sound.name = v[1];
							sound.volume = real_volume;
							sound.tempo = tempo;
							sound.balance = balance;

							Main_Data::game_system->SePlay(sound);
						}
					}
					else if (name == "ap" || name == "mp") { //show or move picture
						bool isShow = name == "ap";
						size_t expectedSize = 18;

						if (isShow) {
							expectedSize += 2;
						}

						if (v.size() < expectedSize) {
							return A::STOP;
						}

						int pic_id = 0;
						if (!to_int(v[1], pic_id)) {
							return A::STOP;
						}

						pic_id += (id + 1) * 50; //offset to avoid conflicting with others using the same picture

						int position_x = 0;
						int position_y = 0;
						int map_x = 0;
						int map_y = 0;
						int pan_x = 0;
						int pan_y = 0;

						if (!to_int(v[2], position_x) || !to_int(v[3], position_y) || !to_int(v[4], map_x) || !to_int(v[5], map_y) | !to_int(v[6], pan_x) || !to_int(v[7], pan_y)) {
							return A::STOP;
						}

						if (Game_Map::LoopHorizontal()) {
							int alt_map_x = map_x + Game_Map::GetWidth() * TILE_SIZE * TILE_SIZE;

							if (std::abs(map_x - Game_Map::GetPositionX()) > std::abs(alt_map_x - Game_Map::GetPositionX())) {
								map_x = alt_map_x;
							}
						}

						if (Game_Map::LoopVertical()) {
							int alt_map_y = map_y + Game_Map::GetHeight() * TILE_SIZE * TILE_SIZE;

							if (std::abs(map_y - Game_Map::GetPositionY()) > std::abs(alt_map_y - Game_Map::GetPositionY())) {
								map_y = alt_map_y;
							}
						}
						
						position_x += (int)(std::floor((map_x / TILE_SIZE) - (pan_x / (TILE_SIZE * 2))) - std::floor((Game_Map::GetPositionX() / TILE_SIZE) - Main_Data::game_player->GetPanX() / (TILE_SIZE * 2)));
						position_y += (int)(std::floor((map_y / TILE_SIZE) - (pan_y / (TILE_SIZE * 2))) - std::floor((Game_Map::GetPositionY() / TILE_SIZE) - Main_Data::game_player->GetPanY() / (TILE_SIZE * 2)));

						int magnify = 100;
						int top_trans = 0;
						int bottom_trans = 0;
						int red = 100;
						int green = 100;
						int blue = 100;
						int saturation = 100;
						int effect_mode = 0;
						int effect_power = 0;

						to_int(v[8], magnify);
						to_int(v[9], top_trans);
						to_int(v[10], bottom_trans);
						to_int(v[11], red);
						to_int(v[12], green);
						to_int(v[13], blue);
						to_int(v[14], saturation);
						to_int(v[15], effect_mode);
						to_int(v[16], effect_power);

						if (isShow) {
							int use_transparent_color_bin = 0;
							int fixed_to_map_bin = 0;

							to_int(v[18], use_transparent_color_bin);
							to_int(v[19], fixed_to_map_bin);

							Game_Pictures::ShowParams params;

							params.position_x = position_x;
							params.position_y = position_y;
							params.magnify = magnify;
							params.top_trans = top_trans;
							params.bottom_trans = bottom_trans;
							params.red = red;
							params.green = green;
							params.blue = blue;
							params.saturation = saturation;
							params.effect_mode = effect_mode;
							params.effect_power = effect_power;
							params.name = v[17];
							params.use_transparent_color = use_transparent_color_bin ? true : false;
							params.fixed_to_map = fixed_to_map_bin ? true : false;

							Main_Data::game_pictures->Show(pic_id, params);
						} else {
							int duration = 0;

							to_int(v[17], duration);

							Game_Pictures::MoveParams params;

							params.position_x = position_x;
							params.position_y = position_y;
							params.magnify = magnify;
							params.top_trans = top_trans;
							params.bottom_trans = bottom_trans;
							params.red = red;
							params.green = green;
							params.blue = blue;
							params.saturation = saturation;
							params.effect_mode = effect_mode;
							params.effect_power = effect_power;
							params.duration = duration;

							Main_Data::game_pictures->Move(pic_id, params);
						}
					}
					else if (name == "rp") { //erase picture
						if (v.size() < 2) {
							return A::STOP;
						}

						int pic_id = 0;
						if (!to_int(v[1], pic_id)) {
							return A::STOP;
						}

						pic_id += (id + 1) * 50; //offset to avoid conflicting with others using the same picture

						Main_Data::game_pictures->Erase(pic_id);
					}
					else if (name == "name") { //set nickname (and optionally change system graphic)
						if (v.size() < 2) {
							return A::STOP;
						}
						auto scene_map = Scene::Find(Scene::SceneType::Map);
						if (scene_map == nullptr) {
							Output::Debug("unexpected");
							//return;
						}
						auto old_list = &DrawableMgr::GetLocalList();
						DrawableMgr::SetLocalList(&scene_map->GetDrawableList());
						player.chat_name = std::make_unique<ChatName>(id, player, std::string(v[1]));
						DrawableMgr::SetLocalList(old_list);

						Web_API::OnPlayerNameUpdated(v[1], id);
					} else {
						return A::PASS;
					}
				}
			}
			return A::PASS;
		});
		return conn;
	}

}

//this will only be called from outside
using namespace YNO_Messages::C2S;
extern "C" {

void SendChatMessageToServer(const char* sys, const char* msg) {
	if (host_nickname == "")
		return;
	connection.SendPacket(ChatPacket(sys, msg));
}

void SendGChatMessageToServer(const char* map_id, const char* prev_map_id, const char* prev_locations, const char* sys, const char* msg) {
	if (host_nickname == "") return;
	connection.SendPacket(GlobalChatPacket(map_id, prev_map_id, prev_locations, sys, msg));
}

void ChangeName(const char* name) {
	if (host_nickname != "") return;
	host_nickname = name;
	connection.SendPacketAsync(NamePacket(host_nickname));
}

void ToggleSinglePlayer() {
	mp_settings.Toggle(Option::SINGLE_PLAYER);
	if (mp_settings(Option::SINGLE_PLAYER)) {
		Game_Multiplayer::Quit();
		Web_API::UpdateConnectionStatus(3); // single
	} else {
		Game_Multiplayer::Connect(room_id);
	}
	Web_API::ReceiveInputFeedback(1); // connected
}

void ToggleNametags() {
	mp_settings.Toggle(Option::ENABLE_NICKS);
	Web_API::ReceiveInputFeedback(2); // connected
}

void TogglePlayerSounds() {
	mp_settings.Toggle(Option::ENABLE_PLAYER_SOUNDS);
	Web_API::ReceiveInputFeedback(3); // connected
}

}

void Game_Multiplayer::Connect(int map_id) {
	room_id = map_id;
	if (mp_settings(Option::SINGLE_PLAYER)) return;
	Game_Multiplayer::Quit();
	Web_API::UpdateConnectionStatus(2); // connecting
	connection.Open(get_room_url(map_id));
}

void Game_Multiplayer::Quit() {
	Web_API::UpdateConnectionStatus(0); // disconnected
	session_active = false;
	connection.Close();
	players.clear();
	dc_players.clear();
	if (Main_Data::game_pictures) {
		Main_Data::game_pictures->EraseAllMultiplayer();
	}
}

void Game_Multiplayer::MainPlayerMoved(int dir) {
	auto& p = Main_Data::game_player;
	connection.SendPacketAsync(MainPlayerPosPacket(p->GetX(), p->GetY()));
}

void Game_Multiplayer::MainPlayerFacingChanged(int dir) {
	connection.SendPacketAsync(FacingPacket(dir));
}

void Game_Multiplayer::MainPlayerChangedMoveSpeed(int spd) {
	connection.SendPacketAsync(SpeedPacket(spd));
}

void Game_Multiplayer::MainPlayerChangedSpriteGraphic(std::string name, int index) {
	connection.SendPacketAsync(SpritePacket(name, index));
	Web_API::OnPlayerSpriteUpdated(name, index);
}

void Game_Multiplayer::SystemGraphicChanged(StringView sys) {
	connection.SendPacketAsync(SysNamePacket(ToString(sys)));
	Web_API::OnUpdateSystemGraphic(ToString(sys));
}

void Game_Multiplayer::SePlayed(lcf::rpg::Sound& sound) {
	if (!Main_Data::game_player->IsMenuCalling()) {
		connection.SendPacketAsync(SEPacket(sound));
	}
}

void Game_Multiplayer::PictureShown(int pic_id, Game_Pictures::ShowParams& params) {
	auto& p = Main_Data::game_player;
	connection.SendPacketAsync(ShowPicturePacket(pic_id, params,
		Game_Map::GetPositionX(), Game_Map::GetPositionY(),
		p->GetPanX(), p->GetPanY()));
}

void Game_Multiplayer::PictureMoved(int pic_id, Game_Pictures::MoveParams& params) {
	auto& p = Main_Data::game_player;
	connection.SendPacketAsync(MovePicturePacket(pic_id, params,
		Game_Map::GetPositionX(), Game_Map::GetPositionY(),
		p->GetPanX(), p->GetPanY()));
}

void Game_Multiplayer::PictureErased(int pic_id) {
	connection.SendPacketAsync(ErasePicturePacket(pic_id));
}

void Game_Multiplayer::ApplyFlash(int r, int g, int b, int power, int frames) {
for (auto& p : players) {
		p.second.ch->Flash(r, g, b, power, frames);
	}
}

void Game_Multiplayer::ApplyTone(Tone tone) {
	for (auto& p : players) {
		p.second.sprite->SetTone(tone);
	}
}

void Game_Multiplayer::ApplyScreenTone() {
	ApplyTone(Main_Data::game_screen->GetTone());
}

Game_Multiplayer::SettingFlags& Game_Multiplayer::GetSettingFlags() { return mp_settings; }

void Game_Multiplayer::Update() {
	if (mp_settings(Option::SINGLE_PLAYER)) return;

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

	if (!dc_players.empty()) {
		auto scene_map = Scene::Find(Scene::SceneType::Map);
		if (scene_map == nullptr) {
			Output::Debug("unexpected");
			return;
		}

		auto old_list = &DrawableMgr::GetLocalList();
		DrawableMgr::SetLocalList(&scene_map->GetDrawableList());
		
		auto dcpi = dc_players.rbegin();
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

	connection.FlushQueue();
}

