#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

//==============================================================================
// EnvelopeFollower
//==============================================================================
void PunchDuckAudioProcessor::EnvelopeFollower::prepare (double sampleRate, float attackMs, float releaseMs)
{
  sr = sampleRate;
setTimes (attackMs, releaseMs);
level = 0.0f;
}

void PunchDuckAudioProcessor::EnvelopeFollower::setTimes (float attackMs, float releaseMs)
{
  attackCoeff  = std::exp (-1.0f / (float) (sr * ((double) attackMs  * 0.001)));
releaseCoeff = std::exp (-1.0f / (float) (sr * ((double) releaseMs * 0.001)));
}

float PunchDuckAudioProcessor::EnvelopeFollower::process (float rectifiedInput)
{
  const float coeff = rectifiedInput > level ? attackCoeff : releaseCoeff;
level = coeff * level + (1.0f - coeff) * rectifiedInput;
return level;
}

//==============================================================================
// FundamentalTracker
//==============================================================================
void PunchDuckAudioProcessor::FundamentalTracker::prepare (double sampleRate)
{
  sr = sampleRate;
ring.fill (0.0f);
hopAccumulator.fill (0.0f);
hopWritePos = 0;
smoothedFreqHz = 60.0f;
}

void PunchDuckAudioProcessor::FundamentalTracker::pushSample (float sample)
{
  hopAccumulator[(size_t) hopWritePos++] = sample;

if (hopWritePos >= hopSize)
{
hopWritePos = 0;

// Slide the analysis window left by one hop and append the new hop.
std::move (ring.begin() + hopSize, ring.end(), ring.begin());
std::copy (hopAccumulator.begin(), hopAccumulator.end(), ring.end() - hopSize);

runAutocorrelation();
}
}

void PunchDuckAudioProcessor::FundamentalTracker::runAutocorrelation()
{
  std::array<float, windowSize> windowed {};

float mean = 0.0f;
for (auto v : ring)
  mean += v;
mean /= (float) windowSize;

for (int i = 0; i < windowSize; ++i)
{
const float w = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) (windowSize - 1));
windowed[(size_t) i] = (ring[(size_t) i] - mean) * w;
}

const int minLag = juce::jmax (1, (int) (sr / 150.0));
const int maxLag = juce::jmin (windowSize - 1, (int) (sr / 25.0));

if (maxLag <= minLag)
  return;

// Coarse pass (decimated) to find the approximate best lag cheaply,
// then a fine pass around it - keeps this real-time safe even though
// it only runs once per analysis hop (~1024 samples).
constexpr int coarseStep = 4;
int bestLag = minLag;
float bestScore = -1.0e9f;

for (int lag = minLag; lag <= maxLag; lag += coarseStep)
{
float sum = 0.0f;
for (int i = 0; i < windowSize - lag; i += 2)
  sum += windowed[(size_t) i] * windowed[(size_t) (i + lag)];

if (sum > bestScore)
{
bestScore = sum;
bestLag = lag;
}
}

const int fineStart = juce::jmax (minLag, bestLag - coarseStep);
const int fineEnd   = juce::jmin (maxLag, bestLag + coarseStep);
bestScore = -1.0e9f;

for (int lag = fineStart; lag <= fineEnd; ++lag)
{
float sum = 0.0f;
for (int i = 0; i < windowSize - lag; ++i)
  sum += windowed[(size_t) i] * windowed[(size_t) (i + lag)];

if (sum > bestScore)
{
bestScore = sum;
bestLag = lag;
}
}

if (bestScore > 0.0f && bestLag > 0)
{
const float freq = (float) (sr / (double) bestLag);
constexpr float smoothingCoeff = 0.85f; // avoid the tracked filters jumping around
smoothedFreqHz = smoothingCoeff * smoothedFreqHz
  + (1.0f - smoothingCoeff) * juce::jlimit (25.0f, 150.0f, freq);
}
}

//==============================================================================
// TransientShaper
//==============================================================================
void PunchDuckAudioProcessor::TransientShaper::prepare (double sampleRate)
{
  fastEnv.prepare (sampleRate, 0.3f, 2.0f);
slowEnv.prepare (sampleRate, 15.0f, 120.0f);
}

