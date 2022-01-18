#ifndef EP_GAME_MULTIPLAYER_H
#define EP_GAME_MULTIPLAYER_H

#include <string>
#include "string_view.h"
#include "game_pictures.h"
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
	void ApplyScreenTone();
}

#endif
