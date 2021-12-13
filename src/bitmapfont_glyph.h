#ifndef EP_BITMAPFONT_GLYPH_H
#define EP_BITMAPFONT_GLYPH_H

#include <stdint.h>

struct BitmapFontGlyph {
	uint16_t code;
	size_t kerning;	// if glyph has fine-tuned kerning enabled, this value holds the width of the glyph plus its right side margin.
					// else, it holds 0 for HALF_WIDTH and 1 for FULL_WIDTH.
	uint16_t data[12];
	bool fineKerning = false; // fine-tuned kerning
	bool fullHeight = true; // switch used for tiny glyphs
};

constexpr bool operator<(const BitmapFontGlyph& lhs, uint32_t const code) {
	return lhs.code < code;
}

constexpr bool operator<(const BitmapFontGlyph& lhs, const BitmapFontGlyph& rhs) {
	return lhs.code < rhs.code;
}

constexpr const BitmapFontGlyph BITMAPFONT_REPLACEMENT_GLYPH = { 65533, true, { 96, 240, 504, 924, 1902, 3967, 4031, 1982, 1020, 440, 240, 96 } };

#endif