float PunchDuckAudioProcessor::TransientShaper::process (float in, float punchAmount)
{
  const float rectified = std::abs (in);
const float fast = fastEnv.process (rectified);
const float slow = slowEnv.process (rectified);

const float transient = juce::jmax (0.0f, fast - slow);
const float normalizedTransient = juce::jlimit (0.0f, 3.0f, transient / (slow + 0.0001f));

const float gain = 1.0f + punchAmount * normalizedTransient * 2.5f;
return in * gain;
}

//==============================================================================
// AdaptiveCompressor
//==============================================================================
void PunchDuckAudioProcessor::AdaptiveCompressor::prepare (double sampleRate)
{
  rmsEnv.prepare (sampleRate, 3.0f, 15.0f);
averageLevelEnv.prepare (sampleRate, 300.0f, 300.0f);
grSmoother.prepare (sampleRate, 5.0f, 60.0f);
makeupDb = 0.0f;
}

float PunchDuckAudioProcessor::AdaptiveCompressor::process (float in, float amount)
{
  const float absIn = std::abs (in);

const float instantLevel = rmsEnv.process (absIn);
const float instantLevelDb = juce::Decibels::gainToDecibels (instantLevel, -100.0f);

const float avgLevel = averageLevelEnv.process (absIn);
const float avgLevelDb = juce::Decibels::gainToDecibels (avgLevel, -100.0f);

// Adaptive threshold: compress relative to the signal's own recent
// average loudness, so this behaves sensibly regardless of gain staging.
const float thresholdDb = avgLevelDb;
const float ratio = 1.0f + amount * 3.0f; // up to 4:1
const float kneeDb = 6.0f;

const float overDb = instantLevelDb - thresholdDb;
float grDb = 0.0f;

if (overDb > -kneeDb * 0.5f)
{
if (overDb <= kneeDb * 0.5f)
{
const float x = overDb + kneeDb * 0.5f;
grDb = (1.0f - 1.0f / ratio) * (x * x) / (2.0f * kneeDb);
}
else
{
grDb = (1.0f - 1.0f / ratio) * overDb;
}
}

grDb = grSmoother.process (juce::jmax (0.0f, grDb));
grDb *= amount;

const float makeupTarget = grDb * 0.7f; // partial auto makeup so turning the knob up isn't just quieter
makeupDb = 0.98f * makeupDb + 0.02f * makeupTarget;

const float gain = juce::Decibels::decibelsToGain (-grDb + makeupDb);
return in * gain;
}

//==============================================================================
// PunchDuckAudioProcessor
//==============================================================================
PunchDuckAudioProcessor::PunchDuckAudioProcessor()
: AudioProcessor (BusesProperties()
.withInput  ("Input",     juce::AudioChannelSet::stereo(), true)
.withInput  ("Sidechain", juce::AudioChannelSet::stereo(), false)
.withOutput ("Output",    juce::AudioChannelSet::stereo(), true)),
apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout PunchDuckAudioProcessor::createParameterLayout()
{
  std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

params.push_back (std::make_unique<juce::AudioParameterChoice> (
  juce::ParameterID { "mode", 1 }, "Mode", juce::StringArray { "Kick", "Bass" }, 0));

params.push_back (std::make_unique<juce::AudioParameterFloat> (
  juce::ParameterID { "kickPunch", 1 }, "Punch",
  juce::NormalisableRange<float> (0.0f, 1.0f), 0.5f));
params.push_back (std::make_unique<juce::AudioParameterFloat> (
  juce::ParameterID { "kickTighten", 1 }, "Tighten",
  juce::NormalisableRange<float> (0.0f, 1.0f), 0.5f));
params.push_back (std::make_unique<juce::AudioParameterFloat> (
  juce::ParameterID { "kickControl", 1 }, "Control",
  juce::NormalisableRange<float> (0.0f, 1.0f), 0.4f));
params.push_back (std::make_unique<juce::AudioParameterFloat> (
  juce::ParameterID { "kickMix", 1 }, "Mix",
  juce::NormalisableRange<float> (0.0f, 1.0f), 1.0f));

params.push_back (std::make_unique<juce::AudioParameterFloat> (
  juce::ParameterID { "bassDuckAmount", 1 }, "Duck Amount",
  juce::NormalisableRange<float> (0.0f, 1.0f), 0.6f));
params.push_back (std::make_unique<juce::AudioParameterFloat> (
  juce::ParameterID { "bassDuckRelease", 1 }, "Duck Release",
  juce::NormalisableRange<float> (20.0f, 400.0f, 0.0f, 0.5f), 120.0f));
params.push_back (std::make_unique<juce::AudioParameterFloat> (
  juce::ParameterID { "bassCarve", 1 }, "Carve",
  juce::NormalisableRange<float> (0.0f, 1.0f), 0.5f));
params.push_back (std::make_unique<juce::AudioParameterFloat> (
  juce::ParameterID { "bassOutputGain", 1 }, "Output",
  juce::NormalisableRange<float> (-12.0f, 12.0f), 0.0f));

return { params.begin(), params.end() };
}

