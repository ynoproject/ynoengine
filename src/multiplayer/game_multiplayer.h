#ifndef EP_GAME_MULTIPLAYER_H
#define EP_GAME_MULTIPLAYER_H

#include <string>
#include <bitset>
#include "../string_view.h"
#include "../game_pictures.h"
#include "../tone.h"
#include <lcf/rpg/sound.h>
#include "yno_packet_limiter.h"
#include "yno_connection.h"

class PlayerOther;

class Game_Multiplayer {
public:
	static Game_Multiplayer& Instance();

	Game_Multiplayer();

	void Connect(int map_id);
	void Initialize();
	void Quit();
	void Update();
	void SendBasicData();
	void MainPlayerMoved(int dir);
	void MainPlayerFacingChanged(int dir);
	void MainPlayerChangedMoveSpeed(int spd);
	void MainPlayerChangedSpriteGraphic(std::string name, int index);
	void MainPlayerFlashed(int r, int g, int b, int p, int f);
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

	enum class Option {
		ENABLE_PLAYER_SOUNDS,
		ENABLE_FLOOD_DEFENDER,
		_PLACEHOLDER, // this is used to indicate the amount of options
	};

	class SettingFlags {
	public:
		bool Get(Option option) const {
			return flags[static_cast<size_t>(option)];
		}

		bool operator()(Option o) {
			return Get(o);
		}

		void Set(Option option, bool val) {
			flags.set(static_cast<size_t>(option), val);
		}

		void Toggle(Option option) {
			flags.flip(static_cast<size_t>(option));
		}

		SettingFlags() {
			// default values here
			Set(Option::ENABLE_PLAYER_SOUNDS, true);
			Set(Option::ENABLE_FLOOD_DEFENDER, true);
		}
	protected:
		std::bitset<static_cast<size_t>(Option::_PLACEHOLDER)> flags;
	};

	SettingFlags& GetSettingFlags() { return mp_settings; }

	SettingFlags mp_settings;
	YNO::PacketLimiter m_limiter{*this};
	YNOConnection connection;
	bool session_active; // if true, it will automatically reconnect when disconnected
	bool session_connected;
	int host_id{-1};
	std::string session_token; // non-null if the user has an ynoproject account logged in
	int room_id{-1};
	int frame_index{-1};

	enum class NametagMode {
		NONE,
		CLASSIC,
		FULL_COMPACT,
		FULL_EXTRA_COMPACT
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

	void SpawnOtherPlayer(int id);
	void ResetRepeatingFlash();
	void InitConnection();
};

inline Game_Multiplayer& GMI() { return Game_Multiplayer::Instance(); }

#endif
