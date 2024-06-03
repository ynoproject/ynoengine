#ifndef EP_GAME_MULTIPLAYER_H
#define EP_GAME_MULTIPLAYER_H

#include <string>
#include <bitset>
#include <unordered_set>
#include "../string_view.h"
#include "../game_pictures.h"
#include "../tone.h"
#include <lcf/rpg/sound.h>
#include "yno_connection.h"

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
	void SystemGraphicChanged(StringView sys);
	void SePlayed(const lcf::rpg::Sound& sound);
	bool IsPictureSynced(int pic_id, Game_Pictures::ShowParams& params);
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

	std::unordered_set<std::string> hrs_set{
		"kalimba_c3","kalimba_c#3","kalimba_d3","kalimba_d#3","kalimba_e3","kalimba_f3","kalimba_f#3","kalimba_g3","kalimba_g#3","kalimba_a3","kalimba_a#3","kalimba_b3","kalimba_c4","kalimba_c#4","kalimba_d4","kalimba_d#4","kalimba_e4","kalimba_f4","kalimba_f#4","kalimba_g4","kalimba_g#4","kalimba_a4","kalimba_a#4","kalimba_b4","kalimba_c5","kalimba_c#5","kalimba_d5","kalimba_d#5","kalimba_e5","guitar_c3","guitar_c#3","guitar_d3","guitar_d#3","guitar_e3","guitar_f3","guitar_f#3","guitar_g3","guitar_g#3","guitar_a3","guitar_a#3","guitar_b3","guitar_c2","guitar_c#2","guitar_d2","guitar_d#2","guitar_e2","guitar_f2","guitar_f#2","guitar_g2","guitar_g#2","guitar_a2","guitar_a#2","guitar_b2","guitar_c3","guitar_c#3","guitar_d3","guitar_d#3","guitar_e3","guitar_f3","guitar_f#3","guitar_g3","guitar_g#3","guitar_a3","guitar_a#3","guitar_b3"
	};

	void SpawnOtherPlayer(int id);
	void ResetRepeatingFlash();
	void InitConnection();
};

inline Game_Multiplayer& GMI() { return Game_Multiplayer::Instance(); }

#endif
