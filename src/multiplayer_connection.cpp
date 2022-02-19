#include "multiplayer_connection.h"
#include <bitset>
#include "output.h"

void MultiplayerConnection::SendPacket(const C2SPacket& p) {
	Send(p.ToBytes());
}

void MultiplayerConnection::FlushQueue() {
	while (!m_queue.empty()) {
		auto& e = m_queue.front();
		Send(e->ToBytes());
		m_queue.pop();
	}
}

void MultiplayerConnection::Dispatch(std::string_view name, ParameterList args) {
	auto it = handlers.find(std::string(name));
	if (it != handlers.end()) {
		try {
			std::invoke(it->second, args);
		} catch (MessageProcessingException& e) {
			Output::Debug("Exception in processing: {}", e.what());
		} catch (std::out_of_range& e) {
			Output::Debug("Too few arguments: {}", e.what());
		}
	} else {
		DispatchUnconditional(name, args);
	}
}

void MultiplayerConnection::DispatchUnconditional(std::string_view name, ParameterList args) {
	for (auto h : unconditional_handlers) {
		auto a = std::invoke(h, name, args);
		if (a == Action::STOP)
			break;
	}
}

MultiplayerConnection::ParameterList MultiplayerConnection::Split(std::string_view src,
	std::string_view delim) {
	size_t p{}, p2{};
	std::vector<std::string_view> r;
	while ((p = src.find(delim, p)) != src.npos) {
		r.emplace_back(src.substr(p2, p - p2));
		p += delim.size();
		p2 = p;
	}
	if (p2 != src.length())
		r.emplace_back(src.substr(p2));
	return r;
}

constexpr std::string_view keywords[] = {
	MultiplayerConnection::PARAM_DELIM,
	MultiplayerConnection::MSG_DELIM,
};
constexpr size_t k_size = sizeof(keywords) / sizeof(std::string_view);

std::string MultiplayerConnection::C2SPacket::Sanitize(std::string_view param) {
	std::string r;
	r.reserve(param.size());
	std::bitset<k_size> searching_marks;
	size_t candidate_index{};
	for (size_t i = 0; i != param.size(); ++i) {
		if (candidate_index == 0) {
			bool found = false;
			for (size_t j = 0; j != k_size; ++j) {
				assert(!keywords[j].empty());
				if (keywords[j][0] == param[i]) {
					searching_marks.set(j);
					found = true;
				}
			}

			if (found)
				candidate_index = 1;
			else
				r += param[i];
		} else {
			bool found = false;
			bool match = false;
			for (size_t j = 0; j != k_size; ++j) {
				if (searching_marks.test(j)) {
					if (keywords[j][candidate_index] == param[i]) {
						found = true;
						if (keywords[j].size() == candidate_index + 1) {
							match = true;
							break;
						}
					} else {
						searching_marks.reset(j);
					}
				}
			}

			if (match) {
				candidate_index = 0;
			} else if (found) {
				++candidate_index;
			} else {
				r.append(param.substr(i - candidate_index, candidate_index + 1));
				candidate_index = 0;
			}
		}
	}
	if (candidate_index != 0)
		r.append(param.substr(param.length() - candidate_index));
	return r;
}

void MultiplayerConnection::RegisterSystemHandler(SystemMessage m, SystemMessageHandler h) {
	sys_handlers[static_cast<size_t>(m)] = h;
}

void MultiplayerConnection::DispatchSystem(SystemMessage m) {
	auto f = sys_handlers[static_cast<size_t>(m)];
	if (f)
		std::invoke(f, *this);
}

