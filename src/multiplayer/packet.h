#ifndef EP_MULTIPLAYER_PACKET_H
#define EP_MULTIPLAYER_PACKET_H

#include <sstream>
#include <string>
#include <charconv>
#include <stdexcept>
#include <vector>

namespace Multiplayer {

class Packet {
public:
	constexpr static std::string_view PARAM_DELIM = "\uFFFF";
	constexpr static std::string_view MSG_DELIM = "\uFFFE";

	Packet() {}
	virtual ~Packet() = default;
protected:
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
	static std::string ToString(const std::vector<std::string>& params) {
		std::ostringstream out;

		for (auto it = params.cbegin(); it != params.cend(); ++it) {
			if (it != params.cbegin()) out << PARAM_DELIM;
			out << *it;
		}

		return out.str();
	}

	template<typename... Args>
	std::string Build(Args... args) const {
		std::string prev {m_name};
		AppendPartial(prev, args...);
		return prev;
	}

	static void AppendPartial(std::string& s) {}

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

namespace S2CPacket__detail {
	template<typename T>
	T Decode(std::string_view s);

	template<>
	inline int Decode<int>(std::string_view s) {
		int r;
		auto e = std::from_chars(s.data(), s.data() + s.size(), r);
		return r;
	}

	template<>
	inline bool Decode<bool>(std::string_view s) {
		return s == "1";
	}
}

class S2CPacket : public Packet {
public:
	virtual ~S2CPacket() = default;

	template<typename T>
	static T Decode(std::string_view s) {
		return S2CPacket__detail::Decode<T>(s);
	}

};

}
#endif
