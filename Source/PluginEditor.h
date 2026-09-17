#pragma once

#include "PluginProcessor.h"

class PunchDuckAudioProcessorEditor : public juce::AudioProcessorEditor,
private juce::ComboBox::Listener
{
public:
explicit PunchDuckAudioProcessorEditor (PunchDuckAudioProcessor&);
~PunchDuckAudioProcessorEditor() override;

void paint (juce::Graphics&) override;
void resized() override;

private:
void comboBoxChanged (juce::ComboBox*) override;
void updateVisibility();

struct KnobWithLabel
{
juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
juce::Label label;
std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

void setupKnob (KnobWithLabel& knob, const juce::String& paramId, const juce::String& labelText);

PunchDuckAudioProcessor& processor;

juce::Label titleLabel;
juce::Label modeLabel;
juce::ComboBox modeBox;
std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAttachment;

KnobWithLabel kickPunch, kickTighten, kickControl, kickMix;
KnobWithLabel bassDuck, bassRelease, bassCarve, bassOutput;

JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PunchDuckAudioProcessorEditor)
};
