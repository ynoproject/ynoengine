#include <map>
#include <memory>
#include <queue>
#include <emscripten/emscripten.h>
#include <emscripten/websocket.h>
#include <charconv>
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

namespace {
	bool nicks_visible = true;
	bool player_sounds = true;
}

struct PlayerOther;

class ChatName : public Drawable {
public:
	ChatName(int id, PlayerOther& player, std::string nickname);

	void Draw(Bitmap& dst) override;

	void SetSystemGraphic(StringView sys_name);

private:
	PlayerOther& player;
	std::string nickname;
	BitmapRef nick_img;
	BitmapRef sys_graphic;
	std::shared_ptr<int> request_id;
	bool dirty = true;
};

struct PlayerOther {
	std::queue<std::pair<int,int>> mvq; //queue of move commands
	std::unique_ptr<Game_PlayerOther> ch; //character
	std::unique_ptr<Sprite_Character> sprite;
	std::unique_ptr<ChatName> chat_name;
};

ChatName::ChatName(int id, PlayerOther& player, std::string nickname) : player(player), nickname(std::move(nickname)), Drawable(Priority_Frame + (id << 8)) {
	DrawableMgr::Register(this);
}

void ChatName::Draw(Bitmap& dst) {
	auto sprite = player.sprite.get();
	if (!nicks_visible || nickname.empty() || !sprite) {
		nick_img.reset();
		dirty = true;
		return;
	}

	if (dirty) {
		// Up to 3 utf-8 characters
		Utils::UtfNextResult utf_next;
		utf_next.next = nickname.data();
		auto end = nickname.data() + nickname.size();

		for (int i = 0; i < 3; ++i) {
			utf_next = Utils::UTF8Next(utf_next.next, end);
			if (utf_next.next == end) {
				break;
			}
		}
		std::string nick_trim;
		nick_trim.append((const char*)nickname.data(), utf_next.next);
		auto rect = Font::Default()->GetSize(nick_trim);
		if (nick_trim.empty()) {
			return;
		}

		nick_img = Bitmap::Create(rect.width + 1, rect.height + 1, true);

		BitmapRef sys;
		if (sys_graphic) {
			sys = sys_graphic;
		} else {
			sys = Cache::SystemOrBlack();
		}

		Text::Draw(*nick_img, 0, 0, *Font::Default(), *sys, 0, nick_trim);

		dirty = false;
	}

	int x = player.ch->GetScreenX() - nick_img->GetWidth() / 2 - 1;
	int y = player.ch->GetScreenY() - TILE_SIZE * 2;
	dst.Blit(x, y, *nick_img, nick_img->GetRect(), Opacity::Opaque());
}

void ChatName::SetSystemGraphic(StringView sys_name) {
	FileRequestAsync* request = AsyncHandler::RequestFile("System", sys_name);
	request_id = request->Bind([this](FileRequestResult* result) {
		if (!result->success) {
			return;
		}
		sys_graphic = Cache::System(result->file);
		dirty = true;
	});
	request->SetGraphicFile(true);
	request->Start();
};

