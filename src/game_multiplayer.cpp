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
#include "sprite_character.h"
#include "window_base.h"
#include "drawable_mgr.h"
#include "scene.h"
#include "bitmap.h"
#include "font.h"
#include "input.h"
#include "game_map.h"
#include "game_system.h"
#include "cache.h"

namespace {
	bool nicks_visible = true;
}

struct Player;

class ChatName : public Drawable {
public:
	ChatName(int id, Player& player, std::string nickname);

	void Draw(Bitmap& dst) override;

	void SetSystemGraphic(StringView sys_name);

private:
	Player& player;
	std::string nickname;
	BitmapRef nick_img;
	BitmapRef sys_graphic;
	std::shared_ptr<int> request_id;
	bool dirty = true;
};

struct Player {
	std::queue<std::pair<int,int>> mvq; //queue of move commands
	std::unique_ptr<Game_PlayerOther> ch; //character
	std::unique_ptr<Sprite_Character> sprite;
	std::unique_ptr<ChatName> chat_name;
};

ChatName::ChatName(int id, Player& player, std::string nickname) : player(player), nickname(std::move(nickname)), Drawable(Priority_Frame + (id << 8)) {
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
	int myid = -1;
	int room_id = -1;
	std::string my_name = "";
	std::map<int,Player> players;
	const std::string delimchar = "\uffff";

	void TrySend(std::string msg) {
		if (!connected) return;
		unsigned short ready;
		emscripten_websocket_get_ready_state(socket, &ready);
		if (ready == 1) { //1 means OPEN
			emscripten_websocket_send_binary(socket, (void*)msg.c_str(), msg.length());
		}
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
		DrawableMgr::SetLocalList(old_list);
	}

	void SendMainPlayerPos() {
		auto& player = Main_Data::game_player;
		std::string msg = "m" + delimchar + std::to_string(player->GetX()) + delimchar + std::to_string(player->GetY());
		TrySend(msg);
	}

	void SendMainPlayerMoveSpeed(int spd) {
		std::string msg = "spd" + delimchar + std::to_string(spd);
		TrySend(msg);
	}

	void SendMainPlayerSprite(std::string name, int index) {
		std::string msg = "spr" + delimchar + name + delimchar + std::to_string(index);
		TrySend(msg);
	}

	void SendMainPlayerName() {
		if (my_name == "") return;
		std::string msg = "name" + delimchar + my_name;
		TrySend(msg);
	}

	void SendSystemName(StringView sys) {
		std::string msg = "sys" + delimchar + ToString(sys);
		TrySend(msg);
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
			while ((pos = s.find(delimchar)) != std::string::npos) {
				v.push_back(s.substr(0, pos));
				s.erase(0, pos + delimchar.length());
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

			if (v.empty()) {
				return EM_FALSE;
			}

			//Output::Debug("msg flagsize {}", v.size());
			if (v[0] == "s") { //set your id command (and get player count)
				if (v.size() < 3) {
					return EM_FALSE;
				}

				if (!to_int(v[1], myid)) {
					return EM_FALSE;
				}

				EM_ASM({
					updatePlayerCount(UTF8ToString($0));
				}, v[2].c_str());
			}
			else {
				if (v.size() < 2) {
					return EM_FALSE;
				}

				if (v[0] == "say") {
					EM_ASM({
						GotChatMsg(UTF8ToString($0));
					}, v[1].c_str());
				}
				else {
					int id = 0;
					if (!to_int(v[1], id)) {
						return EM_FALSE;
					}
					if (id != myid) {
						if (players.count(id) == 0) { //if this is a command for a plyer we don't know of, spawn him
							SpawnOtherPlayer(id);
						}
						if (v[0] == "d") { //disconnect command
							auto scene_map = Scene::Find(Scene::SceneType::Map);
							if (scene_map == nullptr) {
								Output::Debug("unexpected");
								//return;
							}
							auto old_list = &DrawableMgr::GetLocalList();
							DrawableMgr::SetLocalList(&scene_map->GetDrawableList());
							players.erase(id);
							DrawableMgr::SetLocalList(old_list);
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

							players[id].mvq.push(std::make_pair(x, y));
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

							players[id].ch->SetMoveSpeed(speed);
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

							players[id].ch->SetSpriteGraphic(v[2], idx);
						}
						else if (v[0] == "sys") {
							if (v.size() < 3) {
								return EM_FALSE;
							}

							auto chat_name = players[id].chat_name.get();
							if (chat_name) {
								chat_name->SetSystemGraphic(v[2]);
							}
						}
						else if (v[0] == "name") { // nickname
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
							players[id].chat_name = std::make_unique<ChatName>(id, players[id], v[2]);
							DrawableMgr::SetLocalList(old_list);
						}
						//also there's a connect command "c %id%" - player with id %id% has connected
					}
				}
			}
		}

		return EM_TRUE;
	}
}

//this will only be called from outside
extern "C" {

void SendChatMessageToServer(const char* msg) {
	if (my_name == "") return;
	std::string s = "say" + delimchar;
	s += msg;
	TrySend(s);
}

void ChangeName(const char* name) {
	if (my_name != "") return;
	my_name = name;
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

void Game_Multiplayer::MainPlayerChangedMoveSpeed(int spd) {
	SendMainPlayerMoveSpeed(spd);
}

void Game_Multiplayer::MainPlayerChangedSpriteGraphic(std::string name, int index) {
	SendMainPlayerSprite(name, index);
}

void Game_Multiplayer::SystemGraphicChanged(StringView sys) {
	SendSystemName(sys);
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
	if (Input::IsTriggered(Input::InputButton::N4)) {
		nicks_visible = !nicks_visible;
	}
}
