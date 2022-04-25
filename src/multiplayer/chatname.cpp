#include "chatname.h"
#include "../cache.h"
#include "../font.h"
#include "../drawable_mgr.h"

ChatName::ChatName(int id, PlayerOther& player, std::string nickname)
	:player(player),
	nickname(std::move(nickname)),
	Drawable(Priority_Frame + (id << 8)) {
	DrawableMgr::Register(this);
}

void ChatName::Draw(Bitmap& dst) {
	auto sprite = player.sprite.get();
	if (!Game_Multiplayer::GetSettingFlags().Get(Game_Multiplayer::Option::ENABLE_NICKS) || nickname.empty() || !sprite) {
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

	int x = player.ch->GetScreenX() - nick_img->GetWidth() / 2 - 1;
	int y = player.ch->GetScreenY() - TILE_SIZE * 2;
	dst.Blit(x, y, *nick_img, nick_img->GetRect(), Opacity::Opaque());
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
};


