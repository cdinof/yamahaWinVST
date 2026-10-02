// Testes do motor (engine) — compilados e rodados sem JUCE.
//   g++ -std=c++17 -O2 -o engine_test tests/engine_test.cpp engine/*.cpp
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>
#include <vector>

#include "../engine/Chord.h"
#include "../engine/ChordDetector.h"
#include "../engine/ChordTransposer.h"
#include "../engine/StylePlayer.h"
#include "../engine/StyParser.h"

using namespace yamaha;

static int gFail = 0;
static int gPass = 0;

static void check(bool cond, const std::string& what) {
  if (cond) {
    ++gPass;
  } else {
    ++gFail;
    std::printf("  FALHOU: %s\n", what.c_str());
  }
}

static void checkEq(int got, int want, const std::string& what) {
  if (got == want) {
    ++gPass;
  } else {
    ++gFail;
    std::printf("  FALHOU: %s (obtido %d, esperado %d)\n", what.c_str(), got, want);
  }
}

static void checkStr(const std::string& got, const std::string& want,
                     const std::string& what) {
  if (got == want) {
    ++gPass;
  } else {
    ++gFail;
    std::printf("  FALHOU: %s (obtido '%s', esperado '%s')\n", what.c_str(), got.c_str(),
                want.c_str());
  }
}

// ---------------------------------------------------------------------------

static void testChordDetector() {
  std::printf("[detector de acordes]\n");
  ChordDetector sf(FingeringMode::singleFinger);
  Chord c;
  check(sf.detect({60}, c), "single finger 1 nota");
  checkStr(c.label(), "C", "root only -> C");
  check(sf.detect({59, 60}, c), "single finger 2 notas");
  checkStr(c.label(), "C7", "root + branca -> C7");
  check(sf.detect({58, 60}, c), "single finger preta");
  checkStr(c.label(), "Cm", "root + preta -> Cm");
  check(sf.detect({58, 59, 60}, c), "single finger branca+preta");
  checkStr(c.label(), "Cm7", "root + branca + preta -> Cm7");
  check(sf.detect({62, 64}, c), "single finger raiz no topo");
  checkStr(c.label(), "E7", "topo E + branca D -> E7");

  ChordDetector fg(FingeringMode::fingered);
  check(fg.detect({48, 52, 55}, c), "fingered C");
  checkStr(c.label(), "C", "C E G -> C");
  check(fg.detect({48, 51, 55}, c), "fingered Cm");
  checkStr(c.label(), "Cm", "C Eb G -> Cm");
  check(fg.detect({48, 52, 55, 58}, c), "fingered C7");
  checkStr(c.label(), "C7", "C E G Bb -> C7");
  check(fg.detect({52, 55, 60}, c), "fingered inversao");
  checkStr(c.label(), "C/E", "E G C -> C/E");
  check(fg.detect({47, 50, 55}, c), "fingered G/B");
  checkStr(c.label(), "G/B", "B D G -> G/B");
  check(fg.detect({57, 60, 64, 67}, c), "fingered Am7");
  checkStr(c.label(), "Am7", "A C E G -> Am7 (nao C6/A)");
}

