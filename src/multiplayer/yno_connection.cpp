#include "yno_connection.h"
#include "output.h"
#ifdef __EMSCRIPTEN__
#  include <emscripten/websocket.h>
#else
#  include <libwebsockets.h>
#  include <thread>
#endif
#include "../external/TinySHA1.hpp"

struct YNOConnection::IMPL {
#ifdef __EMSCRIPTEN__
	EMSCRIPTEN_WEBSOCKET_T socket;
#else
	lws_ss_handle* handle;
	lws_context* cx;
#endif

	uint32_t msg_count;
	bool closed;

#ifdef __EMSCRIPTEN__
	static bool onopen(int eventType, const EmscriptenWebSocketOpenEvent *event, void *userData) {
		auto _this = static_cast<YNOConnection*>(userData);
		_this->SetConnected(true);
		_this->DispatchSystem(SystemMessage::OPEN);
		return true;
	}
	static bool onclose(int eventType, const EmscriptenWebSocketCloseEvent *event, void *userData) {
		auto _this = static_cast<YNOConnection*>(userData);
		_this->SetConnected(false);
		_this->DispatchSystem(
			event->code == 1028 ?
			SystemMessage::EXIT :
			SystemMessage::CLOSE
		);
		return true;
	}
	static bool onmessage(int eventType, const EmscriptenWebSocketMessageEvent *event, void *userData) {
		auto _this = static_cast<YNOConnection*>(userData);
		// IMPORTANT!! numBytes is always one byte larger than the actual length
		// so the actual length is numBytes - 1

		// NOTE: that extra byte is just in text mode, and it does not exist in binary mode
		if (event->isText) {
			return false;
		}
		std::string_view cstr(reinterpret_cast<const char*>(event->data), event->numBytes);
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
		return true;
	}
#else
	typedef struct {
		struct lws_ss_handle* ss;
		void* opaque_data;

		// custom logic fields begin
		YNOConnection* conn;
		// custom logic fields end

		lws_sorted_usec_list_t sul;
		int count;
		bool due;
	} yno_socket_t;

	static lws_ss_state_return_t yno_socket_rx(void* userData, const uint8_t* in, size_t len, int flags)
	{
		auto* self = static_cast<yno_socket_t*>(userData);
		auto* h = self->ss;

		std::string_view cstr(reinterpret_cast<const char*>(in), len);
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
				self->conn->Dispatch(mstr);
			} else {
				auto namestr = mstr.substr(0, p);
				auto argstr = mstr.substr(p + Multiplayer::Packet::PARAM_DELIM.size());
				self->conn->Dispatch(namestr, Split(argstr));
			}
		}
		return LWSSSSRET_OK;
	}

	static constexpr uint RATE_US = 50000;
	static constexpr uint PKT_SIZE = 80;

	static void yno_socket_txcb(struct lws_sorted_usec_list* sul)
	{
		auto* self = lws_container_of(sul, yno_socket_t, sul);

		self->due = true;
		if (lws_ss_request_tx(self->ss) != LWSSSSRET_OK) {
			// TODO: The fuck you expect me to do?
		}

		lws_sul_schedule(lws_ss_get_context(self->ss), 0, &self->sul, yno_socket_txcb, RATE_US);
	}

	static lws_ss_state_return_t yno_socket_tx(void *userobj, lws_ss_tx_ordinal_t ord, uint8_t *buf, size_t *len, int *flags)
	{
		auto* self = static_cast<yno_socket_t *>(userobj);
		if (!self->due)
			return LWSSSSRET_TX_DONT_SEND;

		self->due = false;

		if (lws_get_random(lws_ss_get_context(self->ss), buf, PKT_SIZE) != PKT_SIZE)
			return LWSSSSRET_TX_DONT_SEND;

		*len = PKT_SIZE;
		*flags = LWSSS_FLAG_SOM | LWSSS_FLAG_EOM;

		self->count++;

		lws_sul_schedule(lws_ss_get_context(self->ss), 0, &self->sul, yno_socket_txcb, RATE_US);

		return LWSSSSRET_OK;
	}


	static lws_ss_state_return_t yno_socket_state(void* userData, void* h_src, lws_ss_constate_t state, lws_ss_tx_ordinal_t sck)
	{
		auto* self = static_cast<yno_socket_t*>(userData);
		switch (state) {
		case LWSSSCS_CREATING:
			return lws_ss_request_tx(self->ss);
		case LWSSSCS_CONNECTED:
			self->conn->SetConnected(true);
			self->conn->DispatchSystem(SystemMessage::OPEN);
			lws_sul_schedule(lws_ss_get_context(self->ss), 0, &self->sul, yno_socket_txcb, RATE_US);
			break;
		case LWSSSCS_DISCONNECTED:
			self->conn->SetConnected(false);
			self->conn->DispatchSystem(SystemMessage::CLOSE);
			lws_sul_cancel(&self->sul);
			break;
		default:
			break;
		}
		return LWSSSSRET_OK;
	}

	static constexpr lws_ss_info_t ssi {
		.streamtype = "yno",
		.user_alloc = sizeof(yno_socket_t),
		.handle_offset = offsetof(yno_socket_t, ss),
		.opaque_user_data_offset = offsetof(yno_socket_t, opaque_data),
		.rx = yno_socket_rx,
		.tx = yno_socket_tx,
		.state = yno_socket_state,
	};
