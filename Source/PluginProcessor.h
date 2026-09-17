#pragma once

#include <JuceHeader.h>
#include <array>
#include <vector>

// PunchDuck
// ---------
// A single FX plugin with two modes, selected by the "Mode" parameter:
//
//  Kick mode  - tracks the kick's fundamental frequency (via a lightweight
//               autocorrelation pitch tracker), then applies an adaptive
//               high-pass ("Tighten"), a fast/slow-envelope transient
//               shaper ("Punch"), and a self-referencing compressor
//               ("Control") whose threshold tracks the kick's own recent
//               loudness. "Mix" blends dry/processed in parallel.
//
//  Bass mode  - the same adaptive engine, plus a genuine sidechain input
//               bus. Route the kick into the sidechain input and this mode
//               (a) ducks the bass broadband, envelope-follower style
//               ("Duck Amount" / "Duck Release"), and (b) dynamically
//               carves a narrow notch at the kick's tracked fundamental out
//               of the bass, but only while the kick is actually sounding
//               ("Carve") - this is what clears the low-end collision that
//               plain sidechain ducking alone doesn't fully solve.
//
// None of this is "AI" in the machine-learning sense - it's real-time
// adaptive DSP (autocorrelation pitch tracking + envelope followers) that
// retunes itself to whatever kick/bass you feed it.

class PunchDuckAudioProcessor : public juce::AudioProcessor
{
public:
PunchDuckAudioProcessor();
~PunchDuckAudioProcessor() override = default;

void prepareToPlay (double sampleRate, int samplesPerBlock) override;
void releaseResources() override {}

bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

juce::AudioProcessorEditor* createEditor() override;
bool hasEditor() const override { return true; }

const juce::String getName() const override { return JucePlugin_Name; }
bool acceptsMidi() const override { return false; }
bool producesMidi() const override { return false; }
bool isMidiEffect() const override { return false; }
double getTailLengthSeconds() const override { return 0.0; }

int getNumPrograms() override { return 1; }
int getCurrentProgram() override { return 0; }
void setCurrentProgram (int) override {}
const juce::String getProgramName (int) override { return {}; }
void changeProgramName (int, const juce::String&) override {}

void getStateInformation (juce::MemoryBlock& destData) override;
void setStateInformation (const void* data, int sizeInBytes) override;

enum class Mode { kick = 0, bass = 1 };

juce::AudioProcessorValueTreeState apvts;

private:
static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

//==============================================================================
// Simple one-pole attack/release envelope follower, reused for RMS tracking,
// transient detection, gain-reduction smoothing and sidechain ducking.
class EnvelopeFollower
{
public:
void prepare (double sampleRate, float attackMs, float releaseMs);
void setTimes (float attackMs, float releaseMs);
float process (float rectifiedInput);
float getLevel() const { return level; }

private:
double sr = 44100.0;
float attackCoeff = 0.0f, releaseCoeff = 0.0f;
float level = 0.0f;
};

// Lightweight real-time fundamental-frequency tracker for kick/bass range
// (~25-150 Hz), using a coarse-to-fine autocorrelation search updated
// once per analysis hop so it stays cheap enough for the audio thread.
class FundamentalTracker
{
public:
void prepare (double sampleRate);
void pushSample (float sample);
float getFrequencyHz() const { return smoothedFreqHz; }

private:
void runAutocorrelation();

static constexpr int windowSize = 4096;
static constexpr int hopSize = 1024;

std::array<float, windowSize> ring {};
std::array<float, hopSize> hopAccumulator {};
int hopWritePos = 0;
double sr = 44100.0;
float smoothedFreqHz = 60.0f;
};

// Fast/slow dual-envelope transient shaper (classic "transient designer"
// technique): the gap between a fast and a slow envelope estimates the
// transient content, which is boosted according to the Punch amount.
class TransientShaper
{
public:
void prepare (double sampleRate);
float process (float in, float punchAmount);

private:
EnvelopeFollower fastEnv, slowEnv;
};

// Feedforward compressor whose threshold is set relative to the
// program's own recent average loudness, so it behaves sensibly
// regardless of input gain staging, plus partial auto makeup gain.
class AdaptiveCompressor
{
public:
void prepare (double sampleRate);
float process (float in, float amount);

private:
EnvelopeFollower rmsEnv;
EnvelopeFollower averageLevelEnv;
EnvelopeFollower grSmoother;
float makeupDb = 0.0f;
};

struct ChannelChain
{
juce::dsp::IIR::Filter<float> tighteningFilter;  // Kick mode adaptive high-pass
juce::dsp::IIR::Filter<float> carveBandpass;      // Bass mode dynamic carve band
TransientShaper transientShaper;
AdaptiveCompressor compressor;
};

FundamentalTracker mainTracker;       // Kick mode: tracks the input signal
FundamentalTracker sidechainTracker;  // Bass mode: tracks the sidechain (kick) signal
EnvelopeFollower sidechainEnvelope;   // Bass mode: drives the broadband duck

std::array<ChannelChain, 2> channels;
std::vector<float> duckEnvScratch;

double currentSampleRate = 44100.0;
float lastTighteningFreq = -1.0f;
float lastCarveFreq = -1.0f;

JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PunchDuckAudioProcessor)
};
