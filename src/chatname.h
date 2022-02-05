#ifndef EP_CHATNAME_H
#define EP_CHATNAME_H

#include <queue>

#include "game_multiplayer.h"
#include "game_playerother.h"
#include "sprite_character.h"
#include "bitmap.h"

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

#endif

