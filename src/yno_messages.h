#ifndef EP_YNO_MESSAGES_H
#define EP_YNO_MESSAGES_H

#include "multiplayer_connection.h"
#include <memory>
#include <lcf/rpg/sound.h>
#include "game_pictures.h"

namespace YNO_Messages {
namespace S2C {

}
namespace C2S {
	using C2SPacket = MultiplayerConnection::C2SPacket;
	class MainPlayerPosPacket : public C2SPacket {
	public:
		MainPlayerPosPacket(int _x, int _y) : x(_x), y(_y) {}
		std::string ToBytes() const override { return Build("m", x, y); }
	protected:
		int x, y;
	};

	class FacingPacket : public C2SPacket {
	public:
		FacingPacket(int _d) : d(_d) {}
		std::string ToBytes() const override { return Build("f", d); }
	protected:
		int d;
	};

	class SpeedPacket : public C2SPacket {
	public:
		SpeedPacket(int _spd) : spd(_spd) {}
		std::string ToBytes() const override { return Build("spd", spd); }
	protected:
		int spd;
	};

	class SpritePacket : public C2SPacket {
	public:
		SpritePacket(std::string _n, int _i) : name(_n), index(_i) {}
		std::string ToBytes() const override { return Build("spr", name, index); }
	protected:
		std::string name;
		int index;
	};

	class NamePacket : public C2SPacket {
	public:
		NamePacket(std::string _n) : name(std::move(_n)) {}
		std::string ToBytes() const override { return Build("name", name); }
	protected:
		std::string name;
	};

	class SEPacket : public C2SPacket {
	public:
		SEPacket(lcf::rpg::Sound _d) : snd(std::move(_d)) {}
		std::string ToBytes() const override { return Build("se", snd.name, snd.volume, snd.tempo, snd.balance); }
	protected:
		lcf::rpg::Sound snd;
	};

	class SysNamePacket : public C2SPacket {
	public:
		SysNamePacket(std::string _s) : s(std::move(_s)) {}
		std::string ToBytes() const override { return Build("sys", s); }
	protected:
		std::string s;
	};

	class PicturePacket : public C2SPacket {
	public:
		PicturePacket(int _pic_id, Game_Pictures::Params& _p,
				int _mx, int _my,
				int _panx, int _pany)
			: pic_id(_pic_id), p(_p),
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
			: PicturePacket(_pid, p_show, _mx, _my, _px, _py), p_show(std::move(_p)) {}
		std::string ToBytes() const override {
			std::string r {"sp"};
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
			: PicturePacket(_pid, p_move, _mx, _my, _px, _py), p_move(std::move(_p)) {}
		std::string ToBytes() const override {
			std::string r {"mp"};
			PicturePacket::Append(r);
			AppendPartial(r, p_move.duration);
			return r;
		}
	protected:
		Game_Pictures::MoveParams p_move;
	};

	class ErasePicturePacket : public C2SPacket {
	public:
		ErasePicturePacket(int _pid) : pic_id(_pid) {}
		std::string ToBytes() const override { return Build("rp", pic_id); }
	protected:
		int pic_id;
	};

	class ChatPacket : public C2SPacket {
	public:
		ChatPacket(std::string _sys, std::string _msg)
			: sys(std::move(_sys)), msg(std::move(_msg)) {}
		std::string ToBytes() const override { return Build("say", sys, msg); }
	protected:
		std::string sys, msg;
	};

	class GlobalChatPacket : public C2SPacket {
	public:
		GlobalChatPacket(std::string _mid,
				std::string _pmid,
				std::string _plocs,
				std::string _sys,
				std::string _msg) : map_id(std::move(_mid)),
		prev_map_id(std::move(_pmid)),
		prev_locations(std::move(_plocs)),
		sys(std::move(_sys)),
		msg(std::move(_msg)) {}
		std::string ToBytes() const override {
			return Build("gsay", map_id, prev_map_id, prev_locations, sys, msg);
		}
	protected:
		std::string map_id, prev_map_id, prev_locations, sys, msg;
	};

}
}

#endif

