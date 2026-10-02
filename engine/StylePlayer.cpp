#include "StylePlayer.h"

#include <algorithm>
#include <cctype>
#include <cmath>

#include "ChordTransposer.h"

namespace yamaha {

namespace {

bool isMain(const std::string& n) { return n.rfind("Main", 0) == 0; }
bool isIntro(const std::string& n) { return n.rfind("Intro", 0) == 0; }
bool isFill(const std::string& n) { return n.rfind("Fill", 0) == 0; }
bool isEnding(const std::string& n) { return n.rfind("Ending", 0) == 0; }

/// Offset em amostras de um evento que ocorre no tick [eventTick] dentro do
/// intervalo [fromTick, toTick] percorrido neste bloco.
int eventOffsetSamples(int eventTick, double fromTick, double toTick,
                       int numSamples) {
  const double span = toTick - fromTick;
  if (span <= 0.0) return numSamples;
  double t = (static_cast<double>(eventTick) - fromTick) / span;
  if (t < 0.0) t = 0.0;
  if (t > 1.0) t = 1.0;
  return static_cast<int>(t * numSamples);
}

} // namespace

// ---------------------------------------------------------------------------
// Carregamento / detecção
// ---------------------------------------------------------------------------

void StylePlayer::loadStyle(YamahaStylePtr s) {
  stop();
  style_ = std::move(s);
  if (style_) tempoBpm_ = style_->tempoBpm;
  detectDrumChannels();
  detectActiveChannels();
  mutedChannels.clear();
  if (style_) selectSection("Main A", true);
}

std::vector<int> StylePlayer::drumChannels1Based() const {
  std::vector<int> out;
  for (int c : drumChannels_) out.push_back(c + 1);
  std::sort(out.begin(), out.end());
  return out;
}

int StylePlayer::destOf(int srcChannel) const {
  auto it = srcToDest_.find(srcChannel);
  return it == srcToDest_.end() ? srcChannel : it->second;
}

void StylePlayer::detectDrumChannels() {
  drumChannels_.clear();
  bankMsb_.clear();
  srcToDest_.clear();
  if (!style_) return;

  for (const auto& seg : style_->casm)
    for (const auto& r : seg.rules) srcToDest_[r.sourceChannel] = r.destChannel;

  for (const auto& ev : style_->setupEvents) {
    if (ev.type() == 0xB0 && ev.data1 == 0) {
      const int dest = destOf(ev.channel());
      bankMsb_[dest] = ev.data2;
      // 127 = XG Drum, 126 = XG SFX, 120 = GS Drum
      if (ev.data2 == 127 || ev.data2 == 126 || ev.data2 == 120)
        drumChannels_.insert(dest);
    }
  }
  // Convenção SFF: canais 9 e 10 (0-based 8 e 9) são Rhythm 1/2.
  for (int ch : {8, 9}) {
    auto it = bankMsb_.find(ch);
    if (it != bankMsb_.end() && it->second < 120) continue;
    drumChannels_.insert(ch);
  }
}

void StylePlayer::detectActiveChannels() {
  activeChannels.clear();
  if (!style_) return;
  for (const auto& seg : style_->casm)
    for (const auto& r : seg.rules) activeChannels.insert(r.destChannel);
  for (const auto& sec : style_->sections)
    for (const auto& ev : sec.events) activeChannels.insert(destOf(ev.channel()));
}

int StylePlayer::ticksPerBar() const {
  if (!style_) return 0;
  const double beatsPerBar = style_->timeSigNum * (4.0 / style_->timeSigDen);
  return static_cast<int>(style_->ppq * beatsPerBar);
}

// ---------------------------------------------------------------------------
// Transporte
// ---------------------------------------------------------------------------

void StylePlayer::setTempo(double bpm) {
  if (bpm < 40.0) bpm = 40.0;
  if (bpm > 300.0) bpm = 300.0;
  tempoBpm_ = bpm;
}

void StylePlayer::selectSection(const std::string& name, bool immediate) {
  if (!style_ || !style_->sectionByName(name)) return;
  if (isMain(name)) lastMainName_ = name;

  if (immediate) {
    clearStartQueue();
    activateSection(name);
    return;
  }

  if (!playing_) {
    // Estado parado: monta uma sequência (toggle).
    auto it = std::find(startQueue_.begin(), startQueue_.end(), name);
    if (it != startQueue_.end()) {
      startQueue_.erase(it);
    } else {
      startQueue_.push_back(name);
    }
    if (!startQueue_.empty()) prepareSection(startQueue_.front());
    return;
  }

  // Em reprodução: 1º toque enfileira; 2º toque na mesma antecipa; tocar outra
  // seção encadeia (Fill B -> Main C).
  if (queuedSection_ == name) {
    queuedImmediate_ = true;
  } else if (!queuedSection_.empty()) {
    auto it = std::find(playSequence_.begin(), playSequence_.end(), name);
    if (it != playSequence_.end()) {
      playSequence_.erase(it);
    } else {
      playSequence_.push_back(name);
    }
    notifyPlayQueue();
  } else {
    queuedSection_ = name;
    queuedImmediate_ = false;
    notifyPlayQueue();
  }
}

void StylePlayer::notifyPlayQueue() {
  // A UI lê startQueue(): 1º item = seção enfileirada, demais = encadeadas.
  std::vector<std::string> list;
  if (!queuedSection_.empty()) list.push_back(queuedSection_);
  for (const auto& s : playSequence_) list.push_back(s);
  if (list.empty()) {
    for (const auto& s : startQueue_) list.push_back(s);
  }
  startQueue_ = list;
}

void StylePlayer::clearStartQueue() { startQueue_.clear(); }

void StylePlayer::activateSection(const std::string& name) {
  if (!style_) return;
  const StyleSection* s = style_->sectionByName(name);
  if (!s) return;
  section_ = s;
  casm_ = style_->casmFor(name);
  cursor_ = 0;
  tick_ = 0;
  tickAccum_ = 0;
  currentSectionName_ = name;
}

void StylePlayer::prepareSection(const std::string& name) {
  if (!style_) return;
  const StyleSection* s = style_->sectionByName(name);
  if (!s) return;
  section_ = s;
  casm_ = style_->casmFor(name);
  cursor_ = 0;
  tick_ = 0;
  currentSectionName_ = name;
}

void StylePlayer::play() {
  if (!style_) return;
  if (!startQueue_.empty()) {
    const std::string first = startQueue_.front();
    playSequence_.assign(startQueue_.begin() + 1, startQueue_.end());
    clearStartQueue();
    activateSection(first);
    notifyPlayQueue();
  }
  if (!section_) return;
  playing_ = true;
  tick_ = 0;
  cursor_ = 0;
  tickAccum_ = 0;
}

void StylePlayer::stop() {
  playing_ = false;
  section_ = nullptr;
  cursor_ = 0;
  tick_ = 0;
  tickAccum_ = 0;
  queuedSection_.clear();
  queuedImmediate_ = false;
  playSequence_.clear();
  startQueue_.clear();
  currentSectionName_ = "-";
  measure_ = 1;
  beat_ = 1;
  sounding_.clear();
}

std::string StylePlayer::nextSectionAfter(const std::string& current) {
  if (!queuedSection_.empty()) {
    const std::string q = queuedSection_;
    queuedSection_.clear();
    queuedImmediate_ = false;
    notifyPlayQueue();
    return q;
  }
  if (!playSequence_.empty()) {
    const std::string next = playSequence_.front();
    playSequence_.erase(playSequence_.begin());
    notifyPlayQueue();
    return next;
  }
  if (isIntro(current) || isFill(current)) return lastMainName_;
  return current; // Main: loop
}

// ---------------------------------------------------------------------------
// Avanço
// ---------------------------------------------------------------------------

void StylePlayer::advance(int numSamples, double startTick, double endTick,
                          std::vector<MidiEv>& out) {
  (void)startTick;
  (void)endTick;
  if (!playing_ || !section_ || !style_) return;
  const double delta = endTick - startTick;
  if (delta <= 0.0) return;

  tickAccum_ += delta;
  const double advanceTicks = std::floor(tickAccum_);
  if (advanceTicks <= 0.0) return;
  tickAccum_ -= advanceTicks;

  const double prevTick = tick_;
  const double target = tick_ + advanceTicks;

  // Compasso/batida para o display.
  const int ppq = style_->ppq;
  const int beatsPerBar =
      static_cast<int>(style_->timeSigNum * (4.0 / style_->timeSigDen));
  if (ppq > 0 && beatsPerBar > 0) {
    measure_ = static_cast<int>(target / (ppq * beatsPerBar)) + 1;
    beat_ = (static_cast<int>(target / ppq) % beatsPerBar) + 1;
  }

  while (cursor_ < static_cast<int>(section_->events.size()) &&
         section_->events[static_cast<size_t>(cursor_)].tick <= target) {
    fire(section_->events[static_cast<size_t>(cursor_)], prevTick, target,
         numSamples, out);
    ++cursor_;
  }
  tick_ = target;

  // Antecipação: 2º toque na seção pendente entra no próximo compasso.
  if (queuedImmediate_ && !queuedSection_.empty()) {
    const int bar = ticksPerBar();
    if (bar > 0 &&
        (static_cast<int>(prevTick / bar) != static_cast<int>(tick_ / bar))) {
      allOff(out, numSamples);
      const std::string next = queuedSection_;
      queuedSection_.clear();
      queuedImmediate_ = false;
      notifyPlayQueue();
      activateSection(next);
      return;
    }
  }

  if (tick_ >= section_->lengthTicks()) {
    allOff(out, numSamples);
    tick_ = 0;
    cursor_ = 0;
    const std::string current = currentSectionName_;
    const std::string next = nextSectionAfter(current);
    if (isEnding(current) && next == current) {
      stop();
      return;
    }
    activateSection(next);
  }
}

void StylePlayer::allOff(std::vector<MidiEv>& out, int numSamples) {
  for (const auto& kv : sounding_) {
    MidiEv ev;
    ev.sampleOffset = numSamples;
    ev.channel = kv.first >> 8;
    ev.status = 0x80 | ev.channel;
    ev.data1 = kv.second;
    out.push_back(ev);
  }
  sounding_.clear();
}

// ---------------------------------------------------------------------------
// Disparo de eventos
// ---------------------------------------------------------------------------

void StylePlayer::fire(const StyleEvent& ev, double fromTick, double toTick,
                       int numSamples, std::vector<MidiEv>& out) {
  const int srcChannel = ev.channel(); // 0-based
  const CtabRule* rule = casm_ ? casm_->ruleFor(srcChannel) : nullptr;

  // Estilo COM CASM: só os canais mapeados naquela seção tocam (é assim que os
  // teclados Yamaha funcionam). Um canal sem regra não foi pensado para essa
  // seção; tocar com regra inventada jogaria a parte no canal errado.
  CtabRule localFallback;
  if (rule == nullptr) {
    if (casm_ && !casm_->rules.empty()) return;
    localFallback = defaultRule(srcChannel); // estilo sem CASM
    rule = &localFallback;
  }

  int outCh = rule->destChannel;
  bool ignoreMute = false;

  // --- Seleção de padrão (note mute / chord mute) ---------------------------
  if (casm_) {
    const int typeId = yamahaTypeId(currentChord_.quality);
    if (!rule->playsChord(currentChord_.root, typeId)) {
      if (segmentCoversChord(casm_, typeId)) return;
      const CtabRule* alt = nearestRuleFor(casm_, rule->destChannel, typeId);
      if (alt != nullptr) {
        rule = alt;
        ignoreMute = true;
      } else {
        // Nenhuma regra cobre: toca como gravado no próprio canal.
        CtabRule pass = passthroughRule(srcChannel);
        rule = &pass;
        outCh = srcChannel;
        ignoreMute = true;
        fireEvent(ev, outCh, fromTick, toTick, numSamples, out, *rule, ignoreMute);
        return;
      }
    }
  }

  fireEvent(ev, outCh, fromTick, toTick, numSamples, out, *rule, ignoreMute);
}

void StylePlayer::fireEvent(const StyleEvent& ev, int outCh, double fromTick,
                            double toTick, int numSamples,
                            std::vector<MidiEv>& out, const CtabRule& rule,
                            bool ignoreMute) {
  const int srcChannel = ev.channel();
  const int key = (srcChannel << 8) | ev.data1;
  const int offset = eventOffsetSamples(ev.tick, fromTick, toTick, numSamples);

  if (ev.type() == 0x90 && ev.data2 > 0) {
    if (mutedChannels.count(outCh)) return; // canal silenciado pelo usuário

    int sendNote;
    const bool asWritten = isDrumChannel(srcChannel, &rule) || playsAsWritten(&rule);
    if (asWritten) {
      sendNote = ev.data1; // bateria / "toca como gravado": nunca transpõe
    } else {
      sendNote = ChordTransposer::transposeNote(ev.data1, currentChord_, rule,
                                                ignoreMute);
      if (sendNote < 0) return; // note/chord mute do CASM
    }
    auto it = sounding_.find(key);
    if (it != sounding_.end()) {
      MidiEv off;
      off.sampleOffset = offset;
      off.channel = outCh;
      off.status = 0x80 | outCh;
      off.data1 = it->second;
      out.push_back(off);
    }
    MidiEv on;
    on.sampleOffset = offset;
    on.channel = outCh;
    on.status = 0x90 | outCh;
    on.data1 = sendNote;
    on.data2 = ev.data2;
    out.push_back(on);
    sounding_[key] = sendNote;
    return;
  }

  if (ev.isNoteOff()) {
    auto it = sounding_.find(key);
    MidiEv off;
    off.sampleOffset = offset;
    off.channel = outCh;
    off.status = 0x80 | outCh;
    off.data1 = (it != sounding_.end()) ? it->second : ev.data1;
    out.push_back(off);
    if (it != sounding_.end()) sounding_.erase(it);
    return;
  }

  if (ev.type() == 0xC0 || ev.type() == 0xB0 || ev.type() == 0xE0) {
    MidiEv m;
    m.sampleOffset = offset;
    m.channel = outCh;
    m.status = ev.status;
    m.data1 = ev.data1;
    m.data2 = ev.data2;
    out.push_back(m);
  }
}

bool StylePlayer::playsAsWritten(const CtabRule* rule) const {
  if (!rule) return false;
  for (int k = 0; k <= 127; k += 12) {
    const CasmZone& z = rule->zoneFor(k);
    if (z.ntt == NttTable::bypass && z.ntr != NtrRule::rootTrans) return true;
  }
  return false;
}

bool StylePlayer::isDrumChannel(int srcChannel, const CtabRule* rule) const {
  const int dest = rule ? rule->destChannel : srcChannel;
  if (drumChannels_.count(dest)) return true;
  auto it = bankMsb_.find(dest);
  if (it != bankMsb_.end() && it->second < 120) return false;

  if (rule) {
    static const char* keywords[] = {"drum", "drm", "rhythm", "rhy",
                                     "perc", "kit", "beat", "dance"};
    std::string lower = rule->name;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    for (const char* k : keywords)
      if (lower.find(k) != std::string::npos) return true;
  }
  return dest == 8 || dest == 9; // convenção SFF (Rhythm 1 e 2)
}

bool StylePlayer::segmentCoversChord(const CasmSegment* seg, int chordTypeId) const {
  for (const auto& r : seg->rules)
    if (r.playsChord(currentChord_.root, chordTypeId)) return true;
  return false;
}

const CtabRule* StylePlayer::nearestRuleFor(const CasmSegment* seg, int destChannel,
                                            int chordTypeId) const {
  const bool minor = isMinorTypeId(chordTypeId);
  const CtabRule* best = nullptr;
  int bestCount = -1;
  for (const auto& r : seg->rules) {
    if (r.destChannel != destChannel) continue;
    int count = 0;
    for (int t = 0; t < 34; ++t) {
      if (isMinorTypeId(t) == minor && r.playsChord(currentChord_.root, t)) ++count;
    }
    if (count > bestCount) {
      bestCount = count;
      best = &r;
    }
  }
  return (best != nullptr && bestCount > 0) ? best : nullptr;
}

CtabRule StylePlayer::passthroughRule(int sourceChannel) const {
  CtabRule r;
  r.sourceChannel = sourceChannel & 0x0F;
  r.name = "passthru";
  r.destChannel = r.sourceChannel;
  r.sourceRoot = 0;
  r.sourceChordType = 2;
  r.zones.emplace_back(NtrRule::rootFixed, NttTable::bypass, 6, 0, 127,
                       RtrRule::pitchShift, false);
  return r;
}

CtabRule StylePlayer::defaultRule(int sourceChannel) const {
  const int ch = sourceChannel & 0x0F;
  NtrRule ntr;
  NttTable ntt;
  bool bassOn = false;
  if (ch == 8 || ch == 9) {
    ntr = NtrRule::rootFixed;
    ntt = NttTable::bypass;
  } else if (ch == 10) {
    ntr = NtrRule::rootTrans;
    ntt = NttTable::melody;
    bassOn = true;
  } else if (ch >= 11 && ch <= 13) {
    ntr = NtrRule::rootFixed;
    ntt = NttTable::chord;
  } else {
    ntr = NtrRule::rootTrans;
    ntt = NttTable::melody;
  }
  CtabRule r;
  r.sourceChannel = ch;
  r.name = "default";
  r.destChannel = (ch >= 8) ? ch : (ch | 0x08);
  r.sourceRoot = 0;
  r.sourceChordType = 2;
  r.zones.emplace_back(ntr, ntt, 6, 0, 127, RtrRule::pitchShift, bassOn);
  return r;
}

} // namespace yamaha
