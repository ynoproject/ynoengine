#include "chatname.h"
#include "../cache.h"
#include "../font.h"
#include "../drawable_mgr.h"
#include "../filefinder.h"
#include "../output.h"

extern "C" {
	#define STB_IMAGE_IMPLEMENTATION
	#include "../external/stb_image.h"
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

		Text::Draw(*nick_img, 0, GetSpriteYOffset(), *Font::Default(), *sys, 0, nick_trim);

		dirty = false;
	}

	if (!player.ch->IsSpriteHidden()) {
		int x = player.ch->GetScreenX() - nick_img->GetWidth() / 2 - 1;
		int y = player.ch->GetScreenY() - TILE_SIZE * 2;
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

bool ChatName::LoadSpriteImage(std::vector<unsigned char>& image, const std::string& filename) {
    int x, y, n;
    unsigned char* data = stbi_load(filename.c_str(), &x, &y, &n, 4);
    if (data != nullptr) {
			image = std::vector<unsigned char>(data, data + x * y * 4);
    }
    stbi_image_free(data);
    return (data != nullptr);
}

int ChatName::GetSpriteYOffset() {
	auto filename = FileFinder::MakePath("CharSet", player.ch->GetSpriteName());
	
	Output::Debug("Loading CharSet for Chat Name Y Offset: {}", filename);
	
	int ret;
	std::vector<unsigned char> image;
	bool success = LoadSpriteImage(image, filename);
	if (!success) {
			Output::Debug("Failed to load CharSet: {}", filename);
			return ret;
	}

	Output::Debug("Loaded CharSet: {}", filename);

	const int CHARSET_WIDTH = 288;
	const int BASE_OFFSET = -13;
	const size_t RGBA = 4;

	int startX = (player.ch->GetSpriteIndex() % 4) * 72 + 24;
	int startY = (player.ch->GetSpriteIndex() >> 2) * 128 + 64;

	for (int y = startY; y < startY + 32; ++y) {
		for (int x = startX; x < startX + 24; ++x) {
			size_t index = RGBA * (y * CHARSET_WIDTH + x);
			int alpha = static_cast<int>(image[index + 3]);
			if (alpha > 0) {
				return BASE_OFFSET + y;
			}
		}
	}

	return 0;
}