#include "web_api.h"
#include "emscripten/emscripten.h"
#include "output.h"

using namespace Web_API;

std::string Web_API::GetSocketURL() {
	return reinterpret_cast<char*>(EM_ASM_INT({
	  var ws = Module.wsUrl;
	  var len = lengthBytesUTF8(ws)+1;
	  var wasm_str = _malloc(len);
	  stringToUTF8(ws, wasm_str, len);
	  return wasm_str;
	}));
}

void Web_API::OnLoadMap(std::string_view name) {
	EM_ASM({
		onLoadMap(UTF8ToString($0));
	}, name.data(), name.size());
}

void Web_API::OnRoomSwitch() {
	EM_ASM({
		onRoomSwitch();
	});
}

void Web_API::SyncPlayerData(std::string_view uuid, int rank, int account_bin, std::string_view badge, const int medals[5], int id) {
	EM_ASM({
		syncPlayerData(UTF8ToString($0, $1), $2, $3, UTF8ToString($4, $5), [ $6, $7, $8, $9, $10 ], $11);
	}, uuid.data(), uuid.size(), rank, account_bin, badge.data(), badge.size(), medals[0], medals[1], medals[2], medals[3], medals[4], id);
}

void Web_API::OnPlayerDisconnect(int id) {
	EM_ASM({
		onPlayerDisconnected($0);
	}, id);
}

void Web_API::OnPlayerNameUpdated(std::string_view name, int id) {
	EM_ASM({
		onPlayerConnectedOrUpdated("", UTF8ToString($0, $1), $2);
	}, name.data(), name.size(), id);
}

void Web_API::OnPlayerSystemUpdated(std::string_view system, int id) {
	EM_ASM({
		onPlayerConnectedOrUpdated(UTF8ToString($0, $1), "", $2);
	}, system.data(), system.size(), id);
}

void Web_API::UpdateConnectionStatus(int status) {
	EM_ASM({
		onUpdateConnectionStatus($0);
	}, status);
}

void Web_API::ReceiveInputFeedback(int s) {
	EM_ASM({
		onReceiveInputFeedback($0);
	}, s);
}

void Web_API::NametagModeUpdated(int m) {
	EM_ASM({
		onNametagModeUpdated($0);
	}, m);
}

void Web_API::OnPlayerSpriteUpdated(std::string_view name, int index, int id) {
	EM_ASM({
		onPlayerSpriteUpdated(UTF8ToString($0, $1), $2, $3);
	}, name.data(), name.size(), index, id);
}

void Web_API::OnPlayerTeleported(int map_id, int x, int y) {
		EM_ASM({
		onPlayerTeleported($0, $1, $2);
	}, map_id, x, y);
}

void Web_API::OnUpdateSystemGraphic(std::string_view sys) {
	EM_ASM({
		onUpdateSystemGraphic(UTF8ToString($0, $1));
	}, sys.data(), sys.size());
}

void Web_API::OnRequestBadgeUpdate() {
	EM_ASM({
		onBadgeUpdateRequested();
	});
}

void Web_API::ShowToastMessage(std::string_view msg, std::string_view icon) {
	EM_ASM({
		showClientToastMessage(UTF8ToString($0, $1), UTF8ToString($2, $3));
	}, msg.data(), msg.size(), icon.data(), icon.size());
}

bool Web_API::ShouldConnectPlayer(std::string_view uuid) {
	int result = EM_ASM_INT({
		return shouldConnectPlayer(UTF8ToString($0, $1)) ? 1 : 0;
	}, uuid.data(), uuid.size());
	return result == 1;
}
