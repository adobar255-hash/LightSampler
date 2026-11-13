#pragma once
#include <JuceHeader.h>
#include <map>
#include "SamplerEngine.h"

class SamplerAudioProcessor : public juce::AudioProcessor
{
public:
    SamplerAudioProcessor();
    ~SamplerAudioProcessor() override = default;

    // Pag-load ng .sfz (isa) o isa/maraming audio files
    juce::String loadFiles (const juce::StringArray& paths);

    const juce::String getName() const override              { return "LightSampler"; }
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& l) const override
    {
        return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
    }
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                          { return true; }
    bool acceptsMidi() const override                        { return true; }
    bool producesMidi() const override                       { return false; }
    double getTailLengthSeconds() const override             { return 0.0; }
    int getNumPrograms() override                            { return 1; }
    int getCurrentProgram() override                         { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override         { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& dest) override;
    void setStateInformation (const void* data, int size) override;

    juce::AudioProcessorValueTreeState apvts;
    juce::String statusText { "Walang naka-load. I-drag ang .sfz o .wav dito." };

private:
    struct Audio
    {
        std::shared_ptr<juce::AudioBuffer<float>> buffer;
        double rate = 44100.0;
    };

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    Audio loadAudio (const juce::File& f);
    int loadSfz (const juce::File& sfz);
    int addRegion (std::map<juce::String, juce::String>& m,
                   const juce::File& dir, const juce::String& defaultPath);

    static constexpr int kNumVoices = 32;   // babaan para mas gumaan ang CPU

    juce::AudioFormatManager formatManager;
    juce::ADSR::Parameters adsrParams;
    SamplerSynth synth;
    std::map<juce::String, Audio> cache;
    juce::StringArray loadedPaths;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SamplerAudioProcessor)
};
