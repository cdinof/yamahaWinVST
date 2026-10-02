// Ponto de entrada do plugin (JUCE chama createPluginFilter).
#include <JuceHeader.h>

#include "PluginProcessor.h"

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
  return new yamaha::YamahaArrangerProcessor();
}
