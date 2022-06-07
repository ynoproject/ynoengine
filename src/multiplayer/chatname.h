#ifndef EP_CHATNAME_H
#define EP_CHATNAME_H

#include <queue>

#include "game_multiplayer.h"
#include "../game_playerother.h"
#include "../sprite_character.h"
#include "../bitmap.h"

struct PlayerOther;

class ChatName : public Drawable {
public:
	ChatName(int id, PlayerOther& player, std::string nickname);

	void Draw(Bitmap& dst) override;

	void SetSystemGraphic(StringView sys_name);

	void SetTransparent(bool val);

private:
	PlayerOther& player;
	std::string nickname;
	BitmapRef nick_img;
	BitmapRef sys_graphic;
	std::shared_ptr<int> request_id;
	bool transparent;
	int base_opacity = 64;
	bool dirty = true;

	void SetBaseOpacity(int val);
	int GetOpacity();
	bool LoadSpriteImage(std::vector<unsigned char>& image, const std::string& filename, int& width, int& height);
	int GetSpriteYOffset();
};

inline void ChatName::SetBaseOpacity(int val) {
	base_opacity = std::clamp(val, 0, 64);
};

struct PlayerOther {
	std::queue<std::pair<int,int>> mvq; //queue of move commands
	std::unique_ptr<Game_PlayerOther> ch; //character
	std::unique_ptr<Sprite_Character> sprite;
	std::unique_ptr<ChatName> chat_name;
};

#endif

