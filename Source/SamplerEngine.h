#pragma once
#include <JuceHeader.h>

//==============================================================================
// Isang region = isang sample na naka-map sa range ng keys at velocity
struct RegionSound : public juce::SynthesiserSound
{
    std::shared_ptr<juce::AudioBuffer<float>> data;
    double sourceRate = 44100.0;
    int loKey = 0, hiKey = 127, rootKey = 60;
    int loVel = 0, hiVel = 127;
    bool loop = false;
    int loopStart = 0, loopEnd = 0;
    float gain = 1.0f;

    bool appliesToNote (int n) override       { return n >= loKey && n <= hiKey; }
    bool appliesToChannel (int) override      { return true; }
    bool matchesVelocity (float v) const
    {
        const int iv = juce::roundToInt (v * 127.0f);
        return iv >= loVel && iv <= hiVel;
    }
};

//==============================================================================
class LsVoice : public juce::SynthesiserVoice
{
public:
    explicit LsVoice (const juce::ADSR::Parameters& p) : params (p) {}

    bool canPlaySound (juce::SynthesiserSound* s) override
    {
        return dynamic_cast<RegionSound*> (s) != nullptr;
    }

    void setCurrentPlaybackSampleRate (double r) override
    {
        juce::SynthesiserVoice::setCurrentPlaybackSampleRate (r);
        adsr.setSampleRate (r);
    }

    void startNote (int note, float velocity, juce::SynthesiserSound* s, int) override
    {
        auto* sound = dynamic_cast<RegionSound*> (s);
        if (sound == nullptr || sound->data == nullptr)
        {
            clearCurrentNote();
            return;
        }

        ratio = std::pow (2.0, (note - sound->rootKey) / 12.0)
                * sound->sourceRate / getSampleRate();
        pos   = 0.0;
        level = velocity * sound->gain;

        adsr.setParameters (params);
        adsr.noteOn();
    }

    void stopNote (float, bool allowTailOff) override
    {
        if (allowTailOff)
        {
            adsr.setParameters (params);
            adsr.noteOff();
        }
        else
        {
            adsr.reset();
            clearCurrentNote();
        }
    }

    void pitchWheelMoved (int) override {}
    void controllerMoved (int, int) override {}

    void renderNextBlock (juce::AudioBuffer<float>& out, int start, int num) override
    {
        auto* s = dynamic_cast<RegionSound*> (getCurrentlyPlayingSound().get());
        if (s == nullptr || s->data == nullptr)
            return;

        const auto& d   = *s->data;
        const int len   = d.getNumSamples();
        const float* L  = d.getReadPointer (0);
        const float* R  = d.getNumChannels() > 1 ? d.getReadPointer (1) : L;
        const bool loop = s->loop && s->loopEnd > s->loopStart + 1;

        float* outL = out.getWritePointer (0, start);
        float* outR = out.getNumChannels() > 1 ? out.getWritePointer (1, start) : nullptr;

        for (int i = 0; i < num; ++i)
        {
            const int idx = (int) pos;
            if (idx >= len - 1 && ! loop)
            {
                adsr.reset();
                clearCurrentNote();
                break;
            }

            int idx2 = idx + 1;
            if (loop && idx2 >= s->loopEnd)
                idx2 = s->loopStart;

            const float frac = (float) (pos - idx);
            const float env  = adsr.getNextSample() * level;

            outL[i] += (L[idx] + (L[idx2] - L[idx]) * frac) * env;
            if (outR != nullptr)
                outR[i] += (R[idx] + (R[idx2] - R[idx]) * frac) * env;

            pos += ratio;
            if (loop && pos >= s->loopEnd)
                pos -= (s->loopEnd - s->loopStart);

            if (! adsr.isActive())
            {
                clearCurrentNote();
                break;
            }
        }
    }

private:
    const juce::ADSR::Parameters& params;
    juce::ADSR adsr;
    double pos = 0.0, ratio = 1.0;
    float level = 1.0f;
};

//==============================================================================
// Synth na gumagalang sa velocity ranges (para sa velocity layers)
class LsSynth : public juce::Synthesiser
{
public:
    void noteOn (int channel, int note, float velocity) override
    {
        const juce::ScopedLock sl (lock);

        for (auto* snd : sounds)
            if (auto* r = dynamic_cast<RegionSound*> (snd))
                if (r->appliesToNote (note) && r->matchesVelocity (velocity))
                    startVoice (findFreeVoice (r, channel, note, isNoteStealingEnabled()),
                                r, channel, note, velocity);
    }
};
