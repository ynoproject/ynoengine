#ifndef EP_CHAT_MULTIPLAYER_H
#define EP_CHAT_MULTIPLAYER_H

#include <string>

namespace Chat_Multiplayer {
	void tryCreateChatWindow();
	void gotMessage(std::string msg);
	void focus();
	void processInputs();
}

#endif