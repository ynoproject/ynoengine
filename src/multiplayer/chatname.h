#ifndef EP_CHATNAME_H
#define EP_CHATNAME_H

#include <queue>

#include "game_multiplayer.h"
#include "../game_playerother.h"
#include "../sprite_character.h"
#include "../bitmap.h"
#include "../battle_animation.h"

struct PlayerOther;

class ChatName : public Drawable {
public:
	ChatName(int id, PlayerOther& player, std::string nickname);

	void Draw(Bitmap& dst) override;

	void SetSystemGraphic(StringView sys_name);

	void SetEffectsDirty();
	
	void SetFlashFramesLeft(int frames);

	void SetTransparent(bool val);

private:
	PlayerOther& player;
	std::string nickname;
	BitmapRef nick_img;
	BitmapRef sys_graphic;
	BitmapRef effects_img;
	std::shared_ptr<int> request_id;
	bool transparent;
	int base_opacity = 32;
	bool dirty = true;
	bool effects_dirty;
	Game_Multiplayer::NametagMode nametag_mode_cache;
	int flash_frames_left;
	int last_valid_sprite_y_offset;
	
	void SetBaseOpacity(int val);
	int GetOpacity();
	int GetSpriteYOffset();
};

inline void ChatName::SetEffectsDirty() {
	effects_dirty = true;
};

inline void ChatName::SetFlashFramesLeft(int frames) {
	flash_frames_left = frames;
};

inline void ChatName::SetBaseOpacity(int val) {
	base_opacity = std::clamp(val, 0, 32);
};

struct PlayerOther {
	bool account; // player is on an account
	std::queue<std::pair<int,int>> mvq; // queue of move commands
	std::unique_ptr<Game_PlayerOther> ch; // character
	std::unique_ptr<Sprite_Character> sprite;
	std::unique_ptr<ChatName> chat_name;
	std::unique_ptr<BattleAnimation> ba; // battle animation
};

#endif
