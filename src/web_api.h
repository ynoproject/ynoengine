#ifndef EP_WEB_API_H
#define EP_WEB_API_H

#include <string>

namespace Web_API {
	std::string GetSocketURL();
	void OnLoadMap(std::string_view name);
	void OnRoomSwitch();
	void SyncPlayerData(std::string_view uuid, int rank, int account_bin, std::string_view badge, const int medals[5], int id = -1);
	void OnChatMessageReceived(std::string_view msg, int id = -1);
	void OnPlayerDisconnect(int id);
	void OnPlayerNameUpdated(std::string_view name, int id);
	void OnPlayerSystemUpdated(std::string_view system, int id);
	void UpdateConnectionStatus(int status);
	void ReceiveInputFeedback(int s);
	void NametagModeUpdated(int m);
	void OnPlayerSpriteUpdated(std::string_view name, int index, int id = -1);
	void OnPlayerTeleported(int map_id, int x, int y);
	void OnUpdateSystemGraphic(std::string_view sys);
	void OnRequestBadgeUpdate();

	// possible values of msg and icon are in forest-orb
	// msg: localizedMessages.toast.client.*
	// icon: icon.js
	void ShowToastMessage(std::string_view msg, std::string_view icon);
}

#endif

