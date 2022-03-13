#ifndef EP_GAME_MULTIPLAYER_H
#define EP_GAME_MULTIPLAYER_H

#include <string>
#include <bitset>
#include "string_view.h"
#include "game_pictures.h"
#include "tone.h"
#include <lcf/rpg/sound.h>

namespace Game_Multiplayer {
	void Connect(int map_id);
	void Quit();
	void Update();
	void MainPlayerMoved(int dir);
	void MainPlayerFacingChanged(int dir);
	void MainPlayerChangedMoveSpeed(int spd);
	void MainPlayerChangedSpriteGraphic(std::string name, int index);
	void SystemGraphicChanged(StringView sys);
	void SePlayed(lcf::rpg::Sound& sound);
	void PictureShown(int pic_id, Game_Pictures::ShowParams& params);
	void PictureMoved(int pic_id, Game_Pictures::MoveParams& params);
	void PictureErased(int pic_id);
	void ApplyFlash(int r, int g, int b, int power, int frames);
	void ApplyTone(Tone tone);
	void ApplyScreenTone();

	enum class Option {
		SINGLE_PLAYER,
		ENABLE_NICKS,
		ENABLE_PLAYER_SOUNDS,
		ENABLE_GLOBAL_MESSAGE_LOCATION,
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
			Set(Option::ENABLE_NICKS, true);
			Set(Option::SINGLE_PLAYER, false);
			Set(Option::ENABLE_PLAYER_SOUNDS, true);
			Set(Option::ENABLE_GLOBAL_MESSAGE_LOCATION, true);
			Set(Option::ENABLE_FLOOD_DEFENDER, true);
		}
	protected:
		std::bitset<static_cast<size_t>(Option::_PLACEHOLDER)> flags;
	};

	SettingFlags& GetSettingFlags();
}

#endif
