// Sequenciador: toca uma seção do estilo reharmonizada, via MIDI OUT.
//
// Porte do StylePlayer do app Android, adaptado para ser dirigido pelo host:
// em vez de um Timer, o plugin chama advance() com a variação de PPQ do bloco.
#pragma once

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "Chord.h"
#include "StyleModel.h"

namespace yamaha {

class StylePlayer {
 public:
  /// Evento MIDI gerado, com offset em amostras dentro do bloco.
  struct MidiEv {
    int sampleOffset = 0;
    int channel = 0; // 0-based
    int status = 0;  // 0x80/0x90/0xB0/0xC0/0xE0
    int data1 = 0;
    int data2 = 0;
  };

  explicit StylePlayer(YamahaStylePtr style = nullptr) {
    loadStyle(std::move(style));
  }

  void loadStyle(YamahaStylePtr s);
  bool hasStyle() const { return style_ != nullptr; }

  void setChord(const Chord& c) { currentChord_ = c; }
  const Chord& chord() const { return currentChord_; }

  void setTempo(double bpm);
  double tempoBpm() const { return tempoBpm_; }

  void selectSection(const std::string& name, bool immediate = false);
  void play();
  void stop();
  bool isPlaying() const { return playing_; }

  /// Avanço por bloco: [startTick] e [endTick] são posições em PPQ (ticks de
  /// semínima) dentro do bloco; [numSamples] serve para calcular os offsets.
  void advance(int numSamples, double startTick, double endTick,
               std::vector<MidiEv>& out);

  // ---- estado para a UI ----------------------------------------------------
  const std::string& currentSectionName() const { return currentSectionName_; }
  std::vector<std::string> startQueue() const { return startQueue_; }
  int currentMeasure() const { return measure_; }
  int currentBeat() const { return beat_; }
  std::vector<int> drumChannels1Based() const;
  SoundStandard standard() const {
    return style_ ? style_->standard : SoundStandard::unknown;
  }
  const YamahaStyle* style() const { return style_.get(); }

  /// Canais (0-based) silenciados pelo usuário.
  std::set<int> mutedChannels;
  /// Canais usados pelo estilo (0-based, já no destino).
  std::set<int> activeChannels;

 private:
  YamahaStylePtr style_;
  const StyleSection* section_ = nullptr;
  const CasmSegment* casm_ = nullptr;
  Chord currentChord_{0, ChordQuality::maj};
  double tempoBpm_ = 120.0;

  int cursor_ = 0;   // próximo evento da seção
  double tick_ = 0;  // posição dentro da seção (ticks)
  double tickAccum_ = 0;
  bool playing_ = false;

  std::string queuedSection_;
  bool queuedImmediate_ = false;
  std::vector<std::string> playSequence_;
  std::vector<std::string> startQueue_;
  std::string lastMainName_ = "Main A";
  std::string currentSectionName_ = "-";
  int measure_ = 1, beat_ = 1;

  std::map<int, int> sounding_; // (canal<<8 | nota) -> nota enviada

  std::set<int> drumChannels_;
  std::map<int, int> bankMsb_;
  std::map<int, int> srcToDest_;

  void detectDrumChannels();
  void detectActiveChannels();
  int destOf(int srcChannel) const;
  int ticksPerBar() const;
  void activateSection(const std::string& name);
  void prepareSection(const std::string& name);
  void clearStartQueue();
  void notifyPlayQueue();
  std::string nextSectionAfter(const std::string& current);
  void fire(const StyleEvent& ev, double fromTick, double toTick, int numSamples,
            std::vector<MidiEv>& out);
  void fireEvent(const StyleEvent& ev, int outCh, double fromTick, double toTick,
                 int numSamples, std::vector<MidiEv>& out, const CtabRule& rule,
                 bool ignoreMute);
  bool playsAsWritten(const CtabRule* rule) const;
  bool isDrumChannel(int srcChannel, const CtabRule* rule) const;
  bool segmentCoversChord(const CasmSegment* seg, int chordTypeId) const;
  const CtabRule* nearestRuleFor(const CasmSegment* seg, int destChannel,
                                 int chordTypeId) const;
  CtabRule passthroughRule(int sourceChannel) const;
  CtabRule defaultRule(int sourceChannel) const;
  void allOff(std::vector<MidiEv>& out, int numSamples);
};

} // namespace yamaha
