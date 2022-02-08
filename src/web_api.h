#ifndef EP_WEB_API_H
#define EP_WEB_API_H

#include <string>

namespace Web_API {
	void OnLoadMap(std::string_view name);
	std::string GetSocketURL();
	void OnChatMessageReceived(std::string_view sys, std::string_view msg);
	void OnGChatMessageReceived(std::string_view sys, std::string_view msg);
	void OnPlayerDisconnect(int id);
	void OnPlayerNameUpdated(std::string_view name, int id);
	void OnPlayerSystemUpdated(std::string_view system, int id);
	void UpdateConnectionStatus(int status);
	void ReceiveInputFeedback(int s);
	void OnPlayerSpriteUpdated(std::string_view name, int index, int id = -1);
	void OnUpdateSystemGraphic(std::string_view sys);
}

#endif

