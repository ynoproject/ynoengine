#ifndef EP_YNO_MESSAGES_H
#define EP_YNO_MESSAGES_H

#include "multiplayer_connection.h"
#include "multiplayer_packet.h"
#include <memory>
#include <lcf/rpg/sound.h>
#include "game_pictures.h"

namespace YNO_Messages {
namespace S2C {
	using S2CPacket = Multiplayer::S2CPacket;
	using PL = Multiplayer::Connection::ParameterList;
	class SyncPlayerDataPacket : public S2CPacket {
	public:
		SyncPlayerDataPacket(const PL& v)
			: host_id(Decode<int>(v.at(0))),
			key(v.at(1)),
			uuid(v.at(2)),
			rank(Decode<int>(v.at(3))) {}

		const int host_id;
		const std::string key;
		const std::string uuid;
		const int rank;
	};

	class GlobalChatPacket : public S2CPacket {
	public:
		GlobalChatPacket(const PL& v)
			: uuid(v.at(0)),
			name(v.at(1)),
			sys(v.at(2)),
			rank(Decode<int>(v.at(3))),
			map_id(v.at(4)),
			prev_map_id(v.at(5)),
			prev_locations(v.at(6)),
			msg(v.at(7)) {}

		const std::string uuid;
		const std::string name;
		const std::string sys;
		const int rank;
		const std::string map_id;
		const std::string prev_map_id;
		const std::string prev_locations;
		const std::string msg;
	};

	class PartyChatPacket : public S2CPacket {
	public:
		PartyChatPacket(const PL& v)
			: uuid(v.at(0)),
			msg(v.at(1)) {}

		const std::string uuid;
		const std::string msg;
	};

	class PlayerPacket : public S2CPacket {
	public:
		PlayerPacket(std::string_view _id) : id(Decode<int>(_id)) {}
		bool IsCurrent(int host_id) const { return id == host_id; }
		const int id;
	};

	class ConnectPacket : public PlayerPacket {
	public:
		ConnectPacket(const PL& v)
			: PlayerPacket(v.at(0)),
			uuid(v.at(1)),
			rank(Decode<int>(v.at(2))) {}
		const std::string uuid;
		const int rank;
	};

	class DisconnectPacket : public PlayerPacket {
	public:
		DisconnectPacket(const PL& v)
			: PlayerPacket(v.at(0)) {}
	};

	class ChatPacket : public PlayerPacket {
	public:
		ChatPacket(const PL& v)
			: PlayerPacket(v.at(0)),
			msg(v.at(1)) {}
		const std::string msg;
	};

	class MovePacket : public PlayerPacket {
	public:
		MovePacket(const PL& v)
			: PlayerPacket(v.at(0)),
			x(Decode<int>(v.at(1))),
			y(Decode<int>(v.at(2))) {}
		const int x, y;
	};

	class FacingPacket : public PlayerPacket {
	public:
		FacingPacket(const PL& v)
			: PlayerPacket(v.at(0)),
			facing(Decode<int>(v.at(1))) {}
		const int facing;
	};
	
	class SpeedPacket : public PlayerPacket {
	public:
		SpeedPacket(const PL& v)
			: PlayerPacket(v.at(0)),
			speed(Decode<int>(v.at(1))) {}
		const int speed;
	};

	class SpritePacket : public PlayerPacket {
	public:
		SpritePacket(const PL& v)
			: PlayerPacket(v.at(0)),
			name(v.at(1)),
			index(Decode<int>(v.at(2))) {}
		const std::string name;
		const int index;
	};

	class FlashPacket : public PlayerPacket {
	public:
		FlashPacket(const PL& v)
			: PlayerPacket(v.at(0)),
			r(Decode<int>(v.at(1))), g(Decode<int>(v.at(2))), b(Decode<int>(v.at(3))), p(Decode<int>(v.at(4))), f(Decode<int>(v.at(5))) {}
		const int r;
		const int g;
		const int b;
		const int p;
		const int f;
	};

	class TonePacket : public PlayerPacket {
	public:
		TonePacket(const PL& v)
			: PlayerPacket(v.at(0)),
			red(Decode<int>(v.at(1))), green(Decode<int>(v.at(2))), blue(Decode<int>(v.at(3))), gray(Decode<int>(v.at(4))) {}
		const int red;
		const int green;
		const int blue;
		const int gray;
	};

	class SystemPacket : public PlayerPacket {
	public:
		SystemPacket(const PL& v)
			: PlayerPacket(v.at(0)),
			name(v.at(1)) {}
		const std::string name;
	};

	class SEPacket : public PlayerPacket {
	public:
		static lcf::rpg::Sound BuildSound(const PL& v) {
			lcf::rpg::Sound s;
			s.name = v.at(1);
			s.volume = Decode<int>(v.at(2));
			s.tempo = Decode<int>(v.at(3));
			s.balance = Decode<int>(v.at(4));
			return s;
		}
		SEPacket(const PL& v)
			: PlayerPacket(v.at(0)),
			snd(BuildSound(v)) {}
		const lcf::rpg::Sound snd;
	};

