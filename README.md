# PunchDuck

A single VST3 FX plugin with a Kick / Bass mode switch, aimed at the "kick
and bass are fighting for the same space" problem in kick-driven electronic
genres (progressive/uplifting trance, melodic techno, etc.).

Neither mode is machine learning / "AI" in the trendy sense; both use
real-time adaptive DSP: a lightweight autocorrelation-based pitch tracker
continuously estimates the kick's (or bass's) fundamental frequency, and the
filters/compressor retune themselves to that estimate as you play.

## Kick mode

Tracks the kick's fundamental, then applies:

Tighten: an adaptive high-pass filter that sits just below the tracked
fundamental, removing sub-rumble without thinning out the thump.

Punch: a fast/slow dual-envelope transient shaper (the classic "transient
designer" technique) that boosts the kick's attack.

Control: a compressor whose threshold is set relative to the kick's own
recent average loudness (not a fixed dB value), so it behaves sensibly no
matter how the track is gain-staged. Includes partial auto makeup gain.

Mix: parallel dry/wet blend, so pushing the other three knobs hard doesn't
have to destroy the original sound.

## Bass mode

Same adaptive engine, plus a genuine sidechain input bus. Route your kick
track's output into PunchDuck's sidechain input on the bass channel (in FL
Studio: right-click the plugin, Sidechain to this track, or use the mixer's
sidechain routing and select this PunchDuck instance as the input).

PunchDuck then tracks the kick's fundamental from the sidechain signal and:

Duck Amount / Duck Release: a broadband ducking compressor driven by an
envelope follower on the sidechain (the classic "pumping" sidechain effect,
reacting to the kick's actual envelope rather than a fixed LFO shape).

Carve: a dynamic notch at the kick's tracked fundamental, applied to the
bass only while the kick is actually sounding. This is what clears the
low-end collision that plain ducking alone doesn't fully solve; ducking
lowers the whole bass, carving specifically removes the exact frequency the
kick and bass are fighting over.

Output: makeup gain (+/-12 dB).

If no sidechain is connected, Bass mode still runs using the bass's own
transients as a fallback, so it isn't silent/broken if you forget to route
the kick, though it's designed to be used with the sidechain connected.

## Building

Clone JUCE (matching the version pinned in .github/workflows/build.yml)
next to or inside this repository:

git clone --depth 1 --branch 8.0.6 https://github.com/juce-framework/JUCE.git

Then configure and build with CMake:

cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target PunchDuck_VST3 --config Release

The built plugin bundle appears under
build/PunchDuck_artefacts/Release/VST3/PunchDuck.vst3

GitHub Actions (.github/workflows/build.yml) builds Windows, macOS (VST3 +
AU) and Linux automatically on every push, and uploads each as a
downloadable workflow artifact.

### Installing on Windows

Extract the downloaded artifact zip and move the PunchDuck.vst3 folder
itself (it must end in .vst3; GitHub's artifact zip nests it inside a
folder named after the artifact, so rename/move just the inner bundle, not
that outer folder) into C:\Program Files\Common Files\VST3\

Make sure that path is registered as a VST3 search path in your DAW (not a
legacy VST2 path) and rescan.
