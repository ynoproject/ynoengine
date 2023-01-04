#ifndef EP_PLAYEROTHER_H
#define EP_PLAYEROTHER_H

#include <queue>
#include <memory>

struct Game_PlayerOther;
struct Sprite_Character;
struct ChatName;
struct BattleAnimation;

struct PlayerOther {
	bool account; // player is on an account
	std::queue<std::pair<int, int>> mvq; // queue of move commands
	std::unique_ptr<Game_PlayerOther> ch; // character
	std::unique_ptr<Sprite_Character> sprite;
	std::unique_ptr<ChatName> chat_name;
	std::unique_ptr<BattleAnimation> battle_animation; // battle animation
};

#endif