	class PicturePacket : public PlayerPacket {
	public:
		static void BuildParams(Game_Pictures::Params& p, const PL& v) {
			p.position_x = Decode<int>(v.at(2));
			p.position_y = Decode<int>(v.at(3));
			p.magnify = Decode<int>(v.at(8));
			p.top_trans = Decode<int>(v.at(9));
			p.bottom_trans = Decode<int>(v.at(10));
			p.red = Decode<int>(v.at(11));
			p.green = Decode<int>(v.at(12));
			p.blue = Decode<int>(v.at(13));
			p.saturation = Decode<int>(v.at(14));
			p.effect_mode = Decode<int>(v.at(15));
			p.effect_power = Decode<int>(v.at(16));
		}
		PicturePacket(Game_Pictures::Params& _params, const PL& v)
			: PlayerPacket(v.at(0)), params(_params),
			pic_id(Decode<int>(v.at(1))),
			map_x(Decode<int>(v.at(4))), map_y(Decode<int>(v.at(5))),
			pan_x(Decode<int>(v.at(6))), pan_y(Decode<int>(v.at(7))) {}
		Game_Pictures::Params& params;
		const int pic_id;
		int map_x, map_y;
		int pan_x, pan_y;
	};

	class ShowPicturePacket : public PicturePacket {
	public:
		Game_Pictures::ShowParams BuildParams(const PL& v) const {
			Game_Pictures::ShowParams p;
			PicturePacket::BuildParams(p, v);
			p.name = v.at(17);
			p.use_transparent_color = Decode<bool>(v.at(18));
			p.fixed_to_map = Decode<bool>(v.at(19));
			return p;
		}
		ShowPicturePacket(const PL& v)
			: PicturePacket(params, v),
			params(BuildParams(v)) {}
		Game_Pictures::ShowParams params;
	};

	class MovePicturePacket : public PicturePacket {
	public:
		Game_Pictures::MoveParams BuildParams(const PL& v) const {
			Game_Pictures::MoveParams p;
			PicturePacket::BuildParams(p, v);
			p.duration = Decode<int>(v.at(17));
			return p;
		}
		MovePicturePacket(const PL& v)
			: PicturePacket(params, v),
			params(BuildParams(v)) {}
		Game_Pictures::MoveParams params;
	};

	class ErasePicturePacket : public PlayerPacket {
	public:
		ErasePicturePacket(const PL& v)
			: PlayerPacket(v.at(0)), pic_id(Decode<int>(v.at(1))) {}
		const int pic_id;
	};

	class NamePacket : public PlayerPacket {
	public:
		NamePacket(const PL& v)
			: PlayerPacket(v.at(0)),
			name(v.at(1)) {}
		const std::string name;
	};

}
namespace C2S {
	using C2SPacket = Multiplayer::C2SPacket;
	class MainPlayerPosPacket : public C2SPacket {
	public:
		MainPlayerPosPacket(int _x, int _y) : C2SPacket("m"),
			x(_x), y(_y) {}
		std::string ToBytes() const override { return Build(x, y); }
	protected:
		int x, y;
	};

	class FacingPacket : public C2SPacket {
	public:
		FacingPacket(int _d) : C2SPacket("f"), d(_d) {}
		std::string ToBytes() const override { return Build(d); }
	protected:
		int d;
	};

	class SpeedPacket : public C2SPacket {
	public:
		SpeedPacket(int _spd) : C2SPacket("spd"), spd(_spd) {}
		std::string ToBytes() const override { return Build(spd); }
	protected:
		int spd;
	};

	class SpritePacket : public C2SPacket {
	public:
		SpritePacket(std::string _n, int _i) : C2SPacket("spr"),
			name(_n), index(_i) {}
		std::string ToBytes() const override { return Build(name, index); }
	protected:
		std::string name;
		int index;
	};

	class FlashPacket : public C2SPacket {
	public:
		FlashPacket(int _r, int _g, int _b, int _p, int _f) : C2SPacket("fl"),
			r(_r), g(_g), b(_b), p(_p), f(_f) {}
		std::string ToBytes() const override { return Build(r, g, b, p, f); }
	protected:
		int r;
		int g;
		int b;
		int p;
		int f;
	};

	class RepeatingFlashPacket : public C2SPacket {
	public:
		RepeatingFlashPacket(int _r, int _g, int _b, int _p, int _f) : C2SPacket("rfl"),
			r(_r), g(_g), b(_b), p(_p), f(_f) {}
		std::string ToBytes() const override { return Build(r, g, b, p, f); }
	protected:
		int r;
		int g;
		int b;
		int p;
		int f;
	};

