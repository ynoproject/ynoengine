#ifndef EP_MULTIPLAYER_CONNECTION_H
#define EP_MULTIPLAYER_CONNECTION_H

#include <stdexcept>
#include <queue>
#include <memory>
#include <map>
#include <vector>
#include <functional>
#include <type_traits>
#include <string>
#include <charconv>

class MessageProcessingException : public std::runtime_error {
public:
	MessageProcessingException() : runtime_error("invalid message argument") {}
	MessageProcessingException(const char* w) : runtime_error(w) {}
	MessageProcessingException(const std::string& w) : runtime_error(std::move(w)) {}
};

class MultiplayerConnection {
public:
	constexpr static std::string_view PARAM_DELIM = "\uFFFF";
	constexpr static std::string_view MSG_DELIM = "\uFFFE";

	MultiplayerConnection() = default;
	MultiplayerConnection(const MultiplayerConnection&) = delete;
	MultiplayerConnection(MultiplayerConnection&&) = default;
	MultiplayerConnection& operator=(const MultiplayerConnection&) = delete;
	MultiplayerConnection& operator=(MultiplayerConnection&&) = default;

	using ParameterList = std::vector<std::string_view>;

	enum class Action {
		PASS,
		STOP,
	};

	class Packet {
	public:
		Packet() : valid(true) {}
		void Cancel() { valid = false; }
		bool IsValid() const { return valid; }
		virtual ~Packet() = default;
	protected:
		bool valid;
	};

	class C2SPacket : public Packet {
	public:
		virtual ~C2SPacket() = default;
		virtual std::string ToBytes() const = 0;

		C2SPacket(std::string _name) : m_name(std::move(_name)) {}
		std::string_view GetName() const { return m_name; }

		static std::string Sanitize(std::string_view param);

		static std::string ToString(const char* x) { return ToString(std::string_view(x)); }
		static std::string ToString(int x) { return std::to_string(x); }
		static std::string ToString(bool x) { return x ? "1" : "0"; }
		static std::string ToString(std::string_view v) { return Sanitize(v); }

		template<typename... Args>
		std::string Build(Args... args) const {
			std::string prev {m_name};
			AppendPartial(prev, args...);
			return prev;
		}

		template<typename T>
		static void AppendPartial(std::string& s, T t) {
			s += PARAM_DELIM;
			s += ToString(t);
		}

		template<typename T, typename... Args>
		static void AppendPartial(std::string& s, T t, Args... args) {
			s += PARAM_DELIM;
			s += ToString(t);
			AppendPartial(s, args...);
		}
	protected:
		std::string m_name;
	};

	class S2CPacket : public Packet {
	public:
		virtual ~S2CPacket() = default;

		template<typename T>
		static T Decode(std::string_view s);

		template<>
		int Decode(std::string_view s) {
			int r;
			auto e = std::from_chars(s.data(), s.data() + s.size(), r);
			if (e.ec != std::errc())
				throw MessageProcessingException("Decoding int");
			return r;
		}

		template<>
		bool Decode(std::string_view s) {
			if (s == "1")
				return true;
			if (s == "0")
				return true;
			throw MessageProcessingException("Decoding bool");
		}
	};

	void SendPacket(const C2SPacket& p);
	template<typename T, typename... Args>
	void SendPacketAsync(Args... args) {
		m_queue.emplace(new T(args...));
	}

	virtual void Open(std::string_view uri) = 0;
	virtual void Close() {}

	virtual void Send(std::string_view data) = 0;
	virtual void FlushQueue();

	template<typename M, typename = std::enable_if_t<std::conjunction_v<
		std::is_convertible<M, S2CPacket>,
		std::is_constructible<M, const ParameterList&>
	>>>
	void RegisterHandler(std::string_view name, std::function<void (M&)> h) {
		handlers.emplace(name, [h] (const ParameterList& args) {
			M pack {args};
			std::invoke(h, pack);
		});
	}

	void RegisterUnconditionalHandler(std::function<Action (std::string_view, const ParameterList&)> h) {
		unconditional_handlers.emplace_back([h] (std::string_view name, const ParameterList& args) {
			return std::invoke(h, name, args);
		});
	}

	enum class SystemMessage {
		OPEN,
		CLOSE,
		_PLACEHOLDER,
	};
	using SystemMessageHandler = std::function<void (MultiplayerConnection&)>;
	void RegisterSystemHandler(SystemMessage m, SystemMessageHandler h);

	void Dispatch(std::string_view name, ParameterList args = ParameterList());
	void DispatchUnconditional(std::string_view name, ParameterList args = ParameterList());

	bool IsConnected() const { return connected; }

	virtual ~MultiplayerConnection() = default;

	void SetKey(std::string k) { key = std::move(k); }
	std::string_view GetKey() const { return key; }

	static std::vector<std::string_view> Split(std::string_view src, std::string_view delim = PARAM_DELIM);

protected:
	bool connected;
	std::queue<std::unique_ptr<C2SPacket>> m_queue;

	void SetConnected(bool v) { connected = v; }
	void DispatchSystem(SystemMessage m);

	std::map<std::string, std::function<void (const ParameterList&)>> handlers;
	std::vector<std::function<Action (std::string_view, const ParameterList&)>>
		unconditional_handlers;
	SystemMessageHandler sys_handlers[static_cast<size_t>(SystemMessage::_PLACEHOLDER)];

	std::string key;
};

#endif

