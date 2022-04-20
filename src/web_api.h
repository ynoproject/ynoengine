#ifndef EP_WEB_API_H
#define EP_WEB_API_H

#include <string>

namespace Web_API {
	std::string GetSocketURL();
	void OnLoadMap(std::string_view name);
	void SyncPlayerData(std::string_view uuid, int rank, int account_bin, std::string_view badge, int id = -1);
	void SyncGlobalPlayerData(std::string_view uuid, std::string_view name, std::string_view sys, int rank, int account_bin, std::string_view badge);
	void OnChatMessageReceived(std::string_view msg, int id = -1);
	void OnGChatMessageReceived(std::string_view uuid, std::string_view map_id, std::string_view prev_map_id, std::string_view prev_locations, std::string_view msg);
	void OnPChatMessageReceived(std::string_view uuid, std::string_view msg);
	void OnPlayerDisconnect(int id);
	void OnPlayerNameUpdated(std::string_view name, int id);
	void OnPlayerSystemUpdated(std::string_view system, int id);
	void UpdateConnectionStatus(int status);
	void ReceiveInputFeedback(int s);
	void OnPlayerSpriteUpdated(std::string_view name, int index, int id = -1);
	void OnPlayerTeleported(int map_id, int x, int y);
	void OnUpdateSystemGraphic(std::string_view sys);

	// possible values of msg and icon are in forest-orb
	// msg: localizedMessages.toast.client.*
	// icon: icon.js
	void ShowToastMessage(std::string_view msg, std::string_view icon);
}

#endif

