#include "StyParser.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace yamaha {

namespace {

uint16_t rdU16(const std::vector<uint8_t>& d, size_t i) {
  return static_cast<uint16_t>((d[i] << 8) | d[i + 1]);
}
uint32_t rdU32(const std::vector<uint8_t>& d, size_t i) {
  return (static_cast<uint32_t>(d[i]) << 24) |
         (static_cast<uint32_t>(d[i + 1]) << 16) |
         (static_cast<uint32_t>(d[i + 2]) << 8) | static_cast<uint32_t>(d[i + 3]);
}

bool matchTag(const std::vector<uint8_t>& d, size_t i, const char* tag) {
  const size_t n = std::strlen(tag);
  if (i + n > d.size()) return false;
  return std::memcmp(d.data() + i, tag, n) == 0;
}

int findTag(const std::vector<uint8_t>& d, const char* tag, size_t from) {
  const size_t n = std::strlen(tag);
  if (d.size() < n) return -1;
  for (size_t i = from; i + n <= d.size(); ++i) {
    if (std::memcmp(d.data() + i, tag, n) == 0) return static_cast<int>(i);
  }
  return -1;
}

std::string strAt(const std::vector<uint8_t>& d, size_t i, size_t len) {
  std::string s;
  s.reserve(len);
  for (size_t k = 0; k < len && i + k < d.size(); ++k)
    s.push_back(static_cast<char>(d[i + k]));
  return s;
}

/// VLQ (variable-length quantity) do MIDI.
struct Vlq {
  uint32_t value;
  size_t next;
};
Vlq readVlq(const std::vector<uint8_t>& d, size_t i) {
  uint32_t v = 0;
  while (i < d.size()) {
    const uint8_t b = d[i++];
    v = (v << 7) | (b & 0x7F);
    if ((b & 0x80) == 0) break;
  }
  return {v, i};
}

bool isSectionName(const std::string& s) {
  static const char* known[] = {
      "Intro A", "Intro B", "Intro C", "Intro D",
      "Main A", "Main B", "Main C", "Main D",
      "Fill In AA", "Fill In BB", "Fill In CC", "Fill In DD",
      "Fill In BA", "Fill In AB",
      "Ending A", "Ending B", "Ending C", "Ending D"};
  for (const char* k : known)
    if (s == k) return true;
  return false;
}

NtrRule decodeNtr(int v) {
  switch (v) {
    case 1: return NtrRule::rootFixed;
    case 2: return NtrRule::guitar;
    default: return NtrRule::rootTrans;
  }
}

RtrRule decodeRtr(int v) {
  switch (v) {
    case 0: return RtrRule::stop;
    case 1: return RtrRule::pitchShift;
    case 2: return RtrRule::pitchShiftToRoot;
    case 3: return RtrRule::retrigger;
    case 4: return RtrRule::retriggerToRoot;
    default: return RtrRule::noteGenerator;
  }
}

/// Tabelas do Cntt/Ctb2 (11 tipos).
NttTable decodeNttNew(int v, NtrRule ntr) {
  const int t = v & 0x7F;
  if (ntr == NtrRule::guitar) {
    switch (t) {
      case 1: return NttTable::guitarStroke;
      case 2: return NttTable::guitarArpeggio;
      default: return NttTable::guitarAllPurpose;
    }
  }
  switch (t) {
    case 0: return NttTable::bypass;
    case 1: return NttTable::melody;
    case 2: return NttTable::chord;
    case 3: return NttTable::melodicMinor;
    case 4: return NttTable::melodicMinor5;
    case 5: return NttTable::harmonicMinor;
    case 6: return NttTable::harmonicMinor5;
    case 7: return NttTable::naturalMinor;
    case 8: return NttTable::naturalMinor5;
    case 9: return NttTable::dorian;
    case 10: return NttTable::dorian5;
    default: return NttTable::melody;
  }
}

/// Tabelas documentadas do Ctab (SFF1): 0 Bypass, 1 Melody, 2 Chord, 3 Bass,
/// 4 Melodic Minor, 5 Harmonic Minor. Sem bit 7 (Bass On vem do codigo 3).
/// Codigos 6..10 nao existem no Ctab -> assumem o sentido do Cntt/Ctb2.
std::pair<NttTable, bool> decodeNttOld(int v) {
  NttTable table;
  switch (v) {
    case 0: table = NttTable::bypass; break;
    case 1: table = NttTable::melody; break;
    case 2: table = NttTable::chord; break;
    case 3: table = NttTable::bass; break;
    case 4: table = NttTable::melodicMinor; break;
    case 5: table = NttTable::harmonicMinor; break;
    default: table = decodeNttNew(v, NtrRule::rootTrans); break;
  }
  return {table, table == NttTable::bass};
}

struct RawEvent {
  int tick;
  int status;
  int data1;
  int data2;
};
struct RawSysex {
  int tick;
  std::vector<uint8_t> bytes;
};
struct Marker {
  int tick;
  std::string text;
};

} // namespace

