#include "PluginProcessor.h"

#include <JuceHeader.h>

#include "PluginEditor.h"

namespace yamaha {

YamahaArrangerProcessor::YamahaArrangerProcessor() {
  synth_.setVolume(volume_.load());
  synth_.setReverb(reverb_.load());
}

YamahaArrangerProcessor::~YamahaArrangerProcessor() = default;

void YamahaArrangerProcessor::prepareToPlay(double sampleRate, int) {
  synth_.setSampleRate(static_cast<float>(sampleRate));
  synth_.setVolume(volume_.load());
  synth_.setReverb(reverb_.load());
  ppqValid_ = false;
}

void YamahaArrangerProcessor::releaseResources() { synth_.allOff(); }

void YamahaArrangerProcessor::takePendingStyle() {
  juce::ScopedLock sl(shared_.lock);
  if (!shared_.styleChanged) return;
  shared_.styleChanged = false;
  player_.loadStyle(shared_.pendingStyle);
  shared_.statusMessage =
      shared_.pendingStyle ? "Estilo carregado" : "Nenhum estilo";
}

void YamahaArrangerProcessor::loadStyleFile(const juce::File& file) {
  // Carrega FORA da thread de áudio (a UI chama isto).
  auto style = std::make_shared<YamahaStyle>();
  const std::string path = file.getFullPathName().toStdString();
  if (!StyParser::parseFile(path, *style)) {
    juce::ScopedLock sl(shared_.lock);
    shared_.statusMessage =
        "Falha ao ler " + file.getFileName().toStdString();
    return;
  }
  {
    juce::ScopedLock sl(shared_.lock);
    shared_.pendingStyle = style;
    shared_.styleChanged = true;
    shared_.statusMessage = "Carregando " + file.getFileName().toStdString();
  }
  takePendingStyle(); // aplica já na thread de mensagem
}

void YamahaArrangerProcessor::loadSoundFontFile(const juce::File& file) {
  const bool ok = synth_.loadFile(file.getFullPathName().toStdString());
  juce::ScopedLock sl(shared_.lock);
  shared_.statusMessage = ok ? "SoundFont: " + file.getFileName().toStdString()
                             : "Falha no SoundFont";
}

void YamahaArrangerProcessor::selectSection(const juce::String& name,
                                            bool immediate) {
  player_.selectSection(name.toStdString(), immediate);
}

void YamahaArrangerProcessor::setPlaying(bool shouldPlay) {
  playing_.store(shouldPlay);
  if (shouldPlay) {
    player_.play();
  } else {
    player_.stop();
    synth_.allOff();
  }
}

void YamahaArrangerProcessor::toggleMute(int channel0Based) {
  auto& m = player_.mutedChannels;
  if (m.count(channel0Based))
    m.erase(channel0Based);
  else
    m.insert(channel0Based);
  if (m.count(channel0Based))
    for (int n = 0; n < 128; ++n) synth_.noteOff(channel0Based, n);
}

void YamahaArrangerProcessor::panic() {
  synth_.allOff();
  player_.stop();
  playing_.store(false);
}

void YamahaArrangerProcessor::setVolume(float v) {
  volume_.store(v);
  synth_.setVolume(v);
}

void YamahaArrangerProcessor::setReverb(float v) {
  reverb_.store(v);
  synth_.setReverb(v);
}

void YamahaArrangerProcessor::nudgeTempo(double steps) {
  // Passos de 5 BPM por clique do knob.
  player_.setTempo(player_.tempoBpm() + steps * 5.0);
}

void YamahaArrangerProcessor::setSplitNote(int note) { splitNote_.store(note); }

juce::String YamahaArrangerProcessor::currentChordLabel() const {
  return juce::String(player_.chord().label());
}

// ---------------------------------------------------------------------------
// Áudio
// ---------------------------------------------------------------------------

void YamahaArrangerProcessor::handleIncomingMidi(const juce::MidiBuffer& midi,
                                                 int numSamples,
                                                 double startPpq,
                                                 double endPpq) {
  const int split = splitNote_.load();
  static ChordDetector detector(FingeringMode::aiFingered);

  for (const auto metadata : midi) {
    const juce::MidiMessage msg = metadata.getMessage();
    if (!msg.isNoteOnOrOff()) continue;
    const int note = msg.getNoteNumber();

    if (msg.isNoteOn()) {
      if (note < split) {
        heldChordNotes_.push_back(note); // zona de acordes
      } else {
        synth_.noteOn(0, note, msg.getVelocity()); // mão direita passa direto
      }
    } else { // note off
      if (note < split) {
        for (auto it = heldChordNotes_.begin(); it != heldChordNotes_.end(); ++it) {
          if (*it == note) {
            heldChordNotes_.erase(it);
            break;
          }
        }
      } else {
        synth_.noteOff(0, note);
      }
    }
  }

  Chord detected;
  if (detector.detect(heldChordNotes_, detected)) player_.setChord(detected);

  // Avança o sequenciador com a variação de PPQ do host.
  std::vector<StylePlayer::MidiEv> styleEvents;
  const double delta = endPpq - startPpq;
  if (playing_.load() && player_.hasStyle() && ppqValid_ && delta > 0.0 &&
      delta < 100000.0) {
    player_.advance(numSamples, startPpq, startPpq + delta, styleEvents);
  }

  for (const auto& e : styleEvents) {
    const int type = e.status & 0xF0;
    if (type == 0x90 && e.data2 > 0) {
      synth_.noteOn(e.channel, e.data1, e.data2);
    } else if (type == 0x80 || (type == 0x90 && e.data2 == 0)) {
      synth_.noteOff(e.channel, e.data1);
    } else if (type == 0xB0 && e.data1 == 7) {
      synth_.setChannelVolume(e.channel, e.data2);
    }
    // Program Change / Pitch Bend: o TinySoundFont não os implementa; os sons
    // já vêm definidos pelo SoundFont em cada canal.
  }
}

void YamahaArrangerProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                           juce::MidiBuffer& midi) {
  juce::ScopedNoDenormals noDenormals;
  const int numSamples = buffer.getNumSamples();
  const int numChannels = buffer.getNumChannels();

  for (int ch = 0; ch < numChannels; ++ch) buffer.clear(ch, 0, numSamples);

  takePendingStyle();

  // ---- posição no host (PPQ) ---------------------------------------------
  const int ppq = player_.style() ? player_.style()->ppq : 480;
  const double seconds = numSamples / getSampleRate();
  double startPpq = lastPpq_;
  double endPpq = lastPpq_;

  bool haveHostPpq = false;
  if (auto* playHead = getPlayHead()) {
    if (auto pos = playHead->getPosition()) {
      if (pos->getPpqPosition().hasValue()) {
        haveHostPpq = true;
        startPpq = *pos->getPpqPosition();
        const double bpm = pos->getBpm().hasValue() ? *pos->getBpm() : 120.0;
        endPpq = startPpq + (bpm / 60.0) * ppq * seconds;
      }
    }
  }
  if (!haveHostPpq) {
    // Relógio interno (standalone ou host sem PPQ): segue o tempo do estilo.
    const double bpm = player_.tempoBpm();
    endPpq = startPpq + (bpm / 60.0) * ppq * seconds;
  }

  handleIncomingMidi(midi, numSamples, startPpq, endPpq);
  lastPpq_ = endPpq;
  ppqValid_ = true;

  // ---- renderiza o sintetizador ------------------------------------------
  if (numChannels == 0) return;
  juce::HeapBlock<float> interleaved(static_cast<size_t>(numSamples) * 2);
  synth_.renderInterleaved(interleaved, numSamples);

  if (numChannels >= 2) {
    float* left = buffer.getWritePointer(0);
    float* right = buffer.getWritePointer(1);
    for (int i = 0; i < numSamples; ++i) {
      left[i] += interleaved[static_cast<size_t>(i) * 2];
      right[i] += interleaved[static_cast<size_t>(i) * 2 + 1];
    }
  } else {
    float* mono = buffer.getWritePointer(0);
    for (int i = 0; i < numSamples; ++i)
      mono[i] += 0.5f * (interleaved[static_cast<size_t>(i) * 2] +
                         interleaved[static_cast<size_t>(i) * 2 + 1]);
  }
}

juce::AudioProcessorEditor* YamahaArrangerProcessor::createEditor() {
  return new YamahaArrangerEditor(*this);
}

void YamahaArrangerProcessor::getStateInformation(juce::MemoryBlock& dest) {
  dest.setSize(0); // estilo e SoundFont são escolhidos em runtime
}

void YamahaArrangerProcessor::setStateInformation(const void*, int) {}

} // namespace yamaha