#endif

#ifdef __EMSCRIPTEN__
	static void set_callbacks(int socket, void* userData) {
		emscripten_websocket_set_onopen_callback(socket, userData, onopen);
		emscripten_websocket_set_onclose_callback(socket, userData, onclose);
		emscripten_websocket_set_onmessage_callback(socket, userData, onmessage);
	}
#endif
	void initWs(lws_ss_policy* policy) {
#ifndef __EMSCRIPTEN__
		lws_set_log_level(LLL_USER | LLL_ERR | LLL_WARN | LLL_NOTICE, nullptr);
		lws_context_creation_info info {
			.protocols = lws_sspc_protocols,
			.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT,
			.fd_limit_per_thread = 1 + 6 + 1,
			.port = CONTEXT_PORT_NO_LISTEN,
			.pss_policies = policy,
		};
		cx = lws_create_context(&info);
		if (!cx) Output::ErrorStr("lws_create_context failed");

		if (lws_ss_create(cx, 0, &ssi, nullptr, &handle, nullptr, nullptr))
			Output::ErrorStr("lws_ss_create failed");

		std::thread event_thread([](lws_context* cx) {
			while (!lws_service(cx, 0));
		}, cx);
	}
#endif
};

const size_t YNOConnection::MAX_QUEUE_SIZE{ 4088 };



YNOConnection::YNOConnection() : impl(new IMPL) {
	impl->closed = true;
}

YNOConnection::YNOConnection(YNOConnection&& o)
	: Connection(std::move(o)), impl(std::move(o.impl)) {
#ifdef __EMSCRIPTEN__
	IMPL::set_callbacks(impl->socket, this);
#endif
}
YNOConnection& YNOConnection::operator=(YNOConnection&& o) {
	Connection::operator=(std::move(o));
	if (this != &o) {
		Close();
		impl = std::move(o.impl);
#ifdef __EMSCRIPTEN__
		IMPL::set_callbacks(impl->socket, this);
#endif
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

#ifdef __EMSCRIPTEN__
	std::string s {uri};
	EmscriptenWebSocketCreateAttributes ws_attrs = {
		s.data(),
		"binary",
		EM_TRUE,
	};
	impl->socket = emscripten_websocket_new(&ws_attrs);
	impl->closed = false;
	IMPL::set_callbacks(impl->socket, this);
#else
	const lws_ss_policy_t yno_policy {
		.streamtype = "yno",
		.endpoint = uri.data(),
		.port = 443,
		.protocol = (uint8_t)(uri.find("wss") == 0 ? 1 : 0),
	};
#endif
	impl->initWs();
}

void YNOConnection::Close() {
	Multiplayer::Connection::Close();
	if (impl->closed)
		return;
	impl->closed = true;
#ifdef __EMSCRIPTEN__
	// strange bug:
	// calling with (impl->socket, 1005, "any reason") raises exceptions
	// might be an emscripten bug
	emscripten_websocket_close(impl->socket, 0, nullptr);
	emscripten_websocket_delete(impl->socket);
#else
	if (impl->handle) lws_ss_destroy(&impl->handle);
#endif
}

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
		return src;

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

const unsigned char psk[] = {};

std::string calculate_header(uint32_t key, uint32_t count, std::string_view msg) {
	std::string hashmsg{as_bytes(psk)};
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
#ifdef __EMSCRIPTEN__
	emscripten_websocket_get_ready_state(impl->socket, &ready);
#else
	ready = impl->ready;
#endif
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
