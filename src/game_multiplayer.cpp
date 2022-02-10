#include <map>
#include <memory>
#include <queue>
#include <emscripten/emscripten.h>
#include <emscripten/websocket.h>
#include <charconv>
#include <regex>
#include <utility>

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
#include "TinySHA1.hpp"
#include "chatname.h"
#include "web_api.h"

using Game_Multiplayer::Option;

namespace {
	Game_Multiplayer::SettingFlags mp_settings;
	EMSCRIPTEN_WEBSOCKET_T socket;
	bool connected = false;
	bool session_active = false; //if true, it will automatically reconnect when disconnected
	int host_id = -1;
	int room_id = -1;
	int msg_count = 0;
	std::string host_nickname = "";
	std::string key = "";
	std::map<int, PlayerOther> players;
	std::vector<PlayerOther> dc_players;
	std::queue<std::string> message_queue;
	const std::string param_delim = "\uffff";
	const std::string message_delim = "\ufffe";
	const std::string secret = "";

	void TrySend(std::string msg) {
		if (!connected) return;
		unsigned short ready;
		emscripten_websocket_get_ready_state(socket, &ready);
		if (ready == 1) { //1 means OPEN
			sha1::SHA1 checksum;
			std::string header;
			uint32_t digest[5];
			char counter[7];
			char signature[9];

			msg_count = msg_count + 1; //increment message count
			snprintf(counter, 7, "%06d", msg_count); //format message count

			std::string hashmsg = key + secret + counter + msg; //construct string for us to hash

			checksum.processBytes(hashmsg.data(), hashmsg.size());
			checksum.getDigest(digest);
			snprintf(signature, 9, "%08x", digest[0]);

			header = signature;
			header += counter;

			std::string sendmsg = header + msg; //signature(8), counter(6), message(any)

			emscripten_websocket_send_binary(socket, (void*)sendmsg.c_str(), sendmsg.length()); //send signed message
		}
	}

	void QueueMessage(std::string msg) {
		message_queue.push(msg);
	}

	std::string SanitizeParameter(std::string param) {
		return std::regex_replace(std::regex_replace(param, std::regex(param_delim), ""), std::regex(message_delim), "");
	}

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

	void SendMainPlayerPos() {
		auto& player = Main_Data::game_player;
		std::string msg = "m" + param_delim + std::to_string(player->GetX()) + param_delim + std::to_string(player->GetY());
		QueueMessage(msg);
	}

	void SendMainPlayerFacing(int dir) {
		std::string msg = "f" + param_delim + std::to_string(dir);
		QueueMessage(msg);
	}

	void SendMainPlayerMoveSpeed(int spd) {
		std::string msg = "spd" + param_delim + std::to_string(spd);
		QueueMessage(msg);
	}

	void SendMainPlayerSprite(std::string name, int index) {
		std::string msg = "spr" + param_delim + SanitizeParameter(name) + param_delim + std::to_string(index);
		QueueMessage(msg);
	}

	void SendMainPlayerName() {
		if (host_nickname == "") return;
		std::string msg = "name" + param_delim + SanitizeParameter(host_nickname);
		QueueMessage(msg);
	}

	void SendSystemName(StringView sys) {
		std::string msg = "sys" + param_delim + SanitizeParameter(ToString(sys));
		QueueMessage(msg);
	}

	void SendSe(lcf::rpg::Sound& sound) {
		std::string msg = "se" + param_delim + SanitizeParameter(sound.name) + param_delim + std::to_string(sound.volume) + param_delim + std::to_string(sound.tempo) + param_delim + std::to_string(sound.balance);
		QueueMessage(msg);
	}

	void SendShowPicture(int pic_id, Game_Pictures::ShowParams& params) {
		std::string msg = "ap" + param_delim + std::to_string(pic_id) + param_delim + std::to_string(params.position_x) + param_delim + std::to_string(params.position_y)
			+ param_delim + std::to_string(Game_Map::GetPositionX()) + param_delim + std::to_string(Game_Map::GetPositionY())
			+ param_delim + std::to_string(Main_Data::game_player->GetPanX())	+ param_delim + std::to_string(Main_Data::game_player->GetPanY())
			+ param_delim + std::to_string(params.magnify) + param_delim + std::to_string(params.top_trans) + param_delim + std::to_string(params.bottom_trans) + param_delim
			+ std::to_string(params.red) + param_delim + std::to_string(params.green) + param_delim + std::to_string(params.blue) + param_delim + std::to_string(params.saturation)
			+ param_delim + std::to_string(params.effect_mode) + param_delim + std::to_string(params.effect_power) + param_delim + SanitizeParameter(params.name)
			+ param_delim + std::to_string(int(params.use_transparent_color)) + param_delim + std::to_string(int(params.fixed_to_map));
		QueueMessage(msg);
	}

