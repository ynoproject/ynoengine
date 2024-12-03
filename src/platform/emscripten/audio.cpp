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

#include <emscripten/em_asm.h>
#ifdef SUPPORT_AUDIO
#include <csignal>
#include "audio.h"
#include "output.h"
#include "baseui.h"
#include <emscripten/atomic.h>
#include <emscripten/wasm_worker.h>

static thread_local bool main_thread = true;
static int request_render = 0;

static bool audioCallback(
	int /*numInputs*/, const AudioSampleFrame */*inputs*/,
	int /*numOutputs*/, AudioSampleFrame *outputs, int /*numParams*/,
	const AudioParamFrame */*params*/, void*)
{
	auto* audio = dynamic_cast<EmscriptenAudio*>(&DisplayUi->GetAudio());
	if (audio)
		audio->PullSamples(outputs->data, outputs->numberOfChannels);
	else
		return false;
	return request_render == 0;
}

static void* workerThread(void*) {
	main_thread = false;

	while (emscripten_atomic_wait_u32(&request_render, 0, ATOMICS_WAIT_DURATION_INFINITE) == ATOMICS_WAIT_OK) {
		auto* audio = dynamic_cast<EmscriptenAudio*>(&DisplayUi->GetAudio());
		if (audio)
			audio->PushSamples();
		else
			break;
	}

	Output::DebugStr("Worker thread exited");
	return nullptr;
}

void EmscriptenAudio::PushSamples() {
	LockMutex();
	Decode(scratch.data(), scratch.size() * sizeof(scratch[0]));
	UnlockMutex();
	buffer.Write(scratch.data(), scratch.size());
}

void EmscriptenAudio::PullSamples(float* outputs, int channels) {
	if (buffer.Read(lscratch.data(), quantum_size * 2)) {
		for (int ch = 0; ch < channels; ++ch) {
			for (int i = 0; i < quantum_size; ++i) {
				outputs[ch * quantum_size + i] = lscratch[i * channels + ch];
			}
		}
	} else {
		// For some reason we have to manually zero out the samples here
		lcf::Span<float> out(outputs, quantum_size * channels);
		std::fill(out.begin(), out.end(), '\0');
	}
	// Have the buffer maintain a head start as long as there is space
	if (buffer.AvailableToWrite() >= scratch.size())
		emscripten_atomic_notify(&request_render, 1);
}

// // main logic done

EmscriptenAudio::EmscriptenAudio(const Game_ConfigAudio& cfg) : GenericAudio(cfg) {
	request_render = 0;
	emscripten_lock_init(&lock);
	pthread_create(&worker, nullptr, workerThread, nullptr);

	audioContext = emscripten_create_audio_context(nullptr);
	int frequency = MAIN_THREAD_EM_ASM_INT({
		const context = Module.audioContext = emscriptenGetAudioObject($0);
		return context.sampleRate;
	}, audioContext);
	SetFormat(frequency, AudioDecoder::Format::F32, 2);

	float latency = MAIN_THREAD_EM_ASM_DOUBLE({ return Module.audioContext.baseLatency || 0.01 });
	buffer_size = latency * frequency * 2;
	if (scratch.size() != buffer_size)
		scratch.resize(buffer_size);
	if (lscratch.size() != buffer_size)
		lscratch.resize(buffer_size);

	static uint8_t audioStack[512 * 1024];

	emscripten_start_wasm_audio_worklet_thread_async(audioContext, audioStack, sizeof(audioStack), [](EMSCRIPTEN_WEBAUDIO_T audioContext, bool success, void*) {
	if (!success) return;

	WebAudioWorkletProcessorCreateOptions opts { "easyrpg-audio" };
	emscripten_create_wasm_audio_worklet_processor_async(audioContext, &opts, [](EMSCRIPTEN_WEBAUDIO_T audioContext, bool success, void*) {
	if (!success) return;

	int outputChannelCounts[] { 2 };
	EmscriptenAudioWorkletNodeCreateOptions opts { 0, 1, outputChannelCounts };
	auto worklet = emscripten_create_wasm_audio_worklet_node(audioContext, "easyrpg-audio", &opts, audioCallback, nullptr);
	MAIN_THREAD_EM_ASM({
		emscriptenGetAudioObject($0).connect(emscriptenGetAudioObject($1).destination);
		document.body.addEventListener("click", function resumeContext() {
			const context = emscriptenGetAudioObject($1);
			if (!context) {
				document.body.removeEventListener("click", resumeContext);
				return false;
			}
			if (context.state === 'suspended') {
				context.resume();
			}
			return false;
		});
	}, worklet, audioContext);

	}, nullptr);
	}, nullptr);
}

EmscriptenAudio::~EmscriptenAudio() {
	pthread_kill(worker, SIGINT);
	emscripten_destroy_audio_context(audioContext);
	request_render = 1;
}

static emscripten_lock_t lock;

void EmscriptenAudio::LockMutex() const {
	if (main_thread)
		emscripten_lock_busyspin_wait_acquire(&lock, 1);
	else
		emscripten_lock_wait_acquire(&lock, 1);
}
void EmscriptenAudio::UnlockMutex() const {
	emscripten_lock_release(&lock);
}

template<typename T, size_t capacity>
bool EmscriptenAudio::RingBuffer<T, capacity>::Write(const T* data, size_t count) {
	if (count == 0) return true;

	size_t wpos = wcur.load(std::memory_order_acquire);
	size_t rpos = rcur.load(std::memory_order_relaxed);

	if (is_full.load(std::memory_order_acquire)) {
		return false;
	}

	size_t available_to_write = AvailableToWrite();
	if (count > available_to_write) {
		return false; // Not enough space to write all data
	}

	for (size_t i = 0; i < count; ++i) {
		buffer[(wpos + i) % capacity] = data[i];
	}

	wcur.store((wpos + count) % capacity, std::memory_order_release);

	if (AvailableToWrite() == 0) {
		is_full.store(true, std::memory_order_release);
	}
	return true;
}

template<typename T, size_t capacity>
bool EmscriptenAudio::RingBuffer<T, capacity>::Read(T* output, size_t count) {
	if (count == 0) return true;

	size_t missing = 0;

	size_t wpos = wcur.load(std::memory_order_relaxed);
	size_t rpos = rcur.load(std::memory_order_acquire);

	if (rpos == wpos && !is_full.load(std::memory_order_acquire)) {
		return false; // No data to read
	}

	size_t available_to_read = (wpos - rpos + capacity) % capacity;
	if (count > available_to_read) {
		missing = count - available_to_read;
		lcf::Span<T> span(output, count);
		std::fill(&span[available_to_read], span.end(), '\0');
		count = available_to_read;
	}

	for (size_t i = 0; i < count; ++i) {
		output[i] = buffer[(rpos + i) % capacity];
	}

	rcur.store((rpos + count) % capacity, std::memory_order_release);

	is_full.store(false, std::memory_order_release);

	return true;
}

template<typename T, size_t capacity>
size_t EmscriptenAudio::RingBuffer<T, capacity>::AvailableToRead() const noexcept {
	size_t wpos = wcur.load(std::memory_order_relaxed);
	size_t rpos = rcur.load(std::memory_order_relaxed);

	if (wpos == rpos && !is_full.load(std::memory_order_relaxed)) {
		return 0; // No data to read
	}

	if (wpos < rpos)
		return wpos + capacity - rpos;

	return wpos - rpos;
}

#endif
