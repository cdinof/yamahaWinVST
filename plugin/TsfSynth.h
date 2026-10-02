// Sintetizador SoundFont embutido (TinySoundFont, domínio público).
#pragma once

#include <cstddef>
#include <string>

struct tsf;

namespace yamaha {

class TsfSynth {
 public:
  TsfSynth() = default;
  ~TsfSynth();

  TsfSynth(const TsfSynth&) = delete;
  TsfSynth& operator=(const TsfSynth&) = delete;

  /// Carrega um .sf2 do disco. Retorna false se falhar.
  bool loadFile(const std::string& path);
  /// Carrega um .sf2 da memória (para embutir um SoundFont no binário).
  bool loadMemory(const void* data, size_t size);

  void setSampleRate(float sampleRate);
  void setVolume(float v01);  // 0..1
  void setReverb(float v01);  // 0..1
  void setChorus(float v01);  // 0..1

  void noteOn(int channel0Based, int note, int velocity);
  void noteOff(int channel0Based, int note);
  void allOff();
  void setChannelVolume(int channel0Based, int value); // CC7

  /// Renderiza [frames] amostras estéreo entrelaçadas (L,R,L,R...).
  void renderInterleaved(float* out, int frames);

  bool hasSoundFont() const { return soundFont_ != nullptr; }

 private:
  tsf* soundFont_ = nullptr;
  float sampleRate_ = 44100.0f;
  float volume_ = 0.8f;
};

} // namespace yamaha