	void SendMovePicture(int pic_id, Game_Pictures::MoveParams& params) {
		std::string msg = "mp" + param_delim + std::to_string(pic_id) + param_delim + std::to_string(params.position_x) + param_delim + std::to_string(params.position_y)
			+ param_delim + std::to_string(Game_Map::GetPositionX()) + param_delim + std::to_string(Game_Map::GetPositionY())
			+ param_delim + std::to_string(Main_Data::game_player->GetPanX())	+ param_delim + std::to_string(Main_Data::game_player->GetPanY())
			+ param_delim + std::to_string(params.magnify) + param_delim + std::to_string(params.top_trans) + param_delim + std::to_string(params.bottom_trans) + param_delim
			+ std::to_string(params.red) + param_delim + std::to_string(params.green) + param_delim + std::to_string(params.blue) + param_delim + std::to_string(params.saturation)
			+ param_delim + std::to_string(params.effect_mode) + param_delim + std::to_string(params.effect_power) + param_delim + std::to_string(params.duration);
		QueueMessage(msg);
	}

	void SendErasePicture(int pic_id) {
		std::string msg = "rp" + param_delim + std::to_string(pic_id);
		QueueMessage(msg);
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

	void init_socket(const std::string& url);
	std::string get_room_url(int room_id);

	EM_BOOL onopen(int eventType, const EmscriptenWebSocketOpenEvent *websocketEvent, void *userData) {
		Web_API::UpdateConnectionStatus(1); // connected
		//puts("onopen");
		session_active = true;
		connected = true;
		auto& player = Main_Data::game_player;
		SendMainPlayerPos();
		SendMainPlayerMoveSpeed(player->GetMoveSpeed());
		SendMainPlayerSprite(player->GetSpriteName(), player->GetSpriteIndex());
		SendMainPlayerName();
		SendSystemName(Main_Data::game_system->GetSystemName());
		return EM_TRUE;
	}
	EM_BOOL onclose(int eventType, const EmscriptenWebSocketCloseEvent *websocketEvent, void *userData) {
		//puts("onclose");
		connected = false;
		if (session_active) {
			Web_API::UpdateConnectionStatus(2); // connecting
			auto room_url = get_room_url(room_id);
			Output::Debug("Reconnecting: {}", room_url);
			init_socket(room_url);
		} else {
			Web_API::UpdateConnectionStatus(0); // disconnected
		}
		return EM_TRUE;
	}
	EM_BOOL onmessage(int eventType, const EmscriptenWebSocketMessageEvent *websocketEvent, void *userData) {
		//puts("onmessage");
		if (websocketEvent->isText) {
			// For only ascii chars.
			//printf("message: %s\n", websocketEvent->data);
			std::string s = (const char*)websocketEvent->data;
			//Output::Debug("msg={}", s);

			//split by delimiter
			std::vector<std::string> v;
			std::size_t pos = 0;
			while ((pos = s.find(param_delim)) != std::string::npos) {
				v.push_back(s.substr(0, pos));
				s.erase(0, pos + param_delim.length());
			}
			if (!s.empty()) v.push_back(s);

			auto to_int = [&](const auto& str, int& num) {
				int out {};
				auto [ptr, ec] { std::from_chars(str.data(), str.data() + str.size(), out) };
				if (ec == std::errc()) {
					num = out;
					return true;
				}
				return false;
			};

			if (v.size() < 2) { //no valid commands smaller than 2 segments
				return EM_FALSE;
			}

			//Output::Debug("msg flagsize {}", v.size());
			if (v[0] == "s") { //set your id command //we need to get our id first otherwise we dont know what commands are us
				if (v.size() < 3) {
					return EM_FALSE;
				}

				if (!to_int(v[1], host_id)) {
					return EM_FALSE;
				}

				key = v[2].c_str();
			}
			else if (v[0] == "say") { //this isn't sent with an id so we do it here
				if (v.size() < 3) {
					return EM_FALSE;
				}
				Web_API::OnChatMessageReceived(v[1], v[2]);
			}
			else if (v[0] == "gsay") { //support for global messages
				if (v.size() < 6) {
					return EM_FALSE;
				}
				
				Web_API::OnGChatMessageReceived(v[1], v[2], v[3], v[4], v[5]);
			}
			else { //these are all for actions of other players, they have an id
				int id = 0;
				if (!to_int(v[1], id)) {
					return EM_FALSE;
				}
				if (id != host_id) { //if the command isn't us
					if (players.count(id) == 0) { //if this is a command for a player we don't know of, spawn him
						SpawnOtherPlayer(id);
					}
					
					PlayerOther& player = players[id];

					if (v[0] == "d") { //disconnect command
						if (v.size() < 2) {
							return EM_FALSE;
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
					else if (v[0] == "m") { //move command
						if (v.size() < 4) {
							return EM_FALSE;
						}

						int x = 0;
						int y = 0;

						if (!to_int(v[2], x)) {
							return EM_FALSE;
						}
						x = Utils::Clamp(x, 0, Game_Map::GetWidth() - 1);

						if (!to_int(v[3], y)) {
							return EM_FALSE;
						}
						y = Utils::Clamp(y, 0, Game_Map::GetHeight() - 1);

						player.mvq.push(std::make_pair(x, y));
					}
					else if (v[0] == "f") { //facing command
						if (v.size() < 3) {
							return EM_FALSE;
						}

						int facing = 0;

						if (!to_int(v[2], facing)) {
							return EM_FALSE;
						}
						facing = Utils::Clamp(facing, 0, 3);
						
						player.ch->SetFacing(facing);
					}
					else if (v[0] == "spd") { //change move speed command
						if (v.size() < 3) {
							return EM_FALSE;
						}

						int speed = 0;
						if (!to_int(v[2], speed)) {
							return EM_FALSE;
						}
						speed = Utils::Clamp(speed, 1, 6);

						player.ch->SetMoveSpeed(speed);
					}
					else if (v[0] == "spr") { //change sprite command
						if (v.size() < 4) {
							return EM_FALSE;
						}

						int idx = 0;
						if (!to_int(v[3], idx)) {
							return EM_FALSE;
						}
						idx = Utils::Clamp(idx, 0, 7);

						player.ch->SetSpriteGraphic(v[2], idx);

						Web_API::OnPlayerSpriteUpdated(v[2], idx, id);
					}
					else if (v[0] == "sys") { //change system graphic
						if (v.size() < 3) {
							return EM_FALSE;
						}

						auto chat_name = player.chat_name.get();
						if (chat_name) {
							chat_name->SetSystemGraphic(v[2]);
						}

						Web_API::OnPlayerSystemUpdated(v[2], id);
					}
					else if (v[0] == "se") { //play sound effect
						if (v.size() < 6) {
							return EM_FALSE;
						}
						
						if (mp_settings(Option::ENABLE_PLAYER_SOUNDS)) {
							int volume = 0;
							int tempo = 0;
							int balance = 0;

							if (!to_int(v[3], volume) || !to_int(v[4], tempo) || !to_int(v[5], balance)) {
								return EM_FALSE;
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
							sound.name = v[2];
							sound.volume = real_volume;
							sound.tempo = tempo;
							sound.balance = balance;

							Main_Data::game_system->SePlay(sound);
						}
					}
					else if (v[0] == "ap" || v[0] == "mp") { //show or move picture
						bool isShow = v[0] == "ap";
						int expectedSize = 19;

						if (isShow) {
							expectedSize += 2;
						}

						if (v.size() < expectedSize) {
							return EM_FALSE;
						}

						int pic_id = 0;
						if (!to_int(v[2], pic_id)) {
							return EM_FALSE;
						}

						pic_id += (id + 1) * 50; //offset to avoid conflicting with others using the same picture

						int position_x = 0;
						int position_y = 0;
						int map_x = 0;
						int map_y = 0;
						int pan_x = 0;
						int pan_y = 0;

						if (!to_int(v[3], position_x) || !to_int(v[4], position_y) || !to_int(v[5], map_x) || !to_int(v[6], map_y) | !to_int(v[7], pan_x) || !to_int(v[8], pan_y)) {
							return EM_FALSE;
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

						to_int(v[9], magnify);
						to_int(v[10], top_trans);
						to_int(v[11], bottom_trans);
						to_int(v[12], red);
						to_int(v[13], green);
						to_int(v[14], blue);
						to_int(v[15], saturation);
						to_int(v[16], effect_mode);
						to_int(v[17], effect_power);

						if (isShow) {
							int use_transparent_color_bin = 0;
							int fixed_to_map_bin = 0;

							to_int(v[19], use_transparent_color_bin);
							to_int(v[20], fixed_to_map_bin);

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
							params.name = v[18];
							params.use_transparent_color = use_transparent_color_bin ? true : false;
							params.fixed_to_map = fixed_to_map_bin ? true : false;

							Main_Data::game_pictures->Show(pic_id, params);
						} else {
							int duration = 0;

							to_int(v[18], duration);

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
					else if (v[0] == "rp") { //erase picture
						if (v.size() < 3) {
							return EM_FALSE;
						}

						int pic_id = 0;
						if (!to_int(v[2], pic_id)) {
							return EM_FALSE;
						}

						pic_id += (id + 1) * 50; //offset to avoid conflicting with others using the same picture

						Main_Data::game_pictures->Erase(pic_id);
					}
					else if (v[0] == "name") { //set nickname (and optionally change system graphic)
						if (v.size() < 3) {
							return EM_FALSE;
						}
						auto scene_map = Scene::Find(Scene::SceneType::Map);
						if (scene_map == nullptr) {
							Output::Debug("unexpected");
							//return;
						}
						auto old_list = &DrawableMgr::GetLocalList();
						DrawableMgr::SetLocalList(&scene_map->GetDrawableList());
						player.chat_name = std::make_unique<ChatName>(id, player, v[2]);
						DrawableMgr::SetLocalList(old_list);

						Web_API::OnPlayerNameUpdated(v[2], id);
					}
				}
			}
		}

		return EM_TRUE;
	}

	void init_socket(const std::string& url) {
		Output::Debug(url);
		EmscriptenWebSocketCreateAttributes ws_attrs = {
			url.c_str(),
			"binary",
			EM_TRUE
		};

		socket = emscripten_websocket_new(&ws_attrs);
		emscripten_websocket_set_onopen_callback(socket, NULL, onopen);
		//emscripten_websocket_set_onerror_callback(socket, NULL, onerror);
		emscripten_websocket_set_onclose_callback(socket, NULL, onclose);
		emscripten_websocket_set_onmessage_callback(socket, NULL, onmessage);
	}

	std::string get_room_url(int room_id) {
		auto server_url = Web_API::GetSocketURL();
		std::string room_url = server_url + std::to_string(room_id);
		return room_url;
	}
}

//this will only be called from outside
extern "C" {

void SendChatMessageToServer(const char* sys, const char* msg) {
	if (host_nickname == "") return;
	std::string s = "say" + param_delim + sys + param_delim + msg;
	TrySend(s);
}

void SendGChatMessageToServer(const char* map_id, const char* prev_map_id, const char* sys, const char* msg) {
	if (host_nickname == "") return;
	std::string s = "gsay" + param_delim + map_id + param_delim + prev_map_id + param_delim + sys + param_delim + msg;
	TrySend(s);
}

void ChangeName(const char* name) {
	if (host_nickname != "") return;
	host_nickname = name;
	SendMainPlayerName();
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
	init_socket(get_room_url(map_id));
}

void Game_Multiplayer::Quit() {
	Web_API::UpdateConnectionStatus(0); // disconnected
	session_active = false;
	emscripten_websocket_deinitialize(); //kills every socket for this thread
	players.clear();
	dc_players.clear();
	if (Main_Data::game_pictures) {
		Main_Data::game_pictures->EraseAllMultiplayer();
	}
}

void Game_Multiplayer::MainPlayerMoved(int dir) {
	SendMainPlayerPos();
}

void Game_Multiplayer::MainPlayerFacingChanged(int dir) {
	SendMainPlayerFacing(dir);
}

void Game_Multiplayer::MainPlayerChangedMoveSpeed(int spd) {
	SendMainPlayerMoveSpeed(spd);
}

void Game_Multiplayer::MainPlayerChangedSpriteGraphic(std::string name, int index) {
	SendMainPlayerSprite(name, index);
	Web_API::OnPlayerSpriteUpdated(name, index);
}

void Game_Multiplayer::SystemGraphicChanged(StringView sys) {
	SendSystemName(sys);
	Web_API::OnUpdateSystemGraphic(ToString(sys));
}

void Game_Multiplayer::SePlayed(lcf::rpg::Sound& sound) {
	if (!Main_Data::game_player->IsMenuCalling()) {
		SendSe(sound);
	}
}

void Game_Multiplayer::PictureShown(int pic_id, Game_Pictures::ShowParams& params) {
	SendShowPicture(pic_id, params);
}

void Game_Multiplayer::PictureMoved(int pic_id, Game_Pictures::MoveParams& params) {
	SendMovePicture(pic_id, params);
}

void Game_Multiplayer::PictureErased(int pic_id) {
	SendErasePicture(pic_id);
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

	if (!message_queue.empty()) {
		std::string message = message_queue.front();
		message_queue.pop();
		if (message.find("name") != 0) {
			while (!message_queue.empty()) {
				if (message_queue.front().find("name") == 0) {
					break;
				}
				std::string appendedMessage = message_delim + message_queue.front();
				if (message.size() + appendedMessage.size() > 4080) {
					break;
				}
				message += appendedMessage;
				message_queue.pop();
			}
		}
		TrySend(message);
	}
}
