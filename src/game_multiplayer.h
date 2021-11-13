#ifndef EP_GAME_MULTIPLAYER_H
#define EP_GAME_MULTIPLAYER_H

#include <string>

namespace Game_Multiplayer {
	void Connect(int map_id);
	void Quit();
	void Update();
	void MainPlayerMoved(int dir);
	void MainPlayerChangedMoveSpeed(int spd);
	void MainPlayerChangedSpriteGraphic(std::string name, int index);
}

#endif