namespace {
	EMSCRIPTEN_WEBSOCKET_T socket;
	bool connected = false;
	int host_id = -1;
	int room_id = -1;
	int msg_count = 0;
	std::string host_nickname = "";
	std::string key = "";
	std::map<int, PlayerOther> players;
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
		TrySend(msg);
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
			+ param_delim + std::to_string(params.magnify) + param_delim + std::to_string(params.top_trans) + param_delim + std::to_string(params.bottom_trans) + param_delim
			+ std::to_string(params.red) + param_delim + std::to_string(params.green) + param_delim + std::to_string(params.blue) + param_delim + std::to_string(params.saturation)
			+ param_delim + std::to_string(params.effect_mode) + param_delim + std::to_string(params.effect_power) + param_delim + SanitizeParameter(params.name) + param_delim + std::to_string(int(params.use_transparent_color)); 
		QueueMessage(msg);
	}

	void SendMovePicture(int pic_id, Game_Pictures::MoveParams& params) {
		std::string msg = "mp" + param_delim + std::to_string(pic_id) + param_delim + std::to_string(params.position_x) + param_delim + std::to_string(params.position_y)
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
		if (abs(dx) > 1 || abs(dy) > 1 || dx == 0 && dy == 0) {
			player->SetX(x);
			player->SetY(y);
			return;
		}
		int dir[3][3] = {{Game_Character::Direction::UpLeft, Game_Character::Direction::Up, Game_Character::Direction::UpRight},
						 {Game_Character::Direction::Left, 0, Game_Character::Direction::Right},
						 {Game_Character::Direction::DownLeft, Game_Character::Direction::Down, Game_Character::Direction::DownRight}};
		player->Move(dir[dy+1][dx+1]);
	}

	EM_BOOL onopen(int eventType, const EmscriptenWebSocketOpenEvent *websocketEvent, void *userData) {
		EM_ASM(
			onUpdateConnectionStatus(1); //connected
		);
 		//puts("onopen");
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
		EM_ASM(
			onUpdateConnectionStatus(0); //disconnected
		);
		//puts("onclose");
		connected = false;

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
			if (v[0] == "s") { //set your id (and get player count) command //we need to get our id first otherwise we dont know what commands are us
				if (v.size() < 4) {
					return EM_FALSE;
				}

				if (!to_int(v[1], host_id)) {
					return EM_FALSE;
				}

				EM_ASM({
					updatePlayerCount(UTF8ToString($0));
				}, v[2].c_str());

				key = v[3].c_str(); //let's hope it works
			}
			else if (v[0] == "say") { //this isn't sent with an id so we do it here
				if (v.size() < 3) {
					return EM_FALSE;
				}
				EM_ASM({
					onChatMessageReceived(UTF8ToString($0), UTF8ToString($1));
				}, v[1].c_str(), v[2].c_str());
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

					if (v[0] == "c") { //connect command
						if (v.size() < 3) {
							return EM_FALSE;
						}

						EM_ASM({
							updatePlayerCount(UTF8ToString($0));
						}, v[2].c_str());
					}
					else if (v[0] == "d") { //disconnect command
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
						players.erase(id);
						DrawableMgr::SetLocalList(old_list);
						EM_ASM({
							updatePlayerCount(UTF8ToString($0));
						}, v[2].c_str());
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
					}
					else if (v[0] == "sys") { //change system graphic
						if (v.size() < 3) {
							return EM_FALSE;
						}

						auto chat_name = player.chat_name.get();
						if (chat_name) {
							chat_name->SetSystemGraphic(v[2]);
						}
					}
					else if (v[0] == "se") { //play sound effect
						if (v.size() < 6) {
							return EM_FALSE;
						}
						
						if (player_sounds) {
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
						return EM_FALSE; //temporarily disable this feature

						bool isShow = v[0] == "ap";
						int expectedSize = 15;

						if (isShow) {
							expectedSize++;
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

						if (!to_int(v[3], position_x) || !to_int(v[4], position_y)) {
							return EM_FALSE;
						}
						
						position_x += player.ch->GetX() - Main_Data::game_player->GetX();
						position_y += player.ch->GetY() - Main_Data::game_player->GetY();

						int magnify = 100;
						int top_trans = 0;
						int bottom_trans = 0;
						int red = 100;
						int green = 100;
						int blue = 100;
						int saturation = 100;
						int effect_mode = 0;
						int effect_power = 0;

						to_int(v[5], magnify);
						to_int(v[6], top_trans);
						to_int(v[7], bottom_trans);
						to_int(v[8], red);
						to_int(v[9], green);
						to_int(v[10], blue);
						to_int(v[11], saturation);
						to_int(v[12], effect_mode);
						to_int(v[13], effect_power);

						if (isShow) {
							int use_transparent_color_bin = 0;

							to_int(v[15], use_transparent_color_bin);

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
							params.name = v[14];
							params.use_transparent_color = use_transparent_color_bin ? true : false;

							Main_Data::game_pictures->Show(pic_id, params);
						} else {
							int duration = 0;

							to_int(v[14], duration);

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
						return EM_FALSE; //temporarily disable this feature

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
					}
				}
			}
		}

		return EM_TRUE;
	}
}