static void testTransposer() {
  std::printf("[transpositor NTR/NTT]\n");

  auto rule = [](NtrRule ntr, NttTable ntt, int highKey = 6, int lo = 0,
                 int hi = 127, bool bassOn = false, int srcType = 2,
                 int srcRoot = 0) {
    CtabRule r;
    r.sourceChannel = 1;
    r.name = "test";
    r.destChannel = 11;
    r.sourceRoot = srcRoot;
    r.sourceChordType = srcType;
    r.zones.emplace_back(ntr, ntt, highKey, lo, hi, RtrRule::pitchShift, bassOn);
    return r;
  };

  const Chord f(5, ChordQuality::maj);
  // ROOT TRANS + Bypass: C3 E3 G3 -> F3 A3 C4
  auto rTrans = rule(NtrRule::rootTrans, NttTable::bypass);
  checkEq(ChordTransposer::transposeNote(60, f, rTrans), 65, "root trans C->F");
  checkEq(ChordTransposer::transposeNote(64, f, rTrans), 69, "root trans E->A");
  checkEq(ChordTransposer::transposeNote(67, f, rTrans), 72, "root trans G->C");

  // ROOT FIXED + Chord: C3 E3 G3 -> C3 F3 A3
  auto rFix = rule(NtrRule::rootFixed, NttTable::chord);
  checkEq(ChordTransposer::transposeNote(48, f, rFix), 48, "root fixed C3->C3");
  checkEq(ChordTransposer::transposeNote(52, f, rFix), 53, "root fixed E3->F3");
  checkEq(ChordTransposer::transposeNote(55, f, rFix), 57, "root fixed G3->A3");

  // HIGH KEY = F: F# derruba uma oitava
  auto rHigh = rule(NtrRule::rootTrans, NttTable::bypass, 5);
  checkEq(ChordTransposer::transposeNote(60, Chord(6, ChordQuality::maj), rHigh), 54,
          "high key F# uma oitava abaixo");

  // Limites de nota: dobra a oitava para dentro da faixa
  auto rLim = rule(NtrRule::rootTrans, NttTable::bypass, 6, 40, 60);
  const int lim = ChordTransposer::transposeNote(72, Chord(0, ChordQuality::maj), rLim);
  check(lim >= 40 && lim <= 60, "limite de nota dobra a oitava");

  // Melody mantém tons de acorde
  auto rMel = rule(NtrRule::rootTrans, NttTable::melody);
  for (int n : {60, 64, 67}) {
    const int out = ChordTransposer::transposeNote(n, f, rMel);
    const auto pcs = f.pitchClasses();
    bool ok = false;
    for (int pc : pcs)
      if (out % 12 == pc) ok = true;
    check(ok, "melody mantém tom de acorde");
  }

  // Bass On em acorde slash: C/E faz o baixo soar E
  auto rBass = rule(NtrRule::rootTrans, NttTable::bass, 6, 0, 127, true);
  const int slash =
      ChordTransposer::transposeNote(36, Chord(0, ChordQuality::maj, 4), rBass);
  checkEq(slash % 12, 4, "bass on C/E segue o baixo");

  // Melodic minor: E4 em Am -> C
  auto rMM = rule(NtrRule::rootTrans, NttTable::melodicMinor);
  checkEq(ChordTransposer::transposeNote(64, Chord(9, ChordQuality::min), rMM) % 12, 0,
          "melodic minor E->C em Am");
  // Harmonic minor: A4 em Am -> F
  auto rHM = rule(NtrRule::rootTrans, NttTable::harmonicMinor);
  checkEq(ChordTransposer::transposeNote(69, Chord(9, ChordQuality::min), rHM) % 12, 5,
          "harmonic minor A->F em Am");

  // Saída sempre dentro de 0..127
  for (int root = 0; root < 12; ++root)
    for (int qi = 0; qi < qualityCount(); ++qi)
      for (int n : {0, 24, 60, 96, 127})
        for (int ti = 0; ti < 14; ++ti) {
          const auto out = ChordTransposer::transposeNote(
              n, Chord(root, static_cast<ChordQuality>(qi)),
              rule(static_cast<NtrRule>(ti % 3),
                   static_cast<NttTable>(ti)));
          if (out >= 0) check(out >= 0 && out <= 127, "faixa MIDI 0..127");
        }
}

