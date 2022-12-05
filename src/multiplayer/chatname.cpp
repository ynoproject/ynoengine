#include "chatname.h"
#include "../cache.h"
#include "../font.h"
#include "../drawable_mgr.h"
#include "../filefinder.h"

std::map<std::string, std::array<int, 96>> sprite_y_offsets;

ChatName::ChatName(int id, PlayerOther& player, std::string nickname)
	:player(player),
	nickname(std::move(nickname)),
	Drawable(Priority_Frame + (id << 8)) {
	DrawableMgr::Register(this);
}

void ChatName::Draw(Bitmap& dst) {
	auto sprite = player.sprite.get();
	
	if (!GMI().GetSettingFlags().Get(Game_Multiplayer::Option::ENABLE_NICKS) || nickname.empty() || !sprite) {
		nick_img.reset();
		dirty = true;
		return;
	}

	auto enable_new_nametags = GMI().GetSettingFlags().Get(Game_Multiplayer::Option::ENABLE_NEW_NAMETAGS);

	if (new_nametag != enable_new_nametags) {
		new_nametag = enable_new_nametags;
		nick_img.reset();
		dirty = true;
	}

	int nick_offset_x = enable_new_nametags ? 1 : 0;

	if (dirty) {
		std::string nick_trim;

		if (enable_new_nametags) {
			nick_trim = nickname;
		} else {
			// Up to 3 utf-8 s
			Utils::UtfNextResult utf_next;
			utf_next.next = nickname.data();
			auto end = nickname.data() + nickname.size();
	
			for (int i = 0; i < 3; ++i) {
				utf_next = Utils::UTF8Next(utf_next.next, end);
				if (utf_next.next == end) {
					break;
				}
			}
			nick_trim.append((const char*)nickname.data(), utf_next.next);
		}

		if (nick_trim.empty()) {
			return;
		}

		auto rect = Font::NameText()->GetSize(nick_trim);
		nick_img = Bitmap::Create(rect.width + 1, rect.height + 1, true);

		BitmapRef sys;
		if (sys_graphic) {
			sys = sys_graphic;
		} else {
			sys = Cache::SystemOrBlack();
		}

		Text::Draw(*nick_img, nick_offset_x, 0, *Font::NameText(), *sys, 0, nick_trim);
		
		dirty = false;

		effects_dirty = true;
	}

	if (flash_frames_left > 0) {
		--flash_frames_left;
		effects_dirty = true;
	}

	if (effects_dirty) {
		bool no_tone = player.sprite->GetTone() == Tone();
		bool no_flash = player.sprite->GetCharacter()->GetFlashColor().alpha == 0;

		effects_img.reset();

		if (no_tone && no_flash) {
			effects_img = nick_img;
		} else {
			effects_img = Cache::SpriteEffect(nick_img, nick_img->GetRect(), false, false, player.sprite->GetTone(), player.sprite->GetCharacter()->GetFlashColor());
		}

		effects_dirty = false;
	}

	if (!player.ch->IsSpriteHidden()) {
		int x = player.ch->GetScreenX() - nick_img->GetWidth() / 2 - 1;
		int y = (player.ch->GetScreenY() - TILE_SIZE * 2) + GetSpriteYOffset();
		if (transparent && base_opacity > 16) {
			SetBaseOpacity(base_opacity - 1);
		} else if (!transparent && base_opacity < 32) {
			SetBaseOpacity(base_opacity + 1);
		}
		dst.Blit(x + nick_offset_x, y, *effects_img, nick_img->GetRect(), Opacity(GetOpacity()));
	}
}

void ChatName::SetSystemGraphic(StringView sys_name) {
	FileRequestAsync* request = AsyncHandler::RequestFile("System", sys_name);
	request_id = request->Bind([this](FileRequestResult* result) {
		if (!result->success) {
			return;
		}
		sys_graphic = Cache::System(result->file);
		dirty = true;
	});
	request->SetGraphicFile(true);
	request->Start();
}

void ChatName::SetTransparent(bool val) {
	transparent = val;
}

int ChatName::GetOpacity() {
	float opacity = (float)player.ch->GetOpacity() * ((float)base_opacity / 32.0);
	return std::floor(opacity);
}

int ChatName::GetSpriteYOffset() {
	std::string sprite_name = player.ch->GetSpriteName();
	if (!sprite_y_offsets.count(sprite_name)) {
		auto filename = FileFinder::FindImage("CharSet", sprite_name);
		if (filename == "") {
			return 0;
		}

		auto offset_array = std::array<int, 96>{ 0 };

		const int BASE_OFFSET = -13;
		const size_t BGRA = 4;

		auto image = Cache::Charset(sprite_name);

		for (int hi = 0; hi < image->height() / 128; ++hi) {
			for (int wi = 0; wi < image->width() / 72; ++wi) {
				for (int fi = 0; fi < 4; ++fi) {
					for (int afi = 0; afi < 3; ++afi) {
						int i = ((hi << 2) + wi) * 12 + (fi * 3) + afi;

						int start_x = wi * 72 + afi * 24;
						int start_y = hi * 128 + fi * 32;

						int y = start_y;

						bool offset_found = false;

						for (; y < start_y + 32; ++y) {
							for (int x = start_x; x < start_x + 24; ++x) {
								size_t index = BGRA * (y * image->width() + x);
								auto pixels = reinterpret_cast<unsigned char*>(image->pixels());
								if (pixels[index + 3] != 0) { // check if alpha is not 0 (fully transparent)
									offset_found = true;
									break;
								}
							}
							
							if (offset_found) {
								break;
							}
						}

						if (offset_found) {
							if (y > start_y + 15) {
								y = start_y + 15;
							}
							offset_array[i] = BASE_OFFSET + (y - start_y);
						} else {
							offset_array[i] = 32;
						}
					}
				}
			}

			sprite_y_offsets[sprite_name] = std::array<int, 96>(offset_array);
		}
	}

	auto frame = player.ch->GetAnimFrame();
	if (frame >= lcf::rpg::EventPage::Frame_middle2) {
		frame = lcf::rpg::EventPage::Frame_middle;
	}

	int ret = sprite_y_offsets[sprite_name][player.ch->GetSpriteIndex() * 12 + player.ch->GetFacing() * 3 + frame];

	if (ret != 32) {
		last_valid_sprite_y_offset = ret;
	} else {
		return last_valid_sprite_y_offset;
	}

	return ret;
}
