// Detector de acordes estilo Yamaha (Single Finger / Fingered / AI Fingered).
#pragma once

#include <vector>

#include "Chord.h"

namespace yamaha {

enum class FingeringMode { singleFinger, fingered, aiFingered, fullKeyboard };

class ChordDetector {
 public:
  explicit ChordDetector(FingeringMode m = FingeringMode::aiFingered) : mode(m) {}

  /// Detecta o acorde a partir das notas seguradas (0..127).
  /// Retorna false quando não reconhece nada (mantém o acorde anterior).
  bool detect(const std::vector<int>& notes, Chord& out) const;

  FingeringMode mode;

 private:
  bool singleFinger(const std::vector<int>& sorted, Chord& out) const;
  bool matchChord(const std::vector<int>& sorted, Chord& out) const;
  bool twoNoteGuess(const std::vector<int>& sorted, Chord& out) const;
  int score(const std::vector<bool>& held, const std::vector<bool>& chordPcs,
            int root, int bassNote, ChordQuality q) const;
};

} // namespace yamaha
