#include "PluginEditor.h"

#include <JuceHeader.h>

namespace yamaha {

namespace {

constexpr int kW = 900;
constexpr int kH = 460;

juce::Colour bg(0xFF101418);
juce::Colour panel(0xFF1B2128);
juce::Colour lcdBg(0xFF0F2A18);
juce::Colour lcdFg(0xFF7CFFA0);
juce::Colour accent(0xFF2E7CF6);
juce::Colour cyan(0xFF35D6E8);
juce::Colour amber(0xFFFFC24B);
juce::Colour red(0xFFFF5A5A);
juce::Colour green(0xFF3DDC84);
juce::Colour textDim(0xFF8A949E);

} // namespace

YamahaArrangerEditor::YamahaArrangerEditor(YamahaArrangerProcessor& p)
    : juce::AudioProcessorEditor(&p), processor_(p) {
  setSize(kW, kH);
  buildUi();
  startTimerHz(30);
}

YamahaArrangerEditor::~YamahaArrangerEditor() { stopTimer(); }

void YamahaArrangerEditor::buildUi() {
  // ---- display ----
  lcd_.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 22.0f,
                          juce::Font::bold));
  lcd_.setColour(juce::Label::backgroundColourId, lcdBg);
  lcd_.setColour(juce::Label::textColourId, lcdFg);
  lcd_.setJustificationType(juce::Justification::centred);
  addAndMakeVisible(lcd_);

  lcdSmall_.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 13.0f,
                               juce::Font::plain));
  lcdSmall_.setColour(juce::Label::backgroundColourId, lcdBg);
  lcdSmall_.setColour(juce::Label::textColourId, lcdFg.withAlpha(0.85f));
  lcdSmall_.setJustificationType(juce::Justification::centred);
  addAndMakeVisible(lcdSmall_);

  status_.setFont(juce::Font(12.0f));
  status_.setColour(juce::Label::textColourId, textDim);
  status_.setJustificationType(juce::Justification::centredLeft);
  addAndMakeVisible(status_);

  // ---- transporte / arquivos ----
  auto setupBtn = [](juce::TextButton& b, juce::Colour c) {
    b.setColour(juce::TextButton::buttonColourId, c);
    b.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
  };
  setupBtn(startBtn_, green);
  setupBtn(stopBtn_, red);
  setupBtn(syncBtn_, panel.brighter(0.2f));
  setupBtn(panicBtn_, amber);
  setupBtn(resetBtn_, panel.brighter(0.1f));
  setupBtn(styleBtn_, accent);
  setupBtn(sf2Btn_, panel.brighter(0.3f));

  startBtn_.onClick = [this] { processor_.setPlaying(true); };
  stopBtn_.onClick = [this] { processor_.setPlaying(false); };
  syncBtn_.onClick = [this] { processor_.setPlaying(true); };
  panicBtn_.onClick = [this] { processor_.panic(); };
  resetBtn_.onClick = [this] {
    processor_.setPlaying(false);
    processor_.panic();
  };
  styleBtn_.onClick = [this] { loadStyleClicked(); };
  sf2Btn_.onClick = [this] { loadSoundFontClicked(); };

  for (auto* b : {&startBtn_, &stopBtn_, &syncBtn_, &panicBtn_, &resetBtn_,
                  &styleBtn_, &sf2Btn_})
    addAndMakeVisible(b);

  // ---- seções ----
  auto setupSection = [this](juce::TextButton& b, const juce::String& name) {
    b.setColour(juce::TextButton::buttonColourId, panel.brighter(0.15f));
    b.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    b.onClick = [this, name] { sectionClicked(name); };
    addAndMakeVisible(b);
  };
  setupSection(introA_, "Intro A");
  setupSection(introB_, "Intro B");
  setupSection(fillA_, "Fill In AA");
  setupSection(fillB_, "Fill In BB");
  setupSection(mainA_, "Main A");
  setupSection(mainB_, "Main B");
  setupSection(mainC_, "Main C");
  setupSection(mainD_, "Main D");
  setupSection(endingA_, "Ending A");
  setupSection(endingB_, "Ending B");

  // ---- knobs ----
  auto setupKnob = [this](juce::Slider& s, juce::Label& l, double from,
                          double to, double value) {
    s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    s.setRange(from, to);
    s.setValue(value, juce::dontSendNotification);
    s.setColour(juce::Slider::rotarySliderFillColourId, accent);
    s.setColour(juce::Slider::thumbColourId, juce::Colours::white);
    l.setFont(juce::Font(11.0f));
    l.setColour(juce::Label::textColourId, textDim);
    l.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(s);
    addAndMakeVisible(l);
  };
  setupKnob(volumeKnob_, volumeLbl_, 0.0, 1.0, 0.8);
  setupKnob(tempoKnob_, tempoLbl_, 40.0, 300.0, 120.0);
  setupKnob(reverbKnob_, reverbLbl_, 0.0, 1.0, 0.25);
  setupKnob(chorusKnob_, chorusLbl_, 0.0, 1.0, 0.0);

  volumeKnob_.onValueChange = [this] {
    processor_.setVolume(static_cast<float>(volumeKnob_.getValue()));
  };
  reverbKnob_.onValueChange = [this] {
    processor_.setReverb(static_cast<float>(reverbKnob_.getValue()));
  };
  tempoKnob_.onValueChange = [this] {
    processor_.player().setTempo(tempoKnob_.getValue());
  };

  // ---- canais ----
  for (int i = 0; i < 16; ++i) {
    channelBtns_[i].setButtonText(juce::String(i + 1));
    channelBtns_[i].setColour(juce::TextButton::buttonColourId,
                              panel.brighter(0.1f));
    channelBtns_[i].setColour(juce::TextButton::textColourOffId,
                              juce::Colours::white);
    channelBtns_[i].onClick = [this, i] { processor_.toggleMute(i); };
    addAndMakeVisible(channelBtns_[i]);
  }
}

