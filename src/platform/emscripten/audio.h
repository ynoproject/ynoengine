/*
 * This file is part of EasyRPG Player.
 *
 * EasyRPG Player is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * EasyRPG Player is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with EasyRPG Player. If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef EP_AUDIO_EMSCRIPTEN_H
#define EP_AUDIO_EMSCRIPTEN_H

#include "audio_generic.h"
#include "game_config.h"
#include <atomic>
#include <emscripten/em_asm.h>
#include <emscripten/wasm_worker.h>
#include <emscripten/webaudio.h>

namespace {
	EMSCRIPTEN_WEBAUDIO_T audioContext = 0;
}

class EmscriptenAudio : public GenericAudio {
public:
	static constexpr size_t quantum_size = 128;
	// // the default "Callback Buffer Size" on Chromium and Firefox for two channels
	// static constexpr size_t buffer_size = 480 * 2;
private:
	template <typename T, size_t capacity>
	struct RingBuffer {
		std::array<T, capacity> buffer;
		std::atomic<size_t> wcur = 0;
		std::atomic<size_t> rcur = 0;
		std::atomic<bool> is_full = false;

	public:
		explicit RingBuffer() noexcept = default;
		bool Write(const T* data, size_t count);
		bool Read(T* output, size_t count);
		size_t AvailableToRead() const noexcept;
		size_t AvailableToWrite() const noexcept {
			return capacity - AvailableToRead();
		}
	};

	pthread_t worker;
	mutable emscripten_lock_t lock;

	size_t buffer_size;
	std::vector<float> scratch;
	std::vector<float> lscratch;
	RingBuffer<float, 480 * 2 * 16> buffer;
public:
	EmscriptenAudio(const Game_ConfigAudio& cfg);
	~EmscriptenAudio();
	void LockMutex() const override;
	void UnlockMutex() const override;

	void PushSamples();
	void PullSamples(float* outputs, int channels);
	static bool Supported() {
		return MAIN_THREAD_EM_ASM_INT({ return !!globalThis.crossOriginIsolated }) != 0;
	}
};

#endif
