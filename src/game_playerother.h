#ifndef EP_GAME_PLAYEROTHER_H
#define EP_GAME_PLAYEROTHER_H

#include "game_character.h"
#include <lcf/rpg/savepartylocation.h>

using Game_PlayerBase = Game_CharacterDataStorage<lcf::rpg::SavePartyLocation>;

/**
 * Game_PlayerOther class
 * game character of other clients
 */
class Game_PlayerOther : public Game_PlayerBase {
public:
	Game_PlayerOther() : Game_PlayerBase(PlayerOther)
	{
		SetDirection(lcf::rpg::EventPage::Direction_down);
		SetMoveSpeed(4);
		SetAnimationType(lcf::rpg::EventPage::AnimType_non_continuous);
	}

	void UpdateNextMovementAction() override {
		//literally just do nothing
	}

	void UpdateAnimation() override {
		//animation is controlled by frame updates
	}

	void Update() {
		Game_Character::Update();
	}
};

#endif