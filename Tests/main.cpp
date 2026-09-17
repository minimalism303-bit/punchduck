#include <juce_core/juce_core.h>
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <iostream>

// Minimal headless DSP smoke test. This doesn't instantiate the full plugin
// (that needs a host), but it exercises the same JUCE DSP building blocks
// the plugin's filters are built from, and confirms they don't produce
// NaN/Inf output on a simple sine burst - a quick sanity check that runs in
// CI without needing an audio device or a DAW.
int main()
{
constexpr double sampleRate = 44100.0;

auto hpCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass (sampleRate, 60.0, 0.707f);
juce::dsp::IIR::Filter<float> highPass;
*highPass.coefficients = *hpCoeffs;

auto bpCoeffs = juce::dsp::IIR::Coefficients<float>::makeBandPass (sampleRate, 60.0, 2.0f);
juce::dsp::IIR::Filter<float> bandPass;
*bandPass.coefficients = *bpCoeffs;

bool ok = true;

for (int i = 0; i < (int) sampleRate; ++i)
{
const float t = (float) i / (float) sampleRate;
const float sample = std::sin (2.0f * juce::MathConstants<float>::pi * 55.0f * t);

const float hpOut = highPass.processSample (sample);
const float bpOut = bandPass.processSample (sample);

if (! std::isfinite (hpOut) || ! std::isfinite (bpOut))
{
ok = false;
break;
}
}

if (! ok)
{
std::cerr << "PunchDuck DSP smoke test FAILED: non-finite filter output" << std::endl;
return 1;
}

std::cout << "PunchDuck DSP smoke test passed" << std::endl;
return 0;
}
