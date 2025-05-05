#ifndef EP_GAME_MULTIPLAYER_H
#define EP_GAME_MULTIPLAYER_H

#include <optional>
#include <string>
#include <bitset>
#include "../string_view.h"
#include "../game_pictures.h"
#include "../tone.h"
#include <lcf/rpg/sound.h>
#include "yno_connection.h"
#include <emscripten/emscripten.h>
#include <utility>
#include "messages.h"

class PlayerOther;

class Game_Multiplayer {
public:
	static Game_Multiplayer& Instance();

	Game_Multiplayer();

	void Connect(int map_id, bool room_switch = false);
	void Initialize();
	void Quit();
	void Update();
	void SendBasicData();
	void MainPlayerMoved(int dir);
	void MainPlayerFacingChanged(int dir);
	void MainPlayerChangedMoveSpeed(int spd);
	void MainPlayerChangedSpriteGraphic(std::string name, int index);
	void MainPlayerJumped(int x, int y);
	void MainPlayerFlashed(int r, int g, int b, int p, int f);
	void MainPlayerChangedTransparency(int transparency);
	void MainPlayerChangedSpriteHidden(bool hidden);
	void MainPlayerTeleported(int map_id, int x, int y);
	void MainPlayerTriggeredEvent(int event_id, bool action);
	void SystemGraphicChanged(std::string_view sys);
	void SePlayed(const lcf::rpg::Sound& sound);
	bool IsPictureSynced(int pic_id, std::string_view pic_name);
	void PictureShown(int pic_id, Game_Pictures::ShowParams& params);
	void PictureMoved(int pic_id, Game_Pictures::MoveParams& params);
	void PictureErased(int pic_id);
	bool IsBattleAnimSynced(int anim_id);
	void PlayerBattleAnimShown(int anim_id);
	void ApplyPlayerBattleAnimUpdates();
	void ApplyFlash(int r, int g, int b, int power, int frames);
	void ApplyRepeatingFlashes();
	void ApplyTone(Tone tone);
	void ApplyScreenTone();
	void SwitchSet(int switch_id, int value);
	void VariableSet(int var_id, int value);

	using RequestId = uint32_t;

	// Allows up to five ints
	template<typename... Args, typename = std::enable_if_t<(std::is_convertible_v<Args, int> && ...)>>
	RequestId MakeRpcRequest(std::string method, Args... args);

	struct {
		bool enable_sounds{ true };
		bool mute_audio{ false };
		int moving_queue_limit{ 4 };
	} settings;

	YNOConnection connection;
	bool session_active{ false }; // if true, it will automatically reconnect when disconnected
	bool session_connected{ false };
	bool switching_room{ true }; // when client enters new room, but not synced to server
	bool switched_room{ false }; // determines whether new connected players should fade in
	int host_id{-1};
	std::string session_token; // non-null if the user has an ynoproject account logged in
	int room_id{-1};
	int frame_index{-1};

	enum class NametagMode {
		NONE,
		CLASSIC,
		COMPACT,
		SLIM
	};

	NametagMode GetNametagMode() { return nametag_mode; }
	void SetNametagMode(int mode) {
		nametag_mode = static_cast<NametagMode>(mode);
	}
	NametagMode nametag_mode{NametagMode::CLASSIC};

	std::map<int, PlayerOther> players;
	std::vector<PlayerOther> dc_players;
	std::vector<int> sync_switches;
	std::vector<int> sync_vars;
	std::vector<int> sync_events;
	std::vector<int> sync_action_events;
	std::vector<std::string> sync_picture_names; // for badge conditions
	std::vector<std::string> global_sync_picture_names;
	std::vector<std::string> global_sync_picture_prefixes;
	std::map<int, bool> sync_picture_cache;
	std::vector<int> sync_battle_anim_ids;
	int last_flash_frame_index{-1};
	std::unique_ptr<std::array<int, 5>> last_frame_flash;
	std::map<int, std::array<int, 5>> repeating_flashes;

	struct RequestContext {
		std::array<int, 5> params;
		std::string method;
		// if code is not 0, response is error message
		std::optional<std::string> response;
		int code = 0;
		explicit RequestContext(std::array<int, 5> _params, std::string _method)
			: params(_params), method(std::move(_method)) {}
	};
	std::map<RequestId, RequestContext> rpc_requests;

	void SpawnOtherPlayer(int id);
	void ResetRepeatingFlash();
	void InitConnection();
};

inline Game_Multiplayer& GMI() { return Game_Multiplayer::Instance(); }

template<typename... Args, typename>
inline auto Game_Multiplayer::MakeRpcRequest(std::string method, Args... args) -> Game_Multiplayer::RequestId {
	using Messages::C2S::RpcRequestPacket;
	std::vector<std::string> params {std::to_string(args)...};
	connection.SendPacketAsync<RpcRequestPacket>(method, std::move(params));

	unsigned id = RpcRequestPacket::LastId();
	// rpc_requests.insert_or_assign(id, (RequestContext){{args...}, method});
	rpc_requests.emplace(std::make_pair(id, RequestContext{{args...}, method}));

	// EM_ASM({
	// 	setTimeout(() => {
	// 		Module.ccall("update_rpc_requests", null, ["number"], [$0]);
	// 	}, 1000);
	// }, id);

	return id;
}

#endif
