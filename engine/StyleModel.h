// Modelos de dados de um estilo Yamaha (.sty / SFF1 / SFF2).
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "YamahaTypes.h"

namespace yamaha {

// ---------------------------------------------------------------------------
// Eventos e seções
// ---------------------------------------------------------------------------

/// Evento MIDI dentro de uma seção (tick relativo ao início da seção).
struct StyleEvent {
  int tick = 0;
  int status = 0; // byte de status completo (canal no nibble baixo)
  int data1 = 0;
  int data2 = 0;

  StyleEvent() = default;
  StyleEvent(int t, int s, int d1, int d2) : tick(t), status(s), data1(d1), data2(d2) {}

  int type() const { return status & 0xF0; }
  int channel() const { return status & 0x0F; }
  bool isNoteOn() const { return type() == 0x90 && data2 > 0; }
  bool isNoteOff() const { return type() == 0x80 || (type() == 0x90 && data2 == 0); }
};

/// Uma "zona" de transposição. SFF1 tem uma zona por canal; SFF2 (Ctb2) tem 3.
struct CasmZone {
  NtrRule ntr = NtrRule::rootTrans;
  NttTable ntt = NttTable::melody;
  int highKey = 6;      // 0..11 — limite superior de tônica (só no ROOT TRANS)
  int noteLow = 0;      // limite inferior de nota
  int noteHigh = 127;   // limite superior de nota
  RtrRule rtr = RtrRule::pitchShift;
  bool bassOn = false;  // segue acordes com baixo diferente (slash)

  CasmZone() = default;
  CasmZone(NtrRule n, NttTable t, int hk, int lo, int hi, RtrRule r, bool b)
      : ntr(n), ntt(t), highKey(hk), noteLow(lo), noteHigh(hi), rtr(r), bassOn(b) {}
};

/// Regra de um canal de origem (Ctab/Ctb2).
///
/// Layout do Ctab (SFF1, 27 bytes):
///   0 source channel | 1..8 nome | 9 dest channel | 10 editable
///   11..12 note mute | 13..17 chord mute | 18 source root | 19 source type
///   20 NTR | 21 NTT | 22 high key | 23 note low | 24 note high | 25 RTR | 26 fim
struct CtabRule {
  int sourceChannel = 0; // 0-based
  std::string name;
  int destChannel = 0;   // 0-based, 8..15 (canais 9..16)
  bool editable = true;
  int noteMute = 0x0FFF;   // bit n = tônica n toca
  int64_t chordMute = 0;   // bit n = chord type n toca; bit 34 = autostart
  bool autoStart = false;
  int sourceRoot = 0;      // 0..11
  int sourceChordType = 2; // 0..33 (2 = M7)
  int midLow = 0;          // SFF2: notas abaixo usam a zona 0
  int midHigh = 127;       // SFF2: notas acima usam a zona 2
  std::vector<CasmZone> zones; // 1 (SFF1) ou 3 (SFF2)
  bool sff2 = false;

  static constexpr int64_t kAllChordTypes = (int64_t(1) << 34) - 1;
  static constexpr int kAllRoots = 0x0FFF;

  /// Zona aplicável a uma nota.
  const CasmZone& zoneFor(int key) const {
    static const CasmZone kDefault(NtrRule::rootTrans, NttTable::melody, 6, 0, 127,
                                   RtrRule::pitchShift, false);
    if (zones.empty()) return kDefault;
    if (zones.size() == 1) return zones[0];
    if (key < midLow) return zones[0];
    if (key > midHigh) return zones[2];
    return zones[1];
  }

  /// Este canal toca com a tônica/tipo informados?
  /// As máscaras selectam entre padrões alternativos (maior/menor) de um mesmo
  /// canal de destino — sem isso os dois tocam juntos e o estilo soa errado.
  bool playsChord(int rootPc, int chordTypeId) const {
    if (noteMute != 0 && noteMute != kAllRoots) {
      if ((noteMute & (1 << (rootPc % 12))) == 0) return false;
    }
    if (chordMute != 0 && chordMute != kAllChordTypes) {
      if ((chordMute & (int64_t(1) << (chordTypeId % 34))) == 0) return false;
    }
    return true;
  }
};

/// Um segmento CASM: regras para uma lista de seções.
struct CasmSegment {
  std::vector<std::string> sections;
  std::vector<CtabRule> rules;

  CasmSegment() = default;
  CasmSegment(std::vector<std::string> s, std::vector<CtabRule> r)
      : sections(std::move(s)), rules(std::move(r)) {}

  /// Regra para um canal de origem (0-based). nullptr se não houver.
  const CtabRule* ruleFor(int sourceChannel) const {
    for (const auto& r : rules)
      if (r.sourceChannel == sourceChannel) return &r;
    return nullptr;
  }
};

/// Seção tocável (Main A, Fill In AA, Intro A, Ending B...).
struct StyleSection {
  std::string name;
  int startTick = 0;
  int endTick = 0; // exclusivo
  std::vector<StyleEvent> events;

  StyleSection() = default;
  StyleSection(std::string n, int start, int end)
      : name(std::move(n)), startTick(start), endTick(end) {}

  int lengthTicks() const { return endTick - startTick; }
};

/// Estilo completo já parseado.
struct YamahaStyle {
  std::string fileName;
  int ppq = 480;
  int formatVersion = 1; // 1 = SFF1, 2 = SFF2
  double tempoBpm = 120.0;
  int timeSigNum = 4;
  int timeSigDen = 4;
  std::vector<StyleSection> sections;
  std::vector<CasmSegment> casm;
  std::vector<StyleEvent> setupEvents;   // zona SInt (antes da 1ª seção)
  std::vector<std::vector<uint8_t>> setupSysex;
  SoundStandard standard = SoundStandard::unknown;

  const StyleSection* sectionByName(const std::string& name) const {
    for (const auto& s : sections)
      if (s.name == name) return &s;
    return nullptr;
  }

  /// Segmento CASM que contém [sectionName] (fallback: o primeiro).
  const CasmSegment* casmFor(const std::string& sectionName) const {
    for (const auto& seg : casm)
      for (const auto& s : seg.sections)
        if (s == sectionName) return &seg;
    return casm.empty() ? nullptr : &casm.front();
  }

  std::vector<std::string> sectionNames() const {
    std::vector<std::string> out;
    out.reserve(sections.size());
    for (const auto& s : sections) out.push_back(s.name);
    return out;
  }
};

using YamahaStylePtr = std::shared_ptr<YamahaStyle>;

} // namespace yamaha