void YamahaArrangerEditor::loadStyleClicked() {
  auto chooser = std::make_shared<juce::FileChooser>(
      "Escolha um estilo Yamaha (.sty)", juce::File{}, "*.sty;*.STY");
  chooser->launchAsync(
      juce::FileBrowserComponent::openMode |
          juce::FileBrowserComponent::canSelectFiles,
      [this, chooser](const juce::FileChooser& fc) {
        const juce::File f = fc.getResult();
        if (f.existsAsFile()) processor_.loadStyleFile(f);
      });
}

void YamahaArrangerEditor::loadSoundFontClicked() {
  auto chooser = std::make_shared<juce::FileChooser>(
      "Escolha um SoundFont (.sf2)", juce::File{}, "*.sf2;*.SF2");
  chooser->launchAsync(
      juce::FileBrowserComponent::openMode |
          juce::FileBrowserComponent::canSelectFiles,
      [this, chooser](const juce::FileChooser& fc) {
        const juce::File f = fc.getResult();
        if (f.existsAsFile()) processor_.loadSoundFontFile(f);
      });
}

void YamahaArrangerEditor::sectionClicked(const juce::String& name) {
  processor_.selectSection(name, false);
}

void YamahaArrangerEditor::updateDisplay() {
  auto& player = processor_.player();
  const juce::String styleName = processor_.hasStyle()
                                     ? juce::String(player.style()->fileName)
                                     : "sem estilo";
  const juce::String stdLabel =
      juce::String(soundStandardLabel(player.standard()));

  lcd_.setText(styleName + "   |   " + processor_.currentChordLabel(),
               juce::dontSendNotification);

  lcdSmall_.setText("PADRAO " + stdLabel + "   TEMPO " +
                        juce::String(player.tempoBpm(), 1) + "   COMPASSO " +
                        juce::String(player.currentMeasure()) + "   SECAO " +
                        juce::String(player.currentSectionName()),
                    juce::dontSendNotification);

  {
    juce::ScopedLock sl(processor_.shared().lock);
    status_.setText(processor_.shared().statusMessage,
                    juce::dontSendNotification);
  }

  // Seções enfileiradas piscam e mostram a ordem
  const auto queue = player.startQueue();
  auto queueIndexOf = [&](const juce::String& n) {
    for (size_t i = 0; i < queue.size(); ++i)
      if (queue[i] == n.toStdString()) return static_cast<int>(i);
    return -1;
  };
  auto mark = [&](juce::TextButton& b, const juce::String& name) {
    const int idx = queueIndexOf(name);
    if (idx >= 0) {
      const bool blink = (juce::Time::getMillisecondCounter() / 300) % 2 == 0;
      b.setColour(juce::TextButton::buttonColourId,
                  blink ? cyan : panel.brighter(0.15f));
      b.setButtonText(juce::String(idx + 1) + " " + name);
    } else {
      b.setColour(juce::TextButton::buttonColourId, panel.brighter(0.15f));
      b.setButtonText(name);
    }
  };
  mark(introA_, "INTRO A");
  mark(introB_, "INTRO B");
  mark(fillA_, "FILL A");
  mark(fillB_, "FILL B");
  mark(mainA_, "MAIN A");
  mark(mainB_, "MAIN B");
  mark(mainC_, "MAIN C");
  mark(mainD_, "MAIN D");
  mark(endingA_, "ENDING A");
  mark(endingB_, "ENDING B");

  // Estado dos canais: verde = usado pelo estilo, vermelho = mudo
  for (int i = 0; i < 16; ++i) {
    const bool used = player.activeChannels.count(i) > 0;
    const bool muted = player.mutedChannels.count(i) > 0;
    if (muted)
      channelBtns_[i].setColour(juce::TextButton::buttonColourId, red);
    else if (used)
      channelBtns_[i].setColour(juce::TextButton::buttonColourId, green);
    else
      channelBtns_[i].setColour(juce::TextButton::buttonColourId,
                                panel.brighter(0.05f));
  }

  startBtn_.setColour(juce::TextButton::buttonColourId,
                      player.isPlaying() ? green.darker(0.3f) : green);
}