bool StyParser::parse(const std::vector<uint8_t>& d, YamahaStyle& out,
                      const std::string& fileName) {
  if (d.size() < 14 || !matchTag(d, 0, "MThd")) return false;
  const uint32_t hlen = rdU32(d, 4);
  if (8 + hlen > d.size()) return false;
  const int div = rdU16(d, 12);
  out = YamahaStyle();
  out.fileName = fileName;
  out.ppq = div;

  std::vector<RawEvent> events;
  std::vector<RawSysex> sysex;
  std::vector<Marker> markers;
  double tempo = 120.0;
  int tsNum = 4, tsDen = 4, formatVersion = 1;

  size_t p = 8 + hlen;
  while (p + 8 <= d.size() && matchTag(d, p, "MTrk")) {
    const uint32_t tlen = rdU32(d, p + 4);
    const size_t start = p + 8;
    const size_t end = std::min(start + tlen, d.size());

    size_t i = start;
    int tick = 0;
    int running = 0;
    while (i < end) {
      const Vlq vlq = readVlq(d, i);
      tick += static_cast<int>(vlq.value);
      i = vlq.next;
      if (i >= end) break;
      uint8_t status = d[i];

      if (status == 0xFF) { // meta
        if (i + 2 > end) break;
        const uint8_t mtype = d[i + 1];
        const Vlq lv = readVlq(d, i + 2);
        const size_t pstart = lv.next;
        const uint32_t plen = lv.value;
        if (mtype == 0x06) {
          markers.push_back({tick, strAt(d, pstart, plen)});
          if (markers.back().text == "SFF1") formatVersion = 1;
          if (markers.back().text == "SFF2") formatVersion = 2;
        } else if (mtype == 0x51 && plen == 3 && pstart + 3 <= d.size()) {
          const uint32_t us = (static_cast<uint32_t>(d[pstart]) << 16) |
                              (static_cast<uint32_t>(d[pstart + 1]) << 8) |
                              static_cast<uint32_t>(d[pstart + 2]);
          if (us > 0) tempo = 60000000.0 / static_cast<double>(us);
        } else if (mtype == 0x58 && plen >= 2 && pstart + 2 <= d.size()) {
          tsNum = d[pstart];
          tsDen = 1 << d[pstart + 1];
        }
        i = pstart + plen;
      } else if (status == 0xF0 || status == 0xF7) { // sysex
        const Vlq lv = readVlq(d, i + 1);
        const size_t pstart = lv.next;
        const uint32_t plen = lv.value;
        std::vector<uint8_t> msg;
        msg.push_back(0xF0);
        for (uint32_t k = 0; k < plen && pstart + k < d.size(); ++k)
          msg.push_back(d[pstart + k]);
        if (msg.empty() || msg.back() != 0xF7) msg.push_back(0xF7);
        sysex.push_back({tick, std::move(msg)});
        i = pstart + plen;
      } else { // canal
        if (status & 0x80) {
          running = status;
          ++i;
        } else {
          status = static_cast<uint8_t>(running);
        }
        const int type = status & 0xF0;
        int d1 = 0, d2 = 0;
        if (type == 0xC0 || type == 0xD0) {
          if (i >= end) break;
          d1 = d[i++];
        } else {
          if (i + 1 >= end) break;
          d1 = d[i++];
          d2 = d[i++];
        }
        if (type == 0x80 || type == 0x90 || type == 0xA0 || type == 0xB0 ||
            type == 0xC0 || type == 0xE0) {
          events.push_back({tick, status, d1, d2});
        }
      }
    }
    p = end;
  }

  // ---- seções a partir dos marcadores --------------------------------------
  std::vector<Marker> sectionMarkers;
  for (const auto& m : markers)
    if (isSectionName(m.text)) sectionMarkers.push_back(m);
  std::stable_sort(sectionMarkers.begin(), sectionMarkers.end(),
                   [](const Marker& a, const Marker& b) { return a.tick < b.tick; });

  for (size_t k = 0; k < sectionMarkers.size(); ++k) {
    const int startTick = sectionMarkers[k].tick;
    const int endTick = (k + 1 < sectionMarkers.size())
                            ? sectionMarkers[k + 1].tick
                            : (1 << 30);
    out.sections.emplace_back(sectionMarkers[k].text, startTick, endTick);
  }

  const int firstSectionTick =
      out.sections.empty() ? (1 << 30) : out.sections.front().startTick;

  // Distribui os eventos nas seções; setup (PC/CC) antes da 1ª seção.
  for (const auto& ev : events) {
    if (ev.tick < firstSectionTick) {
      const int t = ev.status & 0xF0;
      if (t == 0xC0 || t == 0xB0) out.setupEvents.emplace_back(0, ev.status, ev.data1, ev.data2);
      continue;
    }
    for (auto& s : out.sections) {
      if (ev.tick >= s.startTick && ev.tick < s.endTick) {
        s.events.emplace_back(ev.tick - s.startTick, ev.status, ev.data1, ev.data2);
        break;
      }
    }
  }
  // Última seção: limita ao último evento.
  for (auto& s : out.sections) {
    if (s.endTick == (1 << 30)) {
      const int last = s.events.empty() ? 0 : s.events.back().tick + 1;
      s.endTick = s.startTick + last;
    }
  }

  out.tempoBpm = tempo;
  out.timeSigNum = tsNum;
  out.timeSigDen = tsDen;
  out.formatVersion = formatVersion;

  // ---- SysEx de setup + detecção do padrão ---------------------------------
  for (const auto& sx : sysex)
    if (sx.tick < firstSectionTick) out.setupSysex.push_back(sx.bytes);

  SoundStandard best = SoundStandard::unknown;
  auto rank = [](SoundStandard s) {
    switch (s) {
      case SoundStandard::midi2: return 7;
      case SoundStandard::xg: return 6;
      case SoundStandard::gs: return 5;
      case SoundStandard::mt32: return 4;
      case SoundStandard::gm2: return 3;
      case SoundStandard::gmLite: return 2;
      case SoundStandard::gm1: return 1;
      case SoundStandard::unknown: return 0;
    }
    return 0;
  };
  auto consider = [&](SoundStandard s) {
    if (rank(s) > rank(best)) best = s;
  };
  for (const auto& m : out.setupSysex) {
    if (m.size() < 4) continue;
    const uint8_t mfr = m[1];
    if (mfr == 0x7E && m.size() >= 5 && m[3] == 0x09) {
      if (m[4] == 0x01) consider(SoundStandard::gm1);
      if (m[4] == 0x03) consider(SoundStandard::gm2);
    }
    if (mfr == 0x43 && m.size() >= 4 && m[3] == 0x4C) consider(SoundStandard::xg);
    if (mfr == 0x41 && m.size() >= 5) {
      if (m[3] == 0x42) consider(SoundStandard::gs);
      if (m[3] == 0x16) consider(SoundStandard::mt32);
    }
    if (mfr == 0x7E && m.size() >= 4 && m[3] == 0x0D) consider(SoundStandard::midi2);
  }
  out.standard = best;

  // ---- CASM ----------------------------------------------------------------
  const int casmIdx = findTag(d, "CASM", 0);
  if (casmIdx < 0) return true; // estilo sem CASM: segue com o que tem
  const uint32_t casmLen = rdU32(d, static_cast<size_t>(casmIdx) + 4);
  size_t j = static_cast<size_t>(casmIdx) + 8;
  const size_t end = std::min(j + casmLen, d.size());

  while (j + 8 <= end) {
    if (!matchTag(d, j, "CSEG")) { ++j; continue; }
    const uint32_t segLen = rdU32(d, j + 4);
    size_t k = j + 8;
    const size_t segEnd = std::min(k + segLen, end);

    std::vector<std::string> sectionNames;
    if (matchTag(d, k, "Sdec")) {
      const uint32_t sl = rdU32(d, k + 4);
      const std::string names = strAt(d, k + 8, sl);
      size_t pos = 0;
      while (pos <= names.size()) {
        const size_t comma = names.find(',', pos);
        const std::string piece = names.substr(
            pos, comma == std::string::npos ? std::string::npos : comma - pos);
        // trim
        size_t a = piece.find_first_not_of(" \t\r\n");
        size_t b = piece.find_last_not_of(" \t\r\n");
        if (a != std::string::npos) sectionNames.push_back(piece.substr(a, b - a + 1));
        if (comma == std::string::npos) break;
        pos = comma + 1;
      }
      k = k + 8 + sl;
    }

    std::vector<CtabRule> rules;
    while (k + 8 <= segEnd) {
      if (matchTag(d, k, "Ctab") || matchTag(d, k, "Ctb2")) {
        const bool sff2 = matchTag(d, k, "Ctb2");
        const uint32_t cl = rdU32(d, k + 4);
        const size_t cstart = k + 8;
        if (cstart + cl > d.size()) break;
        CtabRule r;
        bool ok = false;
        // ---- decodifica um registro Ctab (SFF1) ou Ctb2 (SFF2) ----
        if ((sff2 && cl >= 40) || (!sff2 && cl >= 26)) {
          r.sourceChannel = d[cstart] & 0x0F;
          r.name = strAt(d, cstart + 1, 8);
          // trim
          {
            size_t a = r.name.find_first_not_of(" \t\r\n");
            size_t b = r.name.find_last_not_of(" \t\r\n");
            r.name = (a == std::string::npos) ? std::string() : r.name.substr(a, b - a + 1);
          }
          const int destRaw = d[cstart + 9] & 0x0F;
          r.destChannel = (destRaw >= 8 && destRaw <= 15) ? destRaw : (r.sourceChannel | 0x08);
          r.editable = (d[cstart + 10] == 0);
          r.noteMute = ((d[cstart + 11] << 8) | d[cstart + 12]) & 0x0FFF;
          int64_t cm = 0;
          for (int b = 13; b < 18; ++b) cm = (cm << 8) | d[cstart + b];
          r.autoStart = (cm & (int64_t(1) << 34)) != 0;
          r.chordMute = cm & CtabRule::kAllChordTypes;
          r.sourceRoot = d[cstart + 18] % 12;
          r.sourceChordType =
              (d[cstart + 19] < static_cast<int>(chordTonesTable().size()))
                  ? d[cstart + 19]
                  : 2;
          r.sff2 = sff2;

          if (sff2) {
            r.midLow = d[cstart + 20];
            r.midHigh = d[cstart + 21];
            for (int base : {22, 28, 34}) {
              const NtrRule ntr = decodeNtr(d[cstart + base]);
              const NttTable ntt = decodeNttNew(d[cstart + base + 1], ntr);
              r.zones.emplace_back(ntr, ntt, d[cstart + base + 2] % 12,
                                   d[cstart + base + 3] & 0x7F,
                                   d[cstart + base + 4] & 0x7F,
                                   decodeRtr(d[cstart + base + 5]),
                                   (d[cstart + base + 1] & 0x80) != 0);
            }
          } else {
            const NtrRule ntr = decodeNtr(d[cstart + 20]);
            const auto nttBass = decodeNttOld(d[cstart + 21]);
            r.zones.emplace_back(ntr, nttBass.first, d[cstart + 22] % 12,
                                 d[cstart + 23] & 0x7F, d[cstart + 24] & 0x7F,
                                 decodeRtr(d[cstart + 25]), nttBass.second);
          }
          ok = true;
        }
        if (ok) rules.push_back(std::move(r));
        k = cstart + cl;
      } else if (matchTag(d, k, "Cntt")) {
        // Cntt: 2 bytes -> (source channel, novo NTT). Refina a tabela do Ctab.
        const uint32_t cl = rdU32(d, k + 4);
        if (cl >= 2 && k + 8 + 2 <= d.size()) {
          const int ch = d[k + 8] & 0x0F;
          const int nttByte = d[k + 9];
          for (auto& r : rules) {
            if (r.sourceChannel == ch && !r.sff2) {
              const NttTable ntt = decodeNttNew(nttByte, r.zones[0].ntr);
              const bool bassOn = r.zones[0].bassOn || (nttByte & 0x80) != 0;
              for (auto& z : r.zones) {
                z.ntt = ntt;
                z.bassOn = bassOn;
              }
            }
          }
        }
        k = k + 8 + cl;
      } else {
        ++k;
      }
    }
    out.casm.emplace_back(std::move(sectionNames), std::move(rules));
    j = segEnd;
  }
  return true;
}

bool StyParser::parseFile(const std::string& path, YamahaStyle& out) {
  std::FILE* f = std::fopen(path.c_str(), "rb");
  if (!f) return false;
  std::fseek(f, 0, SEEK_END);
  const long size = std::ftell(f);
  std::fseek(f, 0, SEEK_SET);
  if (size <= 0) { std::fclose(f); return false; }
  std::vector<uint8_t> buf(static_cast<size_t>(size));
  const size_t read = std::fread(buf.data(), 1, buf.size(), f);
  std::fclose(f);
  if (read != buf.size()) return false;
  const std::string name = path.substr(path.find_last_of("/\\") + 1);
  return parse(buf, out, name);
}

} // namespace yamaha
