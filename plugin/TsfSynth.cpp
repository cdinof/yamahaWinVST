#include "TsfSynth.h"

#define TSF_IMPLEMENTATION
#include "../third_party/tsf.h"

#include <cmath>
#include <cstring>

namespace yamaha {

TsfSynth::~TsfSynth() {
  if (soundFont_) tsf_close(soundFont_);
}

bool TsfSynth::loadFile(const std::string& path) {
  tsf* s = tsf_load_filename(path.c_str());
  if (!s) return false;
  if (soundFont_) tsf_close(soundFont_);
  soundFont_ = s;
  tsf_set_output(soundFont_, TSF_STEREO_INTERLEAVED, TSF_FLOAT, 0);
  tsf_set_sample_rate(soundFont_, sampleRate_);
  tsf_set_volume(soundFont_, volume_);
  return true;
}

bool TsfSynth::loadMemory(const void* data, size_t size) {
  tsf* s = tsf_load_memory(data, static_cast<int>(size));
  if (!s) return false;
  if (soundFont_) tsf_close(soundFont_);
  soundFont_ = s;
  tsf_set_output(soundFont_, TSF_STEREO_INTERLEAVED, TSF_FLOAT, 0);
  tsf_set_sample_rate(soundFont_, sampleRate_);
  tsf_set_volume(soundFont_, volume_);
  return true;
}

void TsfSynth::setSampleRate(float sampleRate) {
  sampleRate_ = sampleRate;
  if (soundFont_) tsf_set_sample_rate(soundFont_, sampleRate);
}

void TsfSynth::setVolume(float v01) {
  volume_ = v01 < 0.0f ? 0.0f : (v01 > 1.0f ? 1.0f : v01);
  if (soundFont_) tsf_set_volume(soundFont_, volume_);
}

void TsfSynth::setReverb(float v01) {
  if (!soundFont_) return;
  tsf_reverb_setup(soundFont_, 0.3f, v01, 0.6f);
}

void TsfSynth::setChorus(float) {
  // TinySoundFont não implementa chorus; fica como no-op (reservado).
}

void TsfSynth::noteOn(int channel0Based, int note, int velocity) {
  if (!soundFont_) return;
  int ch = channel0Based;
  if (ch < 0) ch = 0;
  if (ch > 15) ch = 15;
  if (note < 0) note = 0;
  if (note > 127) note = 127;
  if (velocity < 1) velocity = 1;
  if (velocity > 127) velocity = 127;
  tsf_channel_midi_control(soundFont_, ch, 7, 100, 0); // volume do canal
  tsf_note_on(soundFont_, ch, static_cast<unsigned char>(note),
              static_cast<float>(velocity) / 127.0f);
}

void TsfSynth::noteOff(int channel0Based, int note) {
  if (!soundFont_) return;
  int ch = channel0Based;
  if (ch < 0) ch = 0;
  if (ch > 15) ch = 15;
  if (note < 0 || note > 127) return;
  tsf_note_off(soundFont_, ch, static_cast<unsigned char>(note));
}

void TsfSynth::allOff() {
  if (!soundFont_) return;
  for (int ch = 0; ch < 16; ++ch) tsf_channel_note_off_all(soundFont_, ch);
}

void TsfSynth::setChannelVolume(int channel0Based, int value) {
  if (!soundFont_) return;
  int ch = channel0Based;
  if (ch < 0) ch = 0;
  if (ch > 15) ch = 15;
  int v = value;
  if (v < 0) v = 0;
  if (v > 127) v = 127;
  tsf_channel_midi_control(soundFont_, ch, 7, static_cast<unsigned short>(v), 0);
}

void TsfSynth::renderInterleaved(float* out, int frames) {
  if (!soundFont_ || frames <= 0) {
    std::memset(out, 0, sizeof(float) * static_cast<size_t>(frames) * 2);
    return;
  }
  tsf_render_float(soundFont_, out, frames, 0);
  // Segurança: sem estouro.
  const int n = frames * 2;
  for (int i = 0; i < n; ++i) {
    if (out[i] > 1.0f) out[i] = 1.0f;
    else if (out[i] < -1.0f) out[i] = -1.0f;
  }
}

} // namespace yamaha