bool PunchDuckAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  const auto mainIn  = layouts.getMainInputChannelSet();
const auto mainOut = layouts.getMainOutputChannelSet();

if (mainIn != mainOut)
  return false;

if (mainIn != juce::AudioChannelSet::mono() && mainIn != juce::AudioChannelSet::stereo())
  return false;

if (layouts.inputBuses.size() > 1)
{
const auto sc = layouts.getChannelSet (true, 1);
if (! sc.isDisabled() && sc != mainIn)
  return false;
}

return true;
}

void PunchDuckAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
  currentSampleRate = sampleRate;

mainTracker.prepare (sampleRate);
sidechainTracker.prepare (sampleRate);
sidechainEnvelope.prepare (sampleRate, 2.0f, 150.0f);

for (auto& c : channels)
{
c.tighteningFilter.reset();
c.carveBandpass.reset();
c.transientShaper.prepare (sampleRate);
c.compressor.prepare (sampleRate);

auto hp = juce::dsp::IIR::Coefficients<float>::makeHighPass (sampleRate, 30.0);
*c.tighteningFilter.coefficients = *hp;

auto bp = juce::dsp::IIR::Coefficients<float>::makeBandPass (sampleRate, 60.0, 2.0f);
*c.carveBandpass.coefficients = *bp;
}

lastTighteningFreq = 30.0f;
lastCarveFreq = 60.0f;

if ((int) duckEnvScratch.size() < samplesPerBlock)
  duckEnvScratch.resize ((size_t) juce::jmax (samplesPerBlock, 512));
}

void PunchDuckAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
  juce::ScopedNoDenormals noDenormals;

const auto totalNumInputChannels  = getTotalNumInputChannels();
const auto totalNumOutputChannels = getTotalNumOutputChannels();
for (auto ch = totalNumInputChannels; ch < totalNumOutputChannels; ++ch)
  buffer.clear (ch, 0, buffer.getNumSamples());

auto mainBuffer = getBusBuffer (buffer, true, 0);
const int numSamples  = mainBuffer.getNumSamples();
const int numChannels = juce::jmin (mainBuffer.getNumChannels(), (int) channels.size());

const auto mode = (Mode) (int) apvts.getRawParameterValue ("mode")->load();

bool sidechainConnected = false;
juce::AudioBuffer<float> sidechainBuffer;
if (getBusCount (true) > 1)
{
auto* scBus = getBus (true, 1);
if (scBus != nullptr && scBus->isEnabled())
{
sidechainBuffer = getBusBuffer (buffer, true, 1);
sidechainConnected = sidechainBuffer.getNumChannels() > 0;
}
}

