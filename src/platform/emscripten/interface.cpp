/*
 * This file is part of EasyRPG Player.
 *
 * EasyRPG Player is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * EasyRPG Player is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with EasyRPG Player. If not, see <http://www.gnu.org/licenses/>.
 */

#include "interface.h"

#include <emscripten.h>
#include <emscripten/bind.h>
#include <lcf/lsd/reader.h>
#include <sstream>

#include "async_handler.h"
#include "baseui.h"
#include "filefinder.h"
#include "player.h"
#include "scene_save.h"
#include "output.h"

#include <string>
#include "../../multiplayer/game_multiplayer.h"
#include "../../game_player.h"
#include "../../audio.h"
#include "../../web_api.h"

void Emscripten_Interface::Reset() {
	Player::reset_flag = true;
}

bool Emscripten_Interface::DownloadSavegame(int slot) {
	auto fs = FileFinder::Save();
	std::string name = Scene_Save::GetSaveFilename(fs, slot);
	auto is = fs.OpenInputStream(name);
	if (!is) {
		return false;
	}
	auto save_buffer = Utils::ReadStream(is);
	std::string filename = std::get<1>(FileFinder::GetPathAndFilename(name));
	EM_ASM_ARGS({
		Module.api_private.download_js($0, $1, $2);
	}, save_buffer.data(), save_buffer.size(), filename.c_str());
	return true;
}

void Emscripten_Interface::UploadSavegame(int slot) {
	EM_ASM_INT({
		Module.api_private.uploadSavegame_js($0);
	}, slot);
}

void Emscripten_Interface::RefreshScene() {
	Scene::instance->Refresh();
}

void Emscripten_Interface::TakeScreenshot() {
	static int index = 0;
	std::ostringstream os;
	Output::TakeScreenshot(os);
	std::string screenshot = os.str();
	std::string filename = "screenshot_" + std::to_string(index++) + ".png";
	EM_ASM_ARGS({
		Module.api_private.download_js($0, $1, $2);
	}, screenshot.data(), screenshot.size(), filename.c_str());
}

bool Emscripten_Interface_Private::UploadSavegameStep2(int slot, int buffer_addr, int size) {
	auto fs = FileFinder::Save();
	std::string name = Scene_Save::GetSaveFilename(fs, slot);

	std::istream is(new Filesystem_Stream::InputMemoryStreamBufView(lcf::Span<uint8_t>(reinterpret_cast<uint8_t*>(buffer_addr), size)));

	if (!lcf::LSD_Reader::Load(is)) {
		Output::Warning("Selected file is not a valid savegame");
		return false;
	}

	{
		auto os = fs.OpenOutputStream(name);
		if (!os)
			return false;
		os.write(reinterpret_cast<char*>(buffer_addr), size);
	}

	AsyncHandler::SaveFilesystem();

	return true;
}

// YNOproject

std::array<int, 2> Emscripten_Interface::GetPlayerCoords() {
	auto& player = *Main_Data::game_player;

	int x = player.GetX();
	int y = player.GetY();

	return {x, y};
}

void Emscripten_Interface::SetLanguage(std::string lang) {
	Player::translation.SelectLanguage(lang);
}

void Emscripten_Interface::SessionReady() {
	auto& i = GMI();
	if (i.connection.IsConnected())
		i.connection.Close(); // if SessionReady is called, the websocket must already be closed
	i.session_active = true;
	if (i.room_id != -1)
		i.Connect(i.room_id);
}

void Emscripten_Interface::TogglePlayerSounds() {
	auto& f = Game_Multiplayer::Instance().settings.enable_sounds;
	f = !f;
	Web_API::ReceiveInputFeedback(1);
}

void Emscripten_Interface::ToggleMute() {
	auto& f = Game_Multiplayer::Instance().settings.mute_audio;
	f = !f;
	Web_API::ReceiveInputFeedback(2);
}

void Emscripten_Interface::SetSoundVolume(int volume) {
	Audio().SE_SetGlobalVolume(volume);
}

void Emscripten_Interface::SetMusicVolume(int volume) {
	Audio().BGM_SetGlobalVolume(volume);
}

void Emscripten_Interface::SetNametagMode(int mode) {
	Game_Multiplayer::Instance().SetNametagMode(mode);
	Web_API::NametagModeUpdated(mode);
}

void Emscripten_Interface::SetSessionToken(std::string t) {
	auto& i = Game_Multiplayer::Instance();
	i.session_token.assign(t);
}

bool Emscripten_Interface::ResetCanvas() {
	DisplayUi.reset();
	DisplayUi = BaseUi::CreateUi(Player::screen_width, Player::screen_height, Player::ParseCommandLine());
	return DisplayUi != nullptr;
}

// Binding code
EMSCRIPTEN_BINDINGS(player_interface) {
	emscripten::class_<Emscripten_Interface>("api")
		.class_function("requestReset", &Emscripten_Interface::Reset)
		.class_function("downloadSavegame", &Emscripten_Interface::DownloadSavegame)
		.class_function("uploadSavegame", &Emscripten_Interface::UploadSavegame)
		.class_function("refreshScene", &Emscripten_Interface::RefreshScene)
		.class_function("takeScreenshot", &Emscripten_Interface::TakeScreenshot)

		.class_function("getPlayerCoords", &Emscripten_Interface::GetPlayerCoords)
		.class_function("setLanguage", &Emscripten_Interface::SetLanguage, emscripten::allow_raw_pointers())
		.class_function("sessionReady", &Emscripten_Interface::SessionReady)
		.class_function("togglePlayerSounds", &Emscripten_Interface::TogglePlayerSounds)
		.class_function("toggleMute", &Emscripten_Interface::ToggleMute)
		.class_function("setSoundVolume", &Emscripten_Interface::SetSoundVolume)
		.class_function("setMusicVolume", &Emscripten_Interface::SetMusicVolume)
		.class_function("setNametagMode", &Emscripten_Interface::SetNametagMode)
		.class_function("setSessionToken", &Emscripten_Interface::SetSessionToken, emscripten::allow_raw_pointers())
		.class_function("resetCanvas", &Emscripten_Interface::ResetCanvas)
	;

	emscripten::class_<Emscripten_Interface_Private>("api_private")
		.class_function("uploadSavegameStep2", &Emscripten_Interface_Private::UploadSavegameStep2)
	;

	emscripten::value_array<std::array<int, 2>>("array_int_2")
		.element(emscripten::index<0>())
		.element(emscripten::index<1>())
	;
}
