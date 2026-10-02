// Modelo de acorde + nomes de notas.
#pragma once

#include <string>
#include <vector>

#include "YamahaTypes.h"

namespace yamaha {

inline const char* kNoteNamesSharp[] = {"C",  "C#", "D", "D#", "E",  "F",
                                        "F#", "G",  "G#", "A", "A#", "B"};

inline const char* pitchClassName(int pc) { return kNoteNamesSharp[mod12(pc)]; }

/// Qualidades de acorde reconhecidas, com os intervalos (semitons da tônica).
enum class ChordQuality {
  maj, min, dim, aug, sus4, sus2, maj6, min6, dom7, maj7, min7, minMaj7,
  m7b5, dim7, dom7s5, dom7b5, add9, dom9, maj9, min9, dom7s9, dom7b9,
  oneFive, oneEight
};

struct ChordQualityInfo {
  const char* suffix;
  std::vector<int> intervals;
};

/// Tabela de qualidades — a ORDEM importa (o detector usa o índice como
/// desempate, favorecendo qualidades mais comuns).
inline const std::vector<ChordQualityInfo>& qualityTable() {
  static const std::vector<ChordQualityInfo> table = {
      {"", {0, 4, 7}},          // maj
      {"m", {0, 3, 7}},         // min
      {"dim", {0, 3, 6}},       // dim
      {"aug", {0, 4, 8}},       // aug
      {"sus4", {0, 5, 7}},      // sus4
      {"sus2", {0, 2, 7}},      // sus2
      {"6", {0, 4, 7, 9}},      // maj6
      {"m6", {0, 3, 7, 9}},     // min6
      {"7", {0, 4, 7, 10}},     // dom7
      {"M7", {0, 4, 7, 11}},    // maj7
      {"m7", {0, 3, 7, 10}},    // min7
      {"mM7", {0, 3, 7, 11}},   // minMaj7
      {"m7b5", {0, 3, 6, 10}},  // m7b5
      {"dim7", {0, 3, 6, 9}},   // dim7
      {"7#5", {0, 4, 8, 10}},   // dom7s5
      {"7b5", {0, 4, 6, 10}},   // dom7b5
      {"add9", {0, 4, 7, 14}},  // add9
      {"9", {0, 4, 7, 10, 14}}, // dom9
      {"M9", {0, 4, 7, 11, 14}},// maj9
      {"m9", {0, 3, 7, 10, 14}},// min9
      {"7#9", {0, 4, 7, 10, 15}},  // dom7s9
      {"7b9", {0, 4, 7, 10, 13}},  // dom7b9
      {"1+5", {0, 7}},          // oneFive
      {"1+8", {0}},             // oneEight
  };
  return table;
}

inline const ChordQualityInfo& qualityInfo(ChordQuality q) {
  return qualityTable()[static_cast<size_t>(q)];
}

inline int qualityCount() { return static_cast<int>(qualityTable().size()); }

/// Mapeia uma ChordQuality para o chord type id do CASM Yamaha (0..33).
inline int yamahaTypeId(ChordQuality q) {
  switch (q) {
    case ChordQuality::maj: return 0;
    case ChordQuality::maj6: return 1;
    case ChordQuality::maj7: return 2;
    case ChordQuality::add9: return 4;
    case ChordQuality::maj9: return 5;
    case ChordQuality::aug: return 7;
    case ChordQuality::min: return 8;
    case ChordQuality::min6: return 9;
    case ChordQuality::min7: return 10;
    case ChordQuality::m7b5: return 11;
    case ChordQuality::min9: return 13;
    case ChordQuality::minMaj7: return 15;
    case ChordQuality::dim: return 17;
    case ChordQuality::dim7: return 18;
    case ChordQuality::dom7: return 19;
    case ChordQuality::sus4: return 32;
    case ChordQuality::sus2: return 33;
    case ChordQuality::dom7b5: return 21;
    case ChordQuality::dom9: return 22;
    case ChordQuality::dom7b9: return 25;
    case ChordQuality::dom7s9: return 27;
    case ChordQuality::dom7s5: return 29;
    case ChordQuality::oneFive: return 31;
    case ChordQuality::oneEight: return 30;
  }
  return 0;
}

/// Acorde: tônica (pitch class 0..11), qualidade e baixo opcional (slash).
/// `bass < 0` significa "sem baixo diferente".
struct Chord {
  static constexpr int kNoBass = -1;
  static constexpr int kReferenceRoot = 0; // C (referência dos estilos Yamaha)

  int root = 0;
  ChordQuality quality = ChordQuality::maj;
  int bass = kNoBass;

  Chord() = default;
  Chord(int r, ChordQuality q, int b = kNoBass) : root(mod12(r)), quality(q), bass(b) {}

  std::string label() const {
    std::string s = std::string(pitchClassName(root)) + qualityInfo(quality).suffix;
    if (bass >= 0 && bass != root) {
      s += "/";
      s += pitchClassName(bass);
    }
    return s;
  }

  /// Classes de altura do acorde (root-relative -> absolutas).
  std::vector<int> pitchClasses() const {
    std::vector<int> out;
    for (int iv : qualityInfo(quality).intervals) out.push_back(mod12(root + iv));
    return out;
  }

  bool hasOnBass() const { return bass >= 0 && bass != root; }

  bool operator==(const Chord& o) const {
    return root == o.root && quality == o.quality && bass == o.bass;
  }
  bool operator!=(const Chord& o) const { return !(*this == o); }
};

} // namespace yamaha