void YamahaArrangerEditor::paint(juce::Graphics& g) {
  g.fillAll(bg);
  g.setColour(panel);
  g.fillRoundedRectangle(8.f, 8.f, static_cast<float>(getWidth() - 16),
                         static_cast<float>(getHeight() - 16), 8.f);
}

void YamahaArrangerEditor::resized() {
  auto area = getLocalBounds().reduced(16);
  const int gap = 6;

  auto top = area.removeFromTop(64);
  lcd_.setBounds(top.removeFromTop(34));
  lcdSmall_.setBounds(top.removeFromTop(22));
  area.removeFromTop(gap);

  status_.setBounds(area.removeFromTop(16));
  area.removeFromTop(gap);

  auto main = area;
  auto right = main.removeFromRight(250);
  auto left = main;

  // ---- coluna esquerda: arquivos ----
  auto files = left.removeFromTop(34);
  styleBtn_.setBounds(files.removeFromLeft(120));
  files.removeFromLeft(gap);
  sf2Btn_.setBounds(files.removeFromLeft(120));
  left.removeFromTop(gap);

  // ---- coluna esquerda: grade de seções ----
  auto grid = left.removeFromTop(2 * 34 + gap);
  const int btnW = (grid.getWidth() - gap * 4) / 5;
  juce::TextButton* row1[] = {&introA_, &introB_, &fillA_, &fillB_, &mainA_};
  juce::TextButton* row2[] = {&mainB_, &mainC_, &mainD_, &endingA_, &endingB_};
  auto r1 = grid.removeFromTop(34);
  for (int i = 0; i < 5; ++i) {
    auto b = r1;
    b.removeFromLeft(i * (btnW + gap));
    row1[i]->setBounds(b.removeFromLeft(btnW));
  }
  grid.removeFromTop(gap);
  auto r2 = grid.removeFromTop(34);
  for (int i = 0; i < 5; ++i) {
    auto b = r2;
    b.removeFromLeft(i * (btnW + gap));
    row2[i]->setBounds(b.removeFromLeft(btnW));
  }
  left.removeFromTop(gap);

  // ---- coluna esquerda: canais ----
  auto chans = left.removeFromTop(2 * 26 + gap);
  const int chW = (chans.getWidth() - gap * 7) / 8;
  for (int row = 0; row < 2; ++row) {
    auto r = chans.removeFromTop(26);
    for (int i = 0; i < 8; ++i) {
      auto b = r;
      b.removeFromLeft(i * (chW + gap));
      channelBtns_[row * 8 + i].setBounds(b.removeFromLeft(chW));
    }
    chans.removeFromTop(gap);
  }

  // ---- coluna direita: transporte ----
  auto tr = right.removeFromTop(2 * 34 + gap);
  const int trW = (tr.getWidth() - gap * 2) / 3;
  auto tr1 = tr.removeFromTop(34);
  startBtn_.setBounds(tr1.removeFromLeft(trW));
  tr1.removeFromLeft(gap);
  stopBtn_.setBounds(tr1.removeFromLeft(trW));
  tr1.removeFromLeft(gap);
  syncBtn_.setBounds(tr1.removeFromLeft(trW));
  tr.removeFromTop(gap);
  auto tr2 = tr.removeFromTop(34);
  panicBtn_.setBounds(tr2.removeFromLeft(trW));
  tr2.removeFromLeft(gap);
  resetBtn_.setBounds(tr2.removeFromLeft(trW));
  right.removeFromTop(gap);

  // ---- coluna direita: knobs ----
  auto knobs = right.removeFromTop(2 * 90 + gap);
  const int knW = (knobs.getWidth() - gap) / 2;
  auto k1 = knobs.removeFromTop(90);
  volumeKnob_.setBounds(k1.removeFromLeft(knW).reduced(12, 4));
  k1.removeFromLeft(gap);
  tempoKnob_.setBounds(k1.removeFromLeft(knW).reduced(12, 4));
  volumeLbl_.setBounds(volumeKnob_.getBounds().removeFromBottom(14));
  tempoLbl_.setBounds(tempoKnob_.getBounds().removeFromBottom(14));
  knobs.removeFromTop(gap);
  auto k2 = knobs.removeFromTop(90);
  reverbKnob_.setBounds(k2.removeFromLeft(knW).reduced(12, 4));
  k2.removeFromLeft(gap);
  chorusKnob_.setBounds(k2.removeFromLeft(knW).reduced(12, 4));
  reverbLbl_.setBounds(reverbKnob_.getBounds().removeFromBottom(14));
  chorusLbl_.setBounds(chorusKnob_.getBounds().removeFromBottom(14));
}

void YamahaArrangerEditor::timerCallback() { updateDisplay(); }

} // namespace yamaha
