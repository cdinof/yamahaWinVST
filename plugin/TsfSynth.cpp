#include "TsfSynth.h"

#define TSF_IMPLEMENTATION
#include "../third_party/tsf.h"

#include <cmath>
#include <cstring>

namespace yamaha {

TsfSynth::~TsfSynth() {
  if (soundFont_) tsf_close(soundFont_);
}

void TsfSynth::applyOutputMode() {
  if (!soundFont_) return;
  // tsf_set_output define o modo de saída E a taxa de amostragem.
  // A profundidade (float) é definida pela função de render usada
  // (tsf_render_float).
  tsf_set_output(soundFont_, TSF_STEREO_INTERLEAVED,
                 static_cast<int>(sampleRate_ + 0.5f), 0.0f);
  tsf_set_volume(soundFont_, volume_);
}

bool TsfSynth::loadFile(const std::string& path) {
  tsf* s = tsf_load_filename(path.c_str());
  if (!s) return false;
  if (soundFont_) tsf_close(soundFont_);
  soundFont_ = s;
  applyOutputMode();
  setupChannels();
  return true;
}

bool TsfSynth::loadMemory(const void* data, size_t size) {
  tsf* s = tsf_load_memory(data, static_cast<int>(size));
  if (!s) return false;
  if (soundFont_) tsf_close(soundFont_);
  soundFont_ = s;
  applyOutputMode();
  setupChannels();
  return true;
}

/// Distribui os presets do SoundFont pelos canais (aproximação GM) e marca o
/// canal 10 (índice 9) como bateria. Os Program Changes do estilo corrigem
/// isso depois, via setProgram().
void TsfSynth::setupChannels() {
  if (!soundFont_) return;
  for (int ch = 0; ch < 16; ++ch) {
    const int isDrums = (ch == 9) ? 1 : 0;
    tsf_channel_set_presetnumber(soundFont_, ch, isDrums ? 0 : ch, isDrums);
  }
}

void TsfSynth::setSampleRate(float sampleRate) {
  sampleRate_ = sampleRate;
  applyOutputMode();
}

void TsfSynth::setVolume(float v01) {
  volume_ = v01 < 0.0f ? 0.0f : (v01 > 1.0f ? 1.0f : v01);
  if (soundFont_) tsf_set_volume(soundFont_, volume_);
}

void TsfSynth::setReverb(float) {
  // TinySoundFont não implementa reverb/chorus (os geradores de efeito do SF2
  // não são suportados). O knob fica reservado para uma implementação futura.
}

void TsfSynth::setProgram(int channel0Based, int program) {
  if (!soundFont_) return;
  int ch = channel0Based;
  if (ch < 0) ch = 0;
  if (ch > 15) ch = 15;
  int p = program;
  if (p < 0) p = 0;
  if (p > 127) p = 127;
  const int isDrums = (ch == 9) ? 1 : 0;
  tsf_channel_set_presetnumber(soundFont_, ch, p, isDrums);
}

void TsfSynth::noteOn(int channel0Based, int note, int velocity) {
  if (!soundFont_) return;
  int ch = channel0Based;
  if (ch < 0) ch = 0;
  if (ch > 15) ch = 15;
  int n = note;
  if (n < 0) n = 0;
  if (n > 127) n = 127;
  int v = velocity;
  if (v < 1) v = 1;
  if (v > 127) v = 127;
  tsf_channel_note_on(soundFont_, ch, n, static_cast<float>(v) / 127.0f);
}

void TsfSynth::noteOff(int channel0Based, int note) {
  if (!soundFont_) return;
  int ch = channel0Based;
  if (ch < 0) ch = 0;
  if (ch > 15) ch = 15;
  if (note < 0 || note > 127) return;
  tsf_channel_note_off(soundFont_, ch, note);
}

void TsfSynth::allOff() {
  if (!soundFont_) return;
  for (int ch = 0; ch < 16; ++ch) {
    tsf_channel_note_off_all(soundFont_, ch);
    tsf_channel_sounds_off_all(soundFont_, ch);
  }
}

void TsfSynth::setChannelVolume(int channel0Based, int value) {
  if (!soundFont_) return;
  int ch = channel0Based;
  if (ch < 0) ch = 0;
  if (ch > 15) ch = 15;
  int v = value;
  if (v < 0) v = 0;
  if (v > 127) v = 127;
  tsf_channel_midi_control(soundFont_, ch, 7, v);
}

void TsfSynth::renderInterleaved(float* out, int frames) {
  if (!soundFont_ || frames <= 0) {
    std::memset(out, 0, sizeof(float) * static_cast<size_t>(frames) * 2);
    return;
  }
  tsf_render_float(soundFont_, out, frames, 0);
  // Segurança contra estouro.
  const int n = frames * 2;
  for (int i = 0; i < n; ++i) {
    if (out[i] > 1.0f) out[i] = 1.0f;
    else if (out[i] < -1.0f) out[i] = -1.0f;
  }
}

} // namespace yamaha
