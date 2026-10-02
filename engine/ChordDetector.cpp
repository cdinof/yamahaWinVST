#include "ChordDetector.h"

#include <algorithm>

namespace yamaha {

namespace {

/// Classes de altura das teclas brancas: C D E F G A B.
inline bool isWhitePc(int pc) {
  switch (mod12(pc)) {
    case 0: case 2: case 4: case 5: case 7: case 9: case 11: return true;
    default: return false;
  }
}

} // namespace

bool ChordDetector::detect(const std::vector<int>& notes, Chord& out) const {
  if (notes.empty()) return false;
  std::vector<int> sorted = notes;
  std::sort(sorted.begin(), sorted.end());

  switch (mode) {
    case FingeringMode::singleFinger:
      return singleFinger(sorted, out);
    case FingeringMode::fingered:
    case FingeringMode::aiFingered:
    case FingeringMode::fullKeyboard:
      return matchChord(sorted, out);
  }
  return false;
}

bool ChordDetector::singleFinger(const std::vector<int>& sorted, Chord& out) const {
  // A tecla MAIS AGUDA é a tônica; as abaixo modificam (Yamaha Single Finger).
  const int root = mod12(sorted.back());
  std::vector<int> modifiers;
  for (int n : sorted)
    if (n < sorted.back()) modifiers.push_back(n);

  if (modifiers.empty()) {
    out = Chord(root, ChordQuality::maj);
    return true;
  }

  bool hasWhite = false, hasBlack = false;
  for (int n : modifiers) {
    if (isWhitePc(n)) hasWhite = true; else hasBlack = true;
  }

  if (hasWhite && hasBlack) out = Chord(root, ChordQuality::min7);
  else if (hasBlack)        out = Chord(root, ChordQuality::min);
  else if (hasWhite)        out = Chord(root, ChordQuality::dom7);
  else                      out = Chord(root, ChordQuality::maj);
  return true;
}

bool ChordDetector::twoNoteGuess(const std::vector<int>& sorted, Chord& out) const {
  const int a = mod12(sorted[0]);
  const int b = mod12(sorted[1]);
  const int interval = mod12(b - a);
  switch (interval) {
    case 7:  out = Chord(a, ChordQuality::oneFive); return true; // 5ª justa
    case 4:  out = Chord(a, ChordQuality::maj); return true;     // 3ª maior
    case 3:  out = Chord(a, ChordQuality::min); return true;     // 3ª menor
    case 5:  out = Chord(b, ChordQuality::maj); return true;     // 4ª -> inversão
    default: out = Chord(a, ChordQuality::maj); return true;
  }
}

bool ChordDetector::matchChord(const std::vector<int>& sorted, Chord& out) const {
  const int bassNote = mod12(sorted.front()); // nota mais grave
  std::vector<bool> held(12, false);
  for (int n : sorted) held[mod12(n)] = true;
  const int distinct = static_cast<int>(
      std::count(held.begin(), held.end(), true));

  // AI Fingered: com 1-2 notas, infere.
  if (mode == FingeringMode::aiFingered) {
    if (distinct == 1) {
      out = Chord(mod12(sorted.front()), ChordQuality::maj);
      return true;
    }
    if (distinct == 2) return twoNoteGuess(sorted, out);
  }

  bool haveBest = false;
  Chord best;
  int bestScore = -1;

  for (int root = 0; root < 12; ++root) {
    for (int qi = 0; qi < qualityCount(); ++qi) {
      const ChordQuality q = static_cast<ChordQuality>(qi);
      const auto& info = qualityInfo(q);
      std::vector<bool> chordPcs(12, false);
      for (int iv : info.intervals) chordPcs[mod12(root + iv)] = true;

      const int sc = score(held, chordPcs, root, bassNote, q);
      if (sc > bestScore) {
        bestScore = sc;
        const bool bassIsChordTone = chordPcs[static_cast<size_t>(bassNote)];
        const int bass =
            (bassNote != root && (bassIsChordTone || distinct > 3)) ? bassNote
                                                                   : Chord::kNoBass;
        best = Chord(root, q, bass);
        haveBest = true;
      }
    }
  }
  if (!haveBest || bestScore < 0) return false;
  out = best;
  return true;
}

int ChordDetector::score(const std::vector<bool>& held,
                         const std::vector<bool>& chordPcs, int root,
                         int bassNote, ChordQuality q) const {
  int matched = 0, missing = 0, extra = 0;
  for (int i = 0; i < 12; ++i) {
    if (held[i] && chordPcs[i]) ++matched;
    else if (!held[i] && chordPcs[i]) ++missing;
    else if (held[i] && !chordPcs[i]) ++extra;
  }

  if (!held[static_cast<size_t>(root)]) return -1; // precisa conter a tônica
  if (matched < static_cast<int>(chordPcs.size()) - 0) {
    // 0 faltando para tríades, no máximo 1 para acordes de 4+ notas
    if (!(static_cast<int>(chordPcs.size()) >= 4 && missing <= 1)) return -1;
  }

  int s = matched * 10 - extra * 6 - missing * 8;
  if (bassNote == root) {
    // Posição fundamental é muito preferida. Bônus maior para acordes ricos,
    // para que A-C-E-G seja lido como Am7 e não como C6/A.
    s += 6 + (static_cast<int>(chordPcs.size()) >= 4 ? 2 : 0);
  } else if (chordPcs[static_cast<size_t>(bassNote)]) {
    s += 2; // inversão limpa ainda é boa
  }
  s += (qualityCount() - static_cast<int>(q)); // favorece qualidades comuns
  return s;
}

} // namespace yamaha
