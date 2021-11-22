#include <map>
#include <memory>
#include <queue>
#include <emscripten/emscripten.h>
#include <emscripten/websocket.h>
#include <charconv>

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

struct Player {
	std::queue<std::pair<int,int>> mvq; //queue of move commands
	std::unique_ptr<Game_PlayerOther> ch; //character
	std::unique_ptr<Sprite_Character> sprite;
};

namespace {
	std::unique_ptr<Window_Base> conn_status_window;
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

	void SetConnStatusWindowText(std::string s) {
		conn_status_window->GetContents()->Clear();
		conn_status_window->GetContents()->TextDraw(0, 0, Font::ColorDefault, s);
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
		std::string msg = "[Connected to room " + std::to_string(room_id) + "]";
		EM_ASM({
			GotChatMsg(UTF8ToString($0));
		}, msg.c_str());
		SetConnStatusWindowText("Connected");
		//puts("onopen");
		connected = true;
		auto& player = Main_Data::game_player;
		SendMainPlayerPos();
		SendMainPlayerMoveSpeed(player->GetMoveSpeed());
		SendMainPlayerSprite(player->GetSpriteName(), player->GetSpriteIndex());
		SendMainPlayerName();
		return EM_TRUE;
	}
	EM_BOOL onclose(int eventType, const EmscriptenWebSocketCloseEvent *websocketEvent, void *userData) {
		SetConnStatusWindowText("Disconnected");
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
			if (v[0] == "s") { //set your id command
				if (v.size() < 2) {
					return EM_FALSE;
				}

				if (!to_int(v[1], myid)) {
					return EM_FALSE;
				}
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
	//if the window doesn't exist (first map loaded) then create it
	//else, if the window is visible recreate it
	if (conn_status_window.get() == nullptr || conn_status_window->IsVisible()) {
		auto scene_map = Scene::Find(Scene::SceneType::Map);
		if (scene_map == nullptr) {
			Output::Debug("unexpected");
		}
		else {
			auto old_list = &DrawableMgr::GetLocalList();
			DrawableMgr::SetLocalList(&scene_map->GetDrawableList());
			conn_status_window = std::make_unique<Window_Base>(0, SCREEN_TARGET_HEIGHT-30, 100, 30);
			conn_status_window->SetContents(Bitmap::Create(100, 30));
			conn_status_window->SetZ(2106632960);
			DrawableMgr::SetLocalList(old_list);
		}
	}
	SetConnStatusWindowText("Disconnected");

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
	if (Input::IsReleased(Input::InputButton::N3)) {
		conn_status_window->SetVisible(!conn_status_window->IsVisible());
	}
}