static void testMasks() {
  std::printf("[mascaras de mute / padroes alternativos]\n");
  auto rule = [](int chordMute) {
    CtabRule r;
    r.sourceChannel = 1;
    r.name = "test";
    r.destChannel = 11;
    r.sourceRoot = 0;
    r.sourceChordType = 2;
    r.zones.emplace_back(NtrRule::rootTrans, NttTable::melody, 6, 0, 127,
                         RtrRule::pitchShift, false);
    r.chordMute = chordMute;
    return r;
  };
  const int64_t kMinor = 0x00007FF00; // 11 tipos menores
  const int64_t kMajor = 0x3FFF800FF; // os outros 23

  CtabRule rm = rule(static_cast<int>(kMinor));
  CtabRule rj = rule(static_cast<int>(kMajor));
  bool allMinor = true, allMajor = true, exclusive = true, covered = true;
  for (int t = 0; t < 34; ++t) {
    const bool m = rm.playsChord(0, t);
    const bool j = rj.playsChord(0, t);
    if (m != isMinorTypeId(t)) allMinor = false;
    if (j != !isMinorTypeId(t)) allMajor = false;
    if (m && j) exclusive = false;
    if (!m && !j) covered = false;
  }
  check(allMinor, "mascara menor cobre exatamente os 11 tipos menores");
  check(allMajor, "mascara maior cobre os outros 23");
  check(exclusive, "maior e menor nunca tocam juntos");
  check(covered, "o par cobre os 34 tipos");

  // note mute por tonica
  CtabRule rn = rule(0);
  rn.noteMute = (1 << 0) | (1 << 4) | (1 << 7);
  check(rn.playsChord(0, 0) && rn.playsChord(4, 0) && rn.playsChord(7, 0),
        "note mute libera C/E/G");
  check(!rn.playsChord(1, 0) && !rn.playsChord(11, 0), "note mute bloqueia C#/B");
  rn.noteMute = 0;
  check(rn.playsChord(5, 0), "note mute 0 = sem restricao");

  // ignoreMute
  CtabRule restrictive = rule(0);
  restrictive.chordMute = (1 << 0) | (1 << 2) | (1 << 8) | (1 << 10);
  check(ChordTransposer::transposeNote(60, Chord(0, ChordQuality::dom9), restrictive) < 0,
        "acorde nao coberto -> silencia");
  check(ChordTransposer::transposeNote(60, Chord(0, ChordQuality::dom9), restrictive,
                                       true) >= 0,
        "ignoreMute -> toca o padrao escolhido");
}

