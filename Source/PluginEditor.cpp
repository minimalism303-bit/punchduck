#include "PluginProcessor.h"
#include "PluginEditor.h"

PunchDuckAudioProcessorEditor::PunchDuckAudioProcessorEditor (PunchDuckAudioProcessor& p)
: AudioProcessorEditor (&p), processor (p)
{
titleLabel.setText ("PunchDuck", juce::dontSendNotification);
titleLabel.setJustificationType (juce::Justification::centred);
titleLabel.setFont (juce::Font (22.0f, juce::Font::bold));
addAndMakeVisible (titleLabel);

modeLabel.setText ("Mode", juce::dontSendNotification);
modeLabel.setJustificationType (juce::Justification::centredRight);
addAndMakeVisible (modeLabel);

modeBox.addItem ("Kick", 1);
modeBox.addItem ("Bass", 2);
modeBox.addListener (this);
addAndMakeVisible (modeBox);

modeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
  processor.apvts, "mode", modeBox);

setupKnob (kickPunch,   "kickPunch",   "Punch");
setupKnob (kickTighten, "kickTighten", "Tighten");
setupKnob (kickControl, "kickControl", "Control");
setupKnob (kickMix,     "kickMix",     "Mix");

setupKnob (bassDuck,    "bassDuckAmount",  "Duck Amount");
setupKnob (bassRelease, "bassDuckRelease", "Duck Release");
setupKnob (bassCarve,   "bassCarve",       "Carve");
setupKnob (bassOutput,  "bassOutputGain",  "Output");

updateVisibility();

setSize (480, 300);
}

PunchDuckAudioProcessorEditor::~PunchDuckAudioProcessorEditor()
{
modeBox.removeListener (this);
}

void PunchDuckAudioProcessorEditor::setupKnob (KnobWithLabel& knob, const juce::String& paramId, const juce::String& labelText)
{
  knob.slider.setRange (0.0, 1.0); // overridden by the attachment's parameter range
addAndMakeVisible (knob.slider);

knob.label.setText (labelText, juce::dontSendNotification);
knob.label.setJustificationType (juce::Justification::centred);
addAndMakeVisible (knob.label);

knob.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
  processor.apvts, paramId, knob.slider);
}

void PunchDuckAudioProcessorEditor::comboBoxChanged (juce::ComboBox*)
{
  updateVisibility();
}

void PunchDuckAudioProcessorEditor::updateVisibility()
{
  const bool kickMode = modeBox.getSelectedItemIndex() == 0;

for (auto* k : { &kickPunch, &kickTighten, &kickControl, &kickMix })
{
k->slider.setVisible (kickMode);
k->label.setVisible (kickMode);
}

for (auto* k : { &bassDuck, &bassRelease, &bassCarve, &bassOutput })
{
k->slider.setVisible (! kickMode);
k->label.setVisible (! kickMode);
}
}

void PunchDuckAudioProcessorEditor::paint (juce::Graphics& g)
{
  g.fillAll (juce::Colour (0xff1c1e22));

g.setColour (juce::Colour (0xff2a2d33));
g.fillRoundedRectangle (getLocalBounds().reduced (8).toFloat(), 10.0f);
}

void PunchDuckAudioProcessorEditor::resized()
{
  auto area = getLocalBounds().reduced (20);

titleLabel.setBounds (area.removeFromTop (36));

auto modeRow = area.removeFromTop (32);
modeLabel.setBounds (modeRow.removeFromLeft (60));
modeBox.setBounds (modeRow.removeFromLeft (140));

area.removeFromTop (16);

auto knobArea = area.removeFromTop (200);
const int knobWidth = knobArea.getWidth() / 4;

KnobWithLabel* kickKnobs[4] = { &kickPunch, &kickTighten, &kickControl, &kickMix };
KnobWithLabel* bassKnobs[4] = { &bassDuck, &bassRelease, &bassCarve, &bassOutput };

for (int i = 0; i < 4; ++i)
{
auto col = knobArea.removeFromLeft (knobWidth);
auto labelBounds = col.removeFromBottom (20);

kickKnobs[i]->slider.setBounds (col);
kickKnobs[i]->label.setBounds (labelBounds);

bassKnobs[i]->slider.setBounds (col);
bassKnobs[i]->label.setBounds (labelBounds);
}
}