if (mode == Mode::kick)
{
const float punch   = apvts.getRawParameterValue ("kickPunch")->load();
const float tighten = apvts.getRawParameterValue ("kickTighten")->load();
const float control = apvts.getRawParameterValue ("kickControl")->load();
const float mix      = apvts.getRawParameterValue ("kickMix")->load();

for (int i = 0; i < numSamples; ++i)
{
float monoSum = 0.0f;
for (int ch = 0; ch < numChannels; ++ch)
  monoSum += mainBuffer.getReadPointer (ch)[i];
monoSum /= (float) juce::jmax (1, numChannels);
mainTracker.pushSample (monoSum);
}

const float trackedFreq = mainTracker.getFrequencyHz();
const float tightenFreq = juce::jlimit (20.0f, 140.0f, 20.0f + tighten * (0.85f * trackedFreq - 20.0f));

if (std::abs (tightenFreq - lastTighteningFreq) > 0.5f)
{
auto coeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass (currentSampleRate, tightenFreq, 0.707f);
for (int ch = 0; ch < numChannels; ++ch)
  *channels[(size_t) ch].tighteningFilter.coefficients = *coeffs;
lastTighteningFreq = tightenFreq;
}

for (int ch = 0; ch < numChannels; ++ch)
{
auto* data = mainBuffer.getWritePointer (ch);
auto& c = channels[(size_t) ch];

for (int i = 0; i < numSamples; ++i)
{
const float dry = data[i];

float x = c.tighteningFilter.processSample (dry);
x = c.transientShaper.process (x, punch);
x = c.compressor.process (x, control);

data[i] = dry * (1.0f - mix) + x * mix;
}
}
}
else // Mode::bass
{
const float duckAmount   = apvts.getRawParameterValue ("bassDuckAmount")->load();
const float duckRelease  = apvts.getRawParameterValue ("bassDuckRelease")->load();
const float carve        = apvts.getRawParameterValue ("bassCarve")->load();
const float outputGainDb = apvts.getRawParameterValue ("bassOutputGain")->load();
const float outputGain   = juce::Decibels::decibelsToGain (outputGainDb);

sidechainEnvelope.setTimes (2.0f, duckRelease);

if ((int) duckEnvScratch.size() < numSamples)
  duckEnvScratch.resize ((size_t) numSamples);

for (int i = 0; i < numSamples; ++i)
{
float scSample = 0.0f;

if (sidechainConnected)
{
const int scChannels = sidechainBuffer.getNumChannels();
for (int ch = 0; ch < scChannels; ++ch)
  scSample += sidechainBuffer.getReadPointer (ch)[i];
scSample /= (float) juce::jmax (1, scChannels);
}
else
{
// No sidechain routed yet: fall back to the bass's own
// transients so the effect still does something sane.
for (int ch = 0; ch < numChannels; ++ch)
  scSample += mainBuffer.getReadPointer (ch)[i];
scSample /= (float) juce::jmax (1, numChannels);
}

sidechainTracker.pushSample (scSample);
duckEnvScratch[(size_t) i] = sidechainEnvelope.process (std::abs (scSample));
}

const float carveFreq = juce::jlimit (25.0f, 200.0f, sidechainTracker.getFrequencyHz());

if (std::abs (carveFreq - lastCarveFreq) > 0.5f)
{
auto coeffs = juce::dsp::IIR::Coefficients<float>::makeBandPass (currentSampleRate, carveFreq, 2.0f);
for (int ch = 0; ch < numChannels; ++ch)
  *channels[(size_t) ch].carveBandpass.coefficients = *coeffs;
lastCarveFreq = carveFreq;
}

for (int ch = 0; ch < numChannels; ++ch)
{
auto* data = mainBuffer.getWritePointer (ch);
auto& c = channels[(size_t) ch];

for (int i = 0; i < numSamples; ++i)
{
const float dry = data[i];
const float duckEnv = duckEnvScratch[(size_t) i];

const float bandEnergy = c.carveBandpass.processSample (dry);
const float carved = dry - carve * duckEnv * bandEnergy * 1.5f;

const float duckGain = juce::jmax (0.0f, 1.0f - duckAmount * duckEnv);
data[i] = carved * duckGain * outputGain;
}
}
}
}

juce::AudioProcessorEditor* PunchDuckAudioProcessor::createEditor()
{
  return new PunchDuckAudioProcessorEditor (*this);
}

void PunchDuckAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
  auto state = apvts.copyState();
std::unique_ptr<juce::XmlElement> xml (state.createXml());
copyXmlToBinary (*xml, destData);
}

void PunchDuckAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
  std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
if (xml != nullptr && xml->hasTagName (apvts.state.getType()))
  apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
  return new PunchDuckAudioProcessor();
}