static void testStyParse() {
  std::printf("[parser .sty]\n");
  // A amostra pode estar em varios lugares, dependendo de onde o teste roda.
  const char* candidates[] = {"tests/assets/PianoBallad.sty",
                              "test/assets/PianoBallad.sty",
                              "../tests/assets/PianoBallad.sty",
                              "../../tests/assets/PianoBallad.sty"};
  std::string path;
  for (const char* c : candidates) {
    std::FILE* f = std::fopen(c, "rb");
    if (f) {
      std::fclose(f);
      path = c;
      break;
    }
  }
  if (path.empty()) path = "tests/assets/PianoBallad.sty";

  YamahaStyle st;
  const bool ok = StyParser::parseFile(path, st);
  check(ok, "le PianoBallad.sty (" + path + ")");
  if (!ok) {
    std::printf("  (arquivo de amostra ausente - rode a partir da raiz do "
                "projeto)\n");
    return;
  }
  checkEq(st.ppq, 1920, "PPQ 1920");
  checkEq(st.formatVersion, 1, "SFF1");
  check(st.standard == SoundStandard::xg, "padrao detectado: XG");
  check(!st.setupSysex.empty(), "SysEx de setup capturadas");
  check(st.sectionByName("Main A") != nullptr, "seção Main A existe");
  check(st.sectionByName("Fill In AA") != nullptr, "seção Fill In AA existe");
  check(st.sectionByName("Ending B") != nullptr, "seção Ending B existe");

  const CasmSegment* seg = st.casmFor("Main A");
  check(seg != nullptr, "CSEG de Main A");
  if (!seg) return;

  // src#1 'FolkROOT' -> dest 14, ROOT TRANS + Bass, highKey F
  const CtabRule* r1 = seg->ruleFor(1);
  check(r1 != nullptr, "regra do canal 1");
  if (r1) {
    checkStr(r1->name, "FolkROOT", "nome FolkROOT");
    checkEq(r1->destChannel, 14, "destino 14");
    checkEq(r1->sourceRoot, 0, "source root C");
    checkEq(r1->sourceChordType, 2, "source type M7");
    checkEq(static_cast<int>(r1->zones.size()), 1, "1 zona (SFF1)");
    check(r1->zones[0].ntr == NtrRule::rootTrans, "NTR ROOT TRANS");
    check(r1->zones[0].ntt == NttTable::bass, "NTT Bass");
    check(r1->zones[0].bassOn, "Bass On");
    checkEq(r1->zones[0].highKey, 5, "high key F");
    checkEq(r1->zones[0].noteLow, 0, "note low 0");
    checkEq(r1->zones[0].noteHigh, 127, "note high 127");
  }
  // src#2 'pad51' -> dest 13, ROOT TRANS + Chord, highKey D
  const CtabRule* r2 = seg->ruleFor(2);
  check(r2 != nullptr && r2->destChannel == 13, "pad51 -> destino 13");
  if (r2) {
    check(r2->zones[0].ntt == NttTable::chord, "pad51 NTT Chord");
    checkEq(r2->zones[0].highKey, 2, "pad51 high key D");
  }
  // src#8 'AD Dance' (bateria) -> dest 8, ROOT FIXED + BYPASS
  const CtabRule* r8 = seg->ruleFor(8);
  check(r8 != nullptr && r8->destChannel == 8, "AD Dance -> destino 8");
  if (r8) {
    check(r8->zones[0].ntr == NtrRule::rootFixed, "AD Dance ROOT FIXED");
    check(r8->zones[0].ntt == NttTable::bypass, "AD Dance BYPASS");
  }
  // src#10 'E Bass' -> dest 10, Bass, limites 28-39
  const CtabRule* r10 = seg->ruleFor(10);
  check(r10 != nullptr && r10->destChannel == 10, "E Bass -> destino 10");
  if (r10) {
    check(r10->zones[0].ntt == NttTable::bass, "E Bass NTT Bass");
    checkEq(r10->zones[0].noteLow, 28, "E Bass note low 28");
    checkEq(r10->zones[0].noteHigh, 39, "E Bass note high 39");
  }
  // Nenhum valor absurdo (era o bug do parser desalinhado)
  bool sane = true;
  for (const auto& s : st.casm)
    for (const auto& r : s.rules) {
      if (r.destChannel < 0 || r.destChannel > 15) sane = false;
      for (const auto& z : r.zones)
        if (z.highKey > 11 || z.noteLow > 127 || z.noteHigh > 127) sane = false;
    }
  check(sane, "nenhum valor absurdo no CASM");

  // Todos os destinos são canais de acompanhamento (9..16)
  bool destOk = true;
  for (const auto& s : st.casm)
    for (const auto& r : s.rules)
      if (r.destChannel < 8) destOk = false;
  check(destOk, "todos os destinos em 9..16");
}

