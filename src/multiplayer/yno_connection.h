#ifndef EP_YNO_CONNECTION_H
#define EP_YNO_CONNECTION_H

#include "multiplayer_connection.h"

class YNOConnection : public Multiplayer::Connection {
public:
	YNOConnection();
	YNOConnection(YNOConnection&&);
	YNOConnection& operator=(YNOConnection&&);
	~YNOConnection();

	void Open(std::string_view uri) override;
	void Close() override;
	void Send(std::string_view data) override;
	void FlushQueue() override;
protected:
	struct IMPL;
	std::unique_ptr<IMPL> impl;
};

#endif
