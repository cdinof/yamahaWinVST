// Interface do plugin: painel estilo arranjador Yamaha.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "PluginProcessor.h"

namespace yamaha {

class YamahaArrangerEditor : public juce::AudioProcessorEditor,
                             public juce::Timer {
 public:
  explicit YamahaArrangerEditor(YamahaArrangerProcessor&);
  ~YamahaArrangerEditor() override;

  void paint(juce::Graphics&) override;
  void resized() override;
  void timerCallback() override;

 private:
  void buildUi();
  void loadStyleClicked();
  void loadSoundFontClicked();
  void sectionClicked(const juce::String& name);
  void updateDisplay();

  YamahaArrangerProcessor& processor_;

  // Display
  juce::Label lcd_;
  juce::Label lcdSmall_;
  juce::Label status_;

  // Transporte / arquivos
  juce::TextButton startBtn_{"START"}, stopBtn_{"STOP"}, syncBtn_{"SYNC"},
      panicBtn_{"PANIC"}, resetBtn_{"RESET"};
  juce::TextButton styleBtn_{"STYLE"}, sf2Btn_{"SOUNDFONT"};

  // Seções
  juce::TextButton introA_{"INTRO A"}, introB_{"INTRO B"};
  juce::TextButton fillA_{"FILL A"}, fillB_{"FILL B"};
  juce::TextButton mainA_{"MAIN A"}, mainB_{"MAIN B"}, mainC_{"MAIN C"},
      mainD_{"MAIN D"};
  juce::TextButton endingA_{"ENDING A"}, endingB_{"ENDING B"};

  // Knobs
  juce::Slider volumeKnob_, tempoKnob_, reverbKnob_, chorusKnob_;
  juce::Label volumeLbl_{"", "VOL"}, tempoLbl_{"", "TEMPO"},
      reverbLbl_{"", "REVERB"}, chorusLbl_{"", "CHORUS"};

  // Canais
  juce::TextButton channelBtns_[16];

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(YamahaArrangerEditor)
};

} // namespace yamaha