static void testPlayback() {
  std::printf("[sequenciador]\n");
  const char* candidates[] = {"tests/assets/PianoBallad.sty",
                              "test/assets/PianoBallad.sty",
                              "../tests/assets/PianoBallad.sty",
                              "../../tests/assets/PianoBallad.sty"};
  std::string path;
  for (const char* c : candidates) {
    std::FILE* f = std::fopen(c, "rb");
    if (f) { std::fclose(f); path = c; break; }
  }
  if (path.empty()) path = "tests/assets/PianoBallad.sty";
  YamahaStyle st;
  if (!StyParser::parseFile(path, st)) {
    std::printf("  (amostra ausente - pulando)\n");
    return;
  }
  StylePlayer player(std::make_shared<YamahaStyle>(st));
  check(player.drumChannels1Based().size() >= 1, "canais de bateria detectados");

  // Toca Main A em C e conta eventos por canal de destino
  player.setChord(Chord(0, ChordQuality::maj));
  player.play();
  std::map<int, int> byChannel;
  int notes = 0, drums = 0;
  const int block = 64;
  for (int b = 0; b < 400; ++b) { // ~400 blocos = alguns compassos
    std::vector<StylePlayer::MidiEv> evs;
    player.advance(block, static_cast<double>(b * 40),
                   static_cast<double>((b + 1) * 40), evs);
    for (const auto& e : evs) {
      if ((e.status & 0xF0) == 0x90 && e.data2 > 0) {
        ++notes;
        ++byChannel[e.channel];
        if (e.channel == 8 || e.channel == 9) ++drums;
      }
    }
  }
  check(notes > 20, "gerou notas (" + std::to_string(notes) + ")");
  check(byChannel.count(14) > 0, "canal 14 (destino de FolkROOT) toca");
  check(byChannel.count(13) > 0, "canal 13 (destino de pad51) toca");
  check(byChannel.count(10) > 0, "canal 10 (destino de E Bass) toca");
  check(drums > 0, "bateria toca sem transpor");
  check(byChannel.count(1) == 0, "canal 1 (origem) NAO toca - vai para o destino");

  // Bateria não transpõe: com acorde Am as notas de bateria continuam iguais
  std::map<int, int> drumNotesC, drumNotesAm;
  StylePlayer p2(std::make_shared<YamahaStyle>(st));
  p2.setChord(Chord(0, ChordQuality::maj));
  p2.play();
  for (int b = 0; b < 200; ++b) {
    std::vector<StylePlayer::MidiEv> evs;
    p2.advance(block, static_cast<double>(b * 40), static_cast<double>((b + 1) * 40), evs);
    for (const auto& e : evs)
      if ((e.status & 0xF0) == 0x90 && e.data2 > 0 && (e.channel == 8 || e.channel == 9))
        ++drumNotesC[e.data1];
  }
  StylePlayer p3(std::make_shared<YamahaStyle>(st));
  p3.setChord(Chord(9, ChordQuality::min));
  p3.play();
  for (int b = 0; b < 200; ++b) {
    std::vector<StylePlayer::MidiEv> evs;
    p3.advance(block, static_cast<double>(b * 40), static_cast<double>((b + 1) * 40), evs);
    for (const auto& e : evs)
      if ((e.status & 0xF0) == 0x90 && e.data2 > 0 && (e.channel == 8 || e.channel == 9))
        ++drumNotesAm[e.data1];
  }
  check(drumNotesC == drumNotesAm, "bateria igual em C e em Am (nunca transpõe)");

  // Fila no estado parado: Intro A + Main C
  StylePlayer p4(std::make_shared<YamahaStyle>(st));
  p4.selectSection("Intro A");
  p4.selectSection("Main C");
  auto q = p4.startQueue();
  checkEq(static_cast<int>(q.size()), 2, "fila com 2 seções");
  if (q.size() == 2) {
    checkStr(q[0], "Intro A", "fila[0] = Intro A");
    checkStr(q[1], "Main C", "fila[1] = Main C");
  }
  p4.play();
  checkStr(p4.currentSectionName(), "Intro A", "toca Intro A primeiro");
  // avança até o fim do intro
  for (int b = 0; b < 600; ++b) {
    std::vector<StylePlayer::MidiEv> evs;
    p4.advance(block, static_cast<double>(b * 40), static_cast<double>((b + 1) * 40), evs);
  }
  checkStr(p4.currentSectionName(), "Main C", "depois entra em Main C");

  // Encadeamento durante a execução: Fill B -> Main C
  StylePlayer p5(std::make_shared<YamahaStyle>(st));
  p5.selectSection("Main A", true);
  p5.play();
  p5.selectSection("Fill In BB");
  p5.selectSection("Main C");
  auto q5 = p5.startQueue();
  checkEq(static_cast<int>(q5.size()), 2, "fila ao vivo com 2");
  if (q5.size() == 2) {
    checkStr(q5[0], "Fill In BB", "fila[0] = Fill In BB");
    checkStr(q5[1], "Main C", "fila[1] = Main C");
  }
}

int main() {
  std::printf("=== Yamaha Arranger - testes do motor ===\n\n");
  testChordDetector();
  testTransposer();
  testMasks();
  testStyParse();
  testPlayback();
  std::printf("\n=== resultado: %d ok, %d falhas ===\n", gPass, gFail);
  return gFail == 0 ? 0 : 1;
}