//this will only be called from outside
extern "C" {

void SendChatMessageToServer(const char* sys, const char* msg) {
	if (host_nickname == "") return;
	std::string s = "say" + param_delim + sys + param_delim + msg;
	TrySend(s);
}

void ChangeName(const char* name) {
	if (host_nickname != "") return;
	host_nickname = name;
	SendMainPlayerName();
}

}
void Game_Multiplayer::Connect(int map_id) {
	room_id = map_id;
	Game_Multiplayer::Quit();
	EM_ASM(
		onUpdateConnectionStatus(0); //disconnected
	);

	char* server_url = (char*)EM_ASM_INT({
	  var ws = Module.EASYRPG_WS_URL;
	  var len = lengthBytesUTF8(ws)+1;
	  var wasm_str = _malloc(len);
	  stringToUTF8(ws, wasm_str, len);
	  return wasm_str;
	});

	std::string room_url = server_url + std::to_string(map_id);
	free(server_url);

	Output::Debug(room_url);
	EmscriptenWebSocketCreateAttributes ws_attrs = {
		room_url.c_str(),
		"binary",
		EM_TRUE
	};

	socket = emscripten_websocket_new(&ws_attrs);
	emscripten_websocket_set_onopen_callback(socket, NULL, onopen);
	//emscripten_websocket_set_onerror_callback(socket, NULL, onerror);
	emscripten_websocket_set_onclose_callback(socket, NULL, onclose);
	emscripten_websocket_set_onmessage_callback(socket, NULL, onmessage);
}

void Game_Multiplayer::Quit() {
	emscripten_websocket_deinitialize(); //kills every socket for this thread
	players.clear();
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
}

void Game_Multiplayer::SystemGraphicChanged(StringView sys) {
	SendSystemName(sys);
	EM_ASM({
		onUpdateSystemGraphic(UTF8ToString($0));
	}, ToString(sys).c_str());
}

void Game_Multiplayer::SePlayed(lcf::rpg::Sound& sound) {
	if (!Main_Data::game_player->IsMenuCalling()) {
		SendSe(sound);
	}
}

void Game_Multiplayer::PictureShown(int pic_id, Game_Pictures::ShowParams& params) {
	//SendShowPicture(pic_id, params);
}

void Game_Multiplayer::PictureMoved(int pic_id, Game_Pictures::MoveParams& params) {
	//SendMovePicture(pic_id, params);
}

void Game_Multiplayer::PictureErased(int pic_id) {
	//SendErasePicture(pic_id);
}

void Game_Multiplayer::ApplyFlash(int r, int g, int b, int power, int frames) {
for (auto& p : players) {
		p.second.ch->Flash(r, g, b, power, frames);
	}
}

void Game_Multiplayer::ApplyScreenTone() {
	for (auto& p : players) {
		p.second.sprite->SetTone(Main_Data::game_screen->GetTone());
	}
}

void Game_Multiplayer::Update() {
	for (auto& p : players) {
		auto& q = p.second.mvq;
		if (!q.empty() && p.second.ch->IsStopping()) {
			MovePlayerToPos(p.second.ch, q.front().first, q.front().second);
			q.pop();
		}
		p.second.ch->SetProcessed(false);
		p.second.ch->Update();
		p.second.sprite->Update();
	}
	if (!message_queue.empty()) {
		std::string message = message_queue.front();
		message_queue.pop();
		while (!message_queue.empty()) {
			std::string appendedMessage = message_delim + message_queue.front();
			if (message.size() + appendedMessage.size() > 498) {
				break;
			}
			message += message_delim + message_queue.front();
			message_queue.pop();
		}
		TrySend(message);
	}
	if (Input::IsTriggered(Input::InputButton::N2)) {
		nicks_visible = !nicks_visible;
	}
	if (Input::IsTriggered(Input::InputButton::N3)) {
		player_sounds = !player_sounds;
	}
}
