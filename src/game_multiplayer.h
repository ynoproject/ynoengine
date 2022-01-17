#ifndef EP_GAME_MULTIPLAYER_H
#define EP_GAME_MULTIPLAYER_H

#include <string>
#include "string_view.h"
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
	void ApplyFlash(int r, int g, int b, int power, int frames);
	void ApplyScreenTone();
}

#endif
