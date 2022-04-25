#include "yno_connection.h"
#include <emscripten/websocket.h>
#include "TinySHA1.hpp"

struct YNOConnection::IMPL {
	EMSCRIPTEN_WEBSOCKET_T socket;
	size_t msg_count;
	bool closed;

	static EM_BOOL onopen(int eventType, const EmscriptenWebSocketOpenEvent *event, void *userData) {
		auto _this = static_cast<YNOConnection*>(userData);
		_this->SetConnected(true);
		_this->DispatchSystem(SystemMessage::OPEN);
		return EM_TRUE;
	}
	static EM_BOOL onclose(int eventType, const EmscriptenWebSocketCloseEvent *event, void *userData) {
		auto _this = static_cast<YNOConnection*>(userData);
		_this->SetConnected(false);
		_this->DispatchSystem(SystemMessage::CLOSE);
		return EM_TRUE;
	}
	static EM_BOOL onmessage(int eventType, const EmscriptenWebSocketMessageEvent *event, void *userData) {
		auto _this = static_cast<YNOConnection*>(userData);
		if (event->isText) {
			// IMPORTANT!! numBytes is always one byte larger than the actual length
			// so the actual length is numBytes - 1
			std::string_view mstr(reinterpret_cast<const char*>(event->data),
					event->numBytes - 1);
			auto p = mstr.find(Multiplayer::Packet::PARAM_DELIM);
			if (p == mstr.npos) {
				/*
				Usually npos is the maximum value of size_t.
				Adding to it is undefined behavior.
				If it returns end iterator instead of npos, the if statement is
				duplicated code because the statement in else clause will handle it.
				*/
				_this->Dispatch(mstr);
			} else {
				auto namestr = mstr.substr(0, p);
				auto argstr = mstr.substr(p + Multiplayer::Packet::PARAM_DELIM.size());
				_this->Dispatch(namestr, Split(argstr));
			}
		}
		return EM_TRUE;
	}
};

YNOConnection::YNOConnection() : impl(new IMPL) {
	impl->msg_count = 0;
	impl->closed = true;
}

YNOConnection::YNOConnection(YNOConnection&& o)
	: Connection(std::move(o)), impl(std::move(o.impl)) {
	emscripten_websocket_set_onopen_callback(impl->socket, this, IMPL::onopen);
	emscripten_websocket_set_onclose_callback(impl->socket, this, IMPL::onclose);
	emscripten_websocket_set_onmessage_callback(impl->socket, this, IMPL::onmessage);
}
YNOConnection& YNOConnection::operator=(YNOConnection&& o) {
	Connection::operator=(std::move(o));
	if (this != &o) {
		Close();
		impl = std::move(o.impl);
		emscripten_websocket_set_onopen_callback(impl->socket, this, IMPL::onopen);
		emscripten_websocket_set_onclose_callback(impl->socket, this, IMPL::onclose);
		emscripten_websocket_set_onmessage_callback(impl->socket, this, IMPL::onmessage);
	}
	return *this;
}

YNOConnection::~YNOConnection() {
	if (impl)
		Close();
}

void YNOConnection::Open(std::string_view uri) {
	if (!impl->closed) {
		Close();
	}

	std::string s {uri};
	EmscriptenWebSocketCreateAttributes ws_attrs = {
		s.data(),
		"binary",
		EM_TRUE,
	};
	impl->socket = emscripten_websocket_new(&ws_attrs);
	impl->closed = false;
	emscripten_websocket_set_onopen_callback(impl->socket, this, IMPL::onopen);
	emscripten_websocket_set_onclose_callback(impl->socket, this, IMPL::onclose);
	emscripten_websocket_set_onmessage_callback(impl->socket, this, IMPL::onmessage);
}

void YNOConnection::Close() {
	Multiplayer::Connection::Close();
	if (impl->closed)
		return;
	impl->closed = true;
	// strange bug:
	// calling with (impl->socket, 1005, "any reason") raises exceptions
	// might be an emscripten bug
	emscripten_websocket_close(impl->socket, 0, nullptr);
	emscripten_websocket_delete(impl->socket);
}

static std::string_view get_secret() { return ""; }

std::string calculate_header(std::string_view key, size_t count, std::string_view msg) {
	char counter[7];
	snprintf(counter, 7, "%06zu", count);

	std::string hashmsg{key};
	hashmsg += get_secret();
	hashmsg += counter;
	hashmsg += msg;

	sha1::SHA1 digest;
	uint32_t digest_result[5];
	digest.processBytes(hashmsg.data(), hashmsg.size());
	digest.getDigest(digest_result);

	char signature[9];
	snprintf(signature, 9, "%08x", digest_result[0]);

	std::string r{signature};
	r += counter;
	return r;
}

void YNOConnection::Send(std::string_view data) {
	if (!IsConnected())
		return;
	unsigned short ready;
	emscripten_websocket_get_ready_state(impl->socket, &ready);
	if (ready == 1) { // OPEN
		++impl->msg_count;
		auto sendmsg = calculate_header(GetKey(), impl->msg_count, data);
		sendmsg += data;
		emscripten_websocket_send_binary(impl->socket, sendmsg.data(), sendmsg.size());
	}
}

void YNOConnection::FlushQueue() {
	auto namecmp = [] (std::string_view v, bool include) {
		return (v != "name") == include;
	};

	bool include = false;
	while (!m_queue.empty()) {
		std::string bulk;
		while (!m_queue.empty()) {
			auto& e = m_queue.front();
			if (namecmp(e->GetName(), include))
				break;
			if (!bulk.empty())
				bulk += Multiplayer::Packet::MSG_DELIM;
			bulk += e->ToBytes();
			m_queue.pop();
		}
		if (!bulk.empty())
			Send(bulk);
		include = !include;
	}
}

