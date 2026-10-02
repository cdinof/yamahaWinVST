// Processador do plugin: recebe MIDI do host, detecta acordes, toca o estilo.
#pragma once

#include <JuceHeader.h>

#include <atomic>
#include <memory>
#include <vector>

#include "../engine/ChordDetector.h"
#include "../engine/StylePlayer.h"
#include "../engine/StyParser.h"
#include "TsfSynth.h"

namespace yamaha {

// ---------------------------------------------------------------------------
// Estado compartilhado entre o processador (thread de áudio) e a UI.
// A UI só escreve; o áudio só lê. A CriticalSection é usada apenas na troca de
// estilo (evento raro) — nunca no caminho de áudio.
// ---------------------------------------------------------------------------
class SharedState {
 public:
  juce::CriticalSection lock;
  YamahaStylePtr pendingStyle;  // estilo carregado pela UI
  bool styleChanged = false;
  std::string statusMessage = "Pronto";
};

class YamahaArrangerProcessor : public juce::AudioProcessor {
 public:
  YamahaArrangerProcessor();
  ~YamahaArrangerProcessor() override;

  // ---- AudioProcessor -----------------------------------------------------
  void prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock) override;
  void releaseResources() override;
  void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
  using juce::AudioProcessor::processBlock;

  const juce::String getName() const override { return "Yamaha Arranger"; }
  bool acceptsMidi() const override { return true; }
  bool producesMidi() const override { return false; }
  double getTailLengthSeconds() const override { return 0.0; }

  int getNumPrograms() override { return 1; }
  int getCurrentProgram() override { return 0; }
  void setCurrentProgram(int) override {}
  const juce::String getProgramName(int) override { return {}; }
  void changeProgramName(int, const juce::String&) override {}

  bool hasEditor() const override { return true; }
  juce::AudioProcessorEditor* createEditor() override;

  void getStateInformation(juce::MemoryBlock&) override;
  void setStateInformation(const void*, int) override;

  // ---- Acesso para a UI ---------------------------------------------------
  StylePlayer& player() { return player_; }
  TsfSynth& synth() { return synth_; }
  SharedState& shared() { return shared_; }

  void loadStyleFile(const juce::File& file);
  void loadSoundFontFile(const juce::File& file);
  void selectSection(const juce::String& name, bool immediate);
  void setPlaying(bool shouldPlay);
  void toggleMute(int channel0Based);
  void panic();
  void setVolume(float v);
  void setReverb(float v);
  void nudgeTempo(double steps);
  void setSplitNote(int note);

  juce::String currentChordLabel() const;
  bool hasStyle() const { return player_.hasStyle(); }

 private:
  void takePendingStyle();
  void handleIncomingMidi(const juce::MidiBuffer& midi, int numSamples,
                          double startPpq, double endPpq);

  StylePlayer player_;
  TsfSynth synth_;
  SharedState shared_;

  std::atomic<bool> playing_{false};
  std::atomic<int> splitNote_{54};  // abaixo de F#3 = zona de acordes
  std::atomic<float> volume_{0.8f};
  std::atomic<float> reverb_{0.25f};
  std::vector<int> heldChordNotes_;  // notas seguradas na zona de acordes
  double lastPpq_ = 0.0;
  bool ppqValid_ = false;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(YamahaArrangerProcessor)
};

} // namespace yamaha
