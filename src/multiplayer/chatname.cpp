#include "chatname.h"
#include "../cache.h"
#include "../font.h"
#include "../drawable_mgr.h"
#include "../filefinder.h"
#include "../output.h"

extern "C" {
	#define STB_IMAGE_IMPLEMENTATION
	#include "../external/stb_image.h"

	std::map<std::string, std::array<int, 8>> sprite_y_offsets;
}

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

	if (dirty) {
		// Up to 3 utf-8 characters
		Utils::UtfNextResult utf_next;
		utf_next.next = nickname.data();
		auto end = nickname.data() + nickname.size();

		for (int i = 0; i < 3; ++i) {
			utf_next = Utils::UTF8Next(utf_next.next, end);
			if (utf_next.next == end) {
				break;
			}
		}
		std::string nick_trim;
		nick_trim.append((const char*)nickname.data(), utf_next.next);
		auto rect = Font::Default()->GetSize(nick_trim);
		if (nick_trim.empty()) {
			return;
		}

		nick_img = Bitmap::Create(rect.width + 1, rect.height + 1, true);

		BitmapRef sys;
		if (sys_graphic) {
			sys = sys_graphic;
		} else {
			sys = Cache::SystemOrBlack();
		}

		Text::Draw(*nick_img, 0, 0, *Font::Default(), *sys, 0, nick_trim);

		dirty = false;
	}

	if (!player.ch->IsSpriteHidden()) {
		int x = player.ch->GetScreenX() - nick_img->GetWidth() / 2 - 1;
		int y = (player.ch->GetScreenY() - TILE_SIZE * 2) + GetSpriteYOffset();
		dst.Blit(x, y, *nick_img, nick_img->GetRect(), Opacity(player.ch->GetOpacity()));
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

bool ChatName::LoadSpriteImage(std::vector<unsigned char>& image, const std::string& filename, int& width, int& height) {
	int x, y, n;
	unsigned char* data = stbi_load(filename.c_str(), &width, &height, &n, 3);
	if (data != nullptr) {
		image = std::vector<unsigned char>(data, data + x * y * 3);
	}
	stbi_image_free(data);
	return (data != nullptr);
}

int ChatName::GetSpriteYOffset() {
	std::string sprite_name = player.ch->GetSpriteName();
	if (!sprite_y_offsets.count(sprite_name)) {
		auto filename = FileFinder::FindImage("CharSet", sprite_name);
		if (filename == "") {
			return 0;
		}

		auto offset_array = std::array<int, 8>{ 0, 0, 0, 0, 0, 0, 0, 0 };

		const int CHARSET_WIDTH = 288;
		const int BASE_OFFSET = -13;
		const size_t RGB = 3;
		
		int width, height;
		std::vector<unsigned char> image;
		bool success = LoadSpriteImage(image, filename, width, height);
		if (!success) {
			sprite_y_offsets[sprite_name] = std::array<int, 8>(offset_array);
			return 0;
		}

		for (int hi = 0; hi < height / 128; ++hi) {
			for (int wi = 0; wi < width / 72; ++wi) {
				int i = (hi << 2) + wi;

				int start_x = wi * 72 + 24;
				int start_y = hi * 128 + 64;

				int trans_r = static_cast<int>(image[RGB * (start_y * CHARSET_WIDTH + start_x)]);
				int trans_g = static_cast<int>(image[RGB * (start_y * CHARSET_WIDTH + start_x) + 1]);
				int trans_b = static_cast<int>(image[RGB * (start_y * CHARSET_WIDTH + start_x) + 2]);

				bool offset_found = false;
				int y = start_y;

				for (; y < start_y + 32; ++y) {
					if (y == start_y + 15) {
						offset_found = true;
					} else {
						for (int x = start_x; x < start_x + 24; ++x) {
							size_t index = RGB * (y * CHARSET_WIDTH + x);
							int r = static_cast<int>(image[index]);
							int g = static_cast<int>(image[index + 1]);
							int b = static_cast<int>(image[index + 2]);
							if (r != trans_r || g != trans_g || b != trans_b) {
								offset_found = true;
								break;
							}
						}
					}
					
					if (offset_found) {
						break;
					}
				}

				if (offset_found) {
					offset_array[i] = BASE_OFFSET + (y - start_y);
				}
			}

			sprite_y_offsets[sprite_name] = std::array<int, 8>(offset_array);
		}
	}

	return sprite_y_offsets[sprite_name][player.ch->GetSpriteIndex()];
}
