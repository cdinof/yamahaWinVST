// YamahaArranger - engine: tabelas e enums do formato de estilo Yamaha (SFF).
//
// Porte fiel do motor Dart validado no app Android (ver docs/casm_layout.md).
// C++17, sem dependências externas.
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace yamaha {

// ---------------------------------------------------------------------------
// Tabelas de chord type do CASM (ids 0..33), na ordem usada pelo chord mute.
// Intervalos em semitons a partir da tônica.
// ---------------------------------------------------------------------------
inline const std::vector<std::vector<int>>& chordTonesTable() {
  static const std::vector<std::vector<int>> table = {
      {0, 4, 7},       // 0 Maj
      {0, 4, 7, 9},    // 1 6
      {0, 4, 7, 11},   // 2 M7
      {0, 4, 6, 7, 11},// 3 M7(#11)
      {0, 2, 4, 7},    // 4 (9) / add9
      {0, 2, 4, 7, 11},// 5 M7(9)
      {0, 2, 4, 7, 9}, // 6 6(9)
      {0, 4, 8},       // 7 aug
      {0, 3, 7},       // 8 m
      {0, 3, 7, 9},    // 9 m6
      {0, 3, 7, 10},   // 10 m7
      {0, 3, 6, 10},   // 11 m7b5
      {0, 2, 3, 7},    // 12 m(9)
      {0, 2, 3, 7, 10},// 13 m9
      {0, 3, 5, 7, 10},// 14 m7(11)
      {0, 3, 7, 11},   // 15 mM7
      {0, 2, 3, 7, 11},// 16 mM7(9)
      {0, 3, 6},       // 17 dim
      {0, 3, 6, 9},    // 18 dim7
      {0, 4, 7, 10},   // 19 7
      {0, 5, 7, 10},   // 20 7sus4
      {0, 4, 6, 10},   // 21 7b5
      {0, 2, 4, 7, 10},// 22 9
      {0, 4, 6, 7, 10},// 23 7(#11)
      {0, 4, 7, 9, 10},// 24 13
      {0, 1, 4, 7, 10},// 25 7(b9)
      {0, 4, 7, 8, 10},// 26 7(b13)
      {0, 3, 4, 7, 10},// 27 7(#9)
      {0, 4, 8, 11},   // 28 M7aug
      {0, 4, 8, 10},   // 29 7aug
      {0},             // 30 1+8
      {0, 7},          // 31 1+5
      {0, 5, 7},       // 32 sus4
      {0, 2, 7},       // 33 sus2
  };
  return table;
}

/// Tons (intervalos relativos à tônica) de um chord type id Yamaha.
/// Ids fora da tabela voltam como Maior.
inline const std::vector<int>& chordTonesForType(int typeId) {
  const auto& t = chordTonesTable();
  if (typeId < 0 || typeId >= static_cast<int>(t.size())) return t[0];
  return t[static_cast<size_t>(typeId)];
}

/// true se o chord type id Yamaha é menor (tríade/tétrade).
inline bool isMinorTypeId(int typeId) {
  switch (typeId) {
    case 8: case 9: case 10: case 11: case 12: case 13:
    case 14: case 15: case 16: case 17: case 18:
      return true;
    default:
      return false;
  }
}

/// Nomes legíveis dos chord types (para debug/UI).
inline const char* chordTypeName(int id) {
  static const char* names[] = {
      "Maj", "6", "M7", "M7#11", "add9", "M9", "6/9", "aug", "m", "m6",
      "m7", "m7b5", "m(9)", "m9", "m7(11)", "mM7", "mM7(9)", "dim",
      "dim7", "7", "7sus4", "7b5", "9", "7#11", "13", "7b9", "7b13",
      "7#9", "M7aug", "7aug", "1+8", "1+5", "sus4", "sus2"};
  if (id < 0 || id >= 34) return "?";
  return names[id];
}

// ---------------------------------------------------------------------------
// Enums do CASM
// ---------------------------------------------------------------------------

/// Note Transposition Rule (byte 20 do Ctab / byte 0 de cada zona do Ctb2).
enum class NtrRule {
  rootTrans, ///< desloca o padrão inteiro pelo intervalo da tônica
  rootFixed, ///< mantém o registro e encaixa no tom de acorde mais próximo
  guitar,    ///< desloca pela tônica restringindo a tons do acorde
};

/// Note Transposition Table (byte 21 do Ctab / byte 1 de cada zona).
enum class NttTable {
  bypass, melody, chord, bass,
  melodicMinor, melodicMinor5,
  harmonicMinor, harmonicMinor5,
  naturalMinor, naturalMinor5,
  dorian, dorian5,
  guitarAllPurpose, guitarStroke, guitarArpeggio,
};

/// Retrigger Rule (byte 25 do Ctab).
enum class RtrRule { stop, pitchShift, pitchShiftToRoot, retrigger,
                     retriggerToRoot, noteGenerator };

/// Padrão de módulo de som detectado pelas SysEx do estilo.
enum class SoundStandard { unknown, gm1, gm2, gs, xg, mt32, gmLite, midi2 };

inline const char* soundStandardLabel(SoundStandard s) {
  switch (s) {
    case SoundStandard::unknown: return "GM (padrao)";
    case SoundStandard::gm1: return "GM1";
    case SoundStandard::gm2: return "GM2";
    case SoundStandard::gs: return "GS";
    case SoundStandard::xg: return "XG";
    case SoundStandard::mt32: return "MT-32";
    case SoundStandard::gmLite: return "GM Lite";
    case SoundStandard::midi2: return "MIDI 2.0";
  }
  return "?";
}

/// Utilitários
inline int mod12(int v) { return ((v % 12) + 12) % 12; }

} // namespace yamaha
