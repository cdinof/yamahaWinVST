#include "ChordTransposer.h"

#include <cstdlib>
#include <utility>

namespace yamaha {

namespace {

/// Encaixa a nota no tom de acorde mais proximo mantendo a oitava.
/// Empates resolvem para o tom acima (ex.: G3 -> A3 em F, pelo manual).
int nearestChordToneInRegister(int note, const Chord& chord) {
  const auto pcs = chord.pitchClasses();
  int best = note, bestDist = 999;
  for (int oct = -1; oct <= 1; ++oct) {
    for (int pc : pcs) {
      const int cand = (note / 12 + oct) * 12 + pc;
      const int d = std::abs(cand - note);
      if (d < bestDist || (d == bestDist && cand > best)) {
        bestDist = d;
        best = cand;
      }
    }
  }
  return best;
}

/// Dobra a nota por oitavas ate cair dentro de [lo, hi] (Note Limit).
int foldIntoRange(int n, int lo, int hi) {
  if (lo > hi || (lo == 0 && hi >= 127)) return n;
  int guard = 0;
  while (n < lo && guard++ < 12) n += 12;
  guard = 0;
  while (n > hi && guard++ < 12) n -= 12;
  return n;
}

int clampMidi(int n) {
  while (n < 0) n += 12;
  while (n > 127) n -= 12;
  return n;
}

/// Mapeia um grau do acorde de origem para o MESMO grau no destino.
/// Graus que nao sao tons do acorde de origem sao quantizados para o tom mais
/// proximo (empate -> grau acima).
int degreeMap(int rel, const CtabRule& rule, const Chord& chord) {
  const auto& srcTones = chordTonesForType(rule.sourceChordType);
  const auto& tgtTones = chordTonesForType(yamahaTypeId(chord.quality));
  size_t idx = 0;
  int bestDist = 99;
  for (size_t i = 0; i < srcTones.size(); ++i) {
    const int d = std::abs(srcTones[i] - rel);
    if (d < bestDist) { // estritamente menor: em empate fica o de cima
      bestDist = d;
      idx = i;
    }
  }
  if (idx >= tgtTones.size()) idx = tgtTones.size() - 1;
  return tgtTones[idx] % 12;
}

/// Ajuste de graus ao trocar entre maior e menor (tabelas menores do NTT).
int minorAdjust(int rel, const CtabRule& rule, const Chord& chord,
                const std::vector<std::pair<int, int>>& map) {
  const bool srcMinor = isMinorTypeId(rule.sourceChordType);
  const bool tgtMinor = isMinorTypeId(yamahaTypeId(chord.quality));
  if (srcMinor == tgtMinor) return rel;
  if (srcMinor && !tgtMinor) {
    for (const auto& e : map)
      if (rel == e.second) return e.first; // menor -> maior: inverte
    return rel;
  }
  for (const auto& e : map)
    if (rel == e.first) return e.second; // maior -> menor
  return rel;
}

int applyNtt(int rel, NttTable ntt, const CtabRule& rule, const Chord& chord) {
  switch (ntt) {
    case NttTable::bypass:
    case NttTable::melody:
    case NttTable::bass:
      // Bypass: nada. Melody: cromatico. Bass = Melody + Bass On (o "on bass"
      // ja foi resolvido na escolha da tonica alvo).
      return rel;
    case NttTable::chord:
    case NttTable::guitarAllPurpose:
    case NttTable::guitarStroke:
    case NttTable::guitarArpeggio:
      return degreeMap(rel, rule, chord);
    case NttTable::melodicMinor:
      return minorAdjust(rel, rule, chord, {{4, 3}});
    case NttTable::melodicMinor5:
      return minorAdjust(rel, rule, chord, {{4, 3}, {7, 6}});
    case NttTable::harmonicMinor:
      return minorAdjust(rel, rule, chord, {{4, 3}, {9, 8}});
    case NttTable::harmonicMinor5:
      return minorAdjust(rel, rule, chord, {{4, 3}, {9, 8}, {7, 6}});
    case NttTable::naturalMinor:
      return minorAdjust(rel, rule, chord, {{4, 3}, {9, 8}, {11, 10}});
    case NttTable::naturalMinor5:
      return minorAdjust(rel, rule, chord, {{4, 3}, {9, 8}, {11, 10}, {7, 6}});
    case NttTable::dorian:
      return minorAdjust(rel, rule, chord, {{4, 3}, {11, 10}});
    case NttTable::dorian5:
      return minorAdjust(rel, rule, chord, {{4, 3}, {11, 10}, {7, 6}});
  }
  return rel;
}

} // namespace

int ChordTransposer::transposeNote(int sourceNote, const Chord& chord,
                                   const CtabRule& rule, bool ignoreMute) {
  const CasmZone& zone = rule.zoneFor(sourceNote);
  const int typeId = yamahaTypeId(chord.quality);

  if (!ignoreMute && !rule.playsChord(chord.root, typeId)) return -1;

  const int srcRoot = rule.sourceRoot;
  // "On bass": em acordes com baixo diferente (C/E), a parte de baixo segue o
  // baixo quando a zona tem Bass On.
  const int targetRoot =
      (zone.bassOn && chord.hasOnBass()) ? chord.bass : chord.root;

  // --- 1) transposicao base, conforme o NTR ---------------------------------
  int n;
  switch (zone.ntr) {
    case NtrRule::rootTrans:
    case NtrRule::guitar: {
      int shift = mod12(targetRoot - srcRoot);
      // HIGH KEY: acordes com tonica acima do limite tocam uma oitava abaixo.
      if (zone.ntr == NtrRule::rootTrans && targetRoot > zone.highKey) shift -= 12;
      n = sourceNote + shift;
      break;
    }
    case NtrRule::rootFixed:
      // Mantem o registro; encaixa no tom de acorde mais proximo.
      n = nearestChordToneInRegister(sourceNote, chord);
      break;
    default:
      n = sourceNote;
      break;
  }

  // --- 2) tabela NTT sobre a classe de altura relativa a tonica ------------
  const int rel = mod12(n - targetRoot);
  const int newRel = applyNtt(rel, zone.ntt, rule, chord);
  n = n - rel + newRel;

  // --- 3) limites de nota ---------------------------------------------------
  n = foldIntoRange(n, zone.noteLow, zone.noteHigh);
  return clampMidi(n);
}

} // namespace yamaha