	class RemoveRepeatingFlashPacket : public C2SPacket {
	public:
		RemoveRepeatingFlashPacket() : C2SPacket("rrfl"), {}
	};

	class TonePacket : public C2SPacket {
	public:
		TonePacket(int _red, int _green, int _blue, int _gray) : C2SPacket("t"),
			red(_red), green(_green), blue(_blue), gray(_gray) {}
		std::string ToBytes() const override { return Build(red, green, blue, gray); }
	protected:
		int red;
		int green;
		int blue;
		int gray;
	};

	class NamePacket : public C2SPacket {
	public:
		NamePacket(std::string _n) : C2SPacket("name"), name(std::move(_n)) {}
		std::string ToBytes() const override { return Build(name); }
	protected:
		std::string name;
	};

	class SEPacket : public C2SPacket {
	public:
		SEPacket(lcf::rpg::Sound _d) : C2SPacket("se"), snd(std::move(_d)) {}
		std::string ToBytes() const override { return Build(snd.name, snd.volume, snd.tempo, snd.balance); }
	protected:
		lcf::rpg::Sound snd;
	};

	class SysNamePacket : public C2SPacket {
	public:
		SysNamePacket(std::string _s) : C2SPacket("sys"), s(std::move(_s)) {}
		std::string ToBytes() const override { return Build(s); }
	protected:
		std::string s;
	};

	class PicturePacket : public C2SPacket {
	public:
		PicturePacket(std::string _name, int _pic_id, Game_Pictures::Params& _p,
				int _mx, int _my,
				int _panx, int _pany)
			: C2SPacket(std::move(_name)), pic_id(_pic_id), p(_p),
		map_x(_mx), map_y(_my),
		pan_x(_panx), pan_y(_pany) {}
		void Append(std::string& s) const {
			AppendPartial(s, pic_id, p.position_x, p.position_y,
					map_x, map_y, pan_x, pan_y,
					p.magnify, p.top_trans, p.bottom_trans,
					p.red, p.green, p.blue, p.saturation,
					p.effect_mode, p.effect_power);
		}
	protected:
		int pic_id;
		Game_Pictures::Params& p;
		int map_x, map_y;
		int pan_x, pan_y;
	};

	class ShowPicturePacket : public PicturePacket {
	public:
		ShowPicturePacket(int _pid, Game_Pictures::ShowParams _p,
				int _mx, int _my, int _px, int _py)
			: PicturePacket("ap", _pid, p_show, _mx, _my, _px, _py), p_show(std::move(_p)) {}
		std::string ToBytes() const override {
			std::string r {GetName()};
			PicturePacket::Append(r);
			AppendPartial(r, p_show.name, p_show.use_transparent_color, p_show.fixed_to_map);
			return r;
		}
	protected:
		Game_Pictures::ShowParams p_show;
	};

	class MovePicturePacket : public PicturePacket {
	public:
		MovePicturePacket(int _pid, Game_Pictures::MoveParams _p,
				int _mx, int _my, int _px, int _py)
			: PicturePacket("mp", _pid, p_move, _mx, _my, _px, _py), p_move(std::move(_p)) {}
		std::string ToBytes() const override {
			std::string r {GetName()};
			PicturePacket::Append(r);
			AppendPartial(r, p_move.duration);
			return r;
		}
	protected:
		Game_Pictures::MoveParams p_move;
	};

	class ErasePicturePacket : public C2SPacket {
	public:
		ErasePicturePacket(int _pid) : C2SPacket("rp"), pic_id(_pid) {}
		std::string ToBytes() const override { return Build(pic_id); }
	protected:
		int pic_id;
	};

	class ChatPacket : public C2SPacket {
	public:
		ChatPacket(std::string _msg) : C2SPacket("say"),
			msg(std::move(_msg)) {}
		std::string ToBytes() const override { return Build(msg); }
	protected:
		std::string msg;
	};

	class GlobalChatPacket : public C2SPacket {
	public:
		GlobalChatPacket(std::string _msg, int _enable_loc_bin) : C2SPacket("gsay"),
			msg(std::move(_msg)), enable_loc_bin(_enable_loc_bin) {}
		std::string ToBytes() const override { return Build(msg, enable_loc_bin); }
	protected:
		std::string msg;
		int enable_loc_bin;
	};

	class PartyChatPacket : public C2SPacket {
	public:
		PartyChatPacket(std::string _msg) : C2SPacket("psay"),
			msg(std::move(_msg)) {}
		std::string ToBytes() const override { return Build(msg); }
	protected:
		std::string msg;
	};

	class BanUserPacket : public C2SPacket {
	public:
		BanUserPacket(std::string _uuid) : C2SPacket("ban"),
			uuid(std::move(_uuid)) {}
		std::string ToBytes() const override { return Build(uuid); }
	protected:
		std::string uuid;
	};

}
}

#endif

