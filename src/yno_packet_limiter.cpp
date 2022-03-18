#include "yno_packet_limiter.h"
#include "output.h"
#include "web_api.h"
#include "game_multiplayer.h"

using namespace YNO;

const std::set<std::string_view> PacketLimiter::whitelist {
	"s", "m", "c", "d", "f", "spd", "spr", "sys", "rp", "name"
};

// max packet allowed in 100ms
constexpr size_t THRESHOLD = 150;

PacketLimiter::Action PacketLimiter::OnReceive(std::string_view name, const Multiplayer::S2CPacket& p) {
	if (whitelist.find(name) != whitelist.end())
		return Action::NONE;

	++receive_count;

	using namespace std::chrono;
	auto currtime = Game_Clock::now();
	auto dur = currtime - last_received_time;
	last_received_time = currtime;
	// how many packets are allowed to pass
	auto passed_count = duration_cast<milliseconds>(dur).count() * THRESHOLD / 100;
	if (passed_count > receive_count)
		receive_count = 0;
	else
		receive_count -= passed_count;

	if (receive_count > THRESHOLD) {
		Output::Debug("Max packet limit exceeded");
		Web_API::ShowToastMessage("floodDetected", "important");
		Game_Multiplayer::GetSettingFlags().Set(Game_Multiplayer::Option::SINGLE_PLAYER, true);
		Game_Multiplayer::Quit();
		Web_API::UpdateConnectionStatus(3);
		Web_API::ReceiveInputFeedback(1);
		return Action::DROP;
	}
	return Action::NONE;
}

