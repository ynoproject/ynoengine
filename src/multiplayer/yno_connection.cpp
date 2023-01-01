#include "yno_connection.h"
#include <emscripten/websocket.h>
#include "../external/TinySHA1.hpp"

struct YNOConnection::IMPL {
	EMSCRIPTEN_WEBSOCKET_T socket;
	uint32_t msg_count;
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
		_this->DispatchSystem(
			event->code == 1028 ?
			SystemMessage::EXIT :
			SystemMessage::CLOSE
		);
		return EM_TRUE;
	}
	static EM_BOOL onmessage(int eventType, const EmscriptenWebSocketMessageEvent *event, void *userData) {
		auto _this = static_cast<YNOConnection*>(userData);
		// IMPORTANT!! numBytes is always one byte larger than the actual length
		// so the actual length is numBytes - 1
		std::string_view cstr(reinterpret_cast<const char*>(event->data),
				event->numBytes - 1);
		std::vector<std::string_view> mstrs = Split(cstr, Multiplayer::Packet::MSG_DELIM);
		for (auto& mstr : mstrs) {
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

const size_t YNOConnection::MAX_QUEUE_SIZE{ 4088 };

YNOConnection::YNOConnection() : impl(new IMPL) {
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

template<typename T>
std::string_view as_bytes(const T& v) {
	static_assert(sizeof(v) % 2 == 0, "Unsupported numeric type");
	return std::string_view(
		reinterpret_cast<const char*>(&v),
		sizeof(v)
	);
}

// poor method to test current endian
// need improvement
bool is_big_endian() {
	union endian_tester {
		uint16_t num;
		char layout[2];
	};

	endian_tester t;
	t.num = 1;
	return t.layout[0] == 0;
}

std::string reverse_endian(std::string src) {
	if (src.size() % 2 == 1)
		std::terminate();

	size_t it1{0}, it2{src.size() - 1}, itend{src.size() / 2};
	while (it1 != itend) {
		std::swap(src[it1], src[it2]);
		++it1;
		--it2;
	}
	return src;
}

template<typename T>
std::string as_big_endian_bytes(T v) {
	auto r = as_bytes<T>(v);
	std::string sr{r.data(), r.size()};
	if (is_big_endian())
		return sr;
	else
		return reverse_endian(sr);
}

std::string calculate_header(uint32_t key, uint32_t count, std::string_view msg) {
	std::string hashmsg{get_secret()};
	hashmsg += as_big_endian_bytes(key);
	hashmsg += as_big_endian_bytes(count);
	hashmsg += msg;

	sha1::SHA1 digest;
	uint32_t digest_result[5];
	digest.processBytes(hashmsg.data(), hashmsg.size());
	digest.getDigest(digest_result);

	std::string r{as_big_endian_bytes(digest_result[0])};
	r += as_big_endian_bytes(count);
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
		return (v != "sr") == include;
	};

	bool include = false;
	while (!m_queue.empty()) {
		std::string bulk;
		while (!m_queue.empty()) {
			auto& e = m_queue.front();
			if (namecmp(e->GetName(), include))
				break;
			auto data = e->ToBytes();
			// send before overflow
			if (bulk.size() + data.size() > MAX_QUEUE_SIZE) {
				Send(bulk);
				bulk.clear();
			}
			if (!bulk.empty())
				bulk += Multiplayer::Packet::MSG_DELIM;
			bulk += data;
			m_queue.pop();
		}
		if (!bulk.empty())
			Send(bulk);
		include = !include;
	}
}
