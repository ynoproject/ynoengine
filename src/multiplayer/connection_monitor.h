#ifndef EP_CONNECTION_MONITOR_H
#define EP_CONNECTION_MONITOR_H

#include "multiplayer_packet.h"

class ConnectionMonitor {
public:
	enum class Action {
		NONE,
		DROP, // drop that message, without dispatching it
	};
	virtual Action OnReceive(std::string_view name, const Multiplayer::S2CPacket& p) = 0;
};

#endif

