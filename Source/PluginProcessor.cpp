#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
SamplerAudioProcessor::SamplerAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    formatManager.registerBasicFormats();
    for (int i = 0; i < kNumVoices; ++i)
        synth.addVoice (new SamplerVoice (adsrParams));
}

juce::AudioProcessorValueTreeState::ParameterLayout SamplerAudioProcessor::createLayout()
{
    using P = juce::AudioParameterFloat;
    using R = juce::NormalisableRange<float>;
    using ID = juce::ParameterID;

    juce::AudioProcessorValueTreeState::ParameterLayout l;
    l.add (std::make_unique<P> (ID { "attack",  1 }, "Attack",  R (0.001f, 5.0f,  0.001f, 0.4f), 0.005f));
    l.add (std::make_unique<P> (ID { "decay",   1 }, "Decay",   R (0.001f, 5.0f,  0.001f, 0.4f), 0.1f));
    l.add (std::make_unique<P> (ID { "sustain", 1 }, "Sustain", R (0.0f,   1.0f,  0.001f),       1.0f));
    l.add (std::make_unique<P> (ID { "release", 1 }, "Release", R (0.001f, 10.0f, 0.001f, 0.4f), 0.2f));
    l.add (std::make_unique<P> (ID { "gain",    1 }, "Gain",    R (-48.0f, 6.0f,  0.1f),         0.0f));
    return l;
}

//==============================================================================
void SamplerAudioProcessor::prepareToPlay (double sampleRate, int)
{
    synth.setCurrentPlaybackSampleRate (sampleRate);
}

void SamplerAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    adsrParams.attack  = apvts.getRawParameterValue ("attack")->load();
    adsrParams.decay   = apvts.getRawParameterValue ("decay")->load();
    adsrParams.sustain = apvts.getRawParameterValue ("sustain")->load();
    adsrParams.release = apvts.getRawParameterValue ("release")->load();

    synth.renderNextBlock (buffer, midi, 0, buffer.getNumSamples());

    buffer.applyGain (juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("gain")->load()));
}

juce::AudioProcessorEditor* SamplerAudioProcessor::createEditor()
{
    return new SamplerEditor (*this);
}

//==============================================================================
SamplerAudioProcessor::Audio SamplerAudioProcessor::loadAudio (const juce::File& f)
{
    const auto key = f.getFullPathName();
    if (auto it = cache.find (key); it != cache.end())
        return it->second;

    Audio a;
    if (std::unique_ptr<juce::AudioFormatReader> reader { formatManager.createReaderFor (f) })
    {
        const int len = (int) reader->lengthInSamples;
        if (len > 1)
        {
            a.buffer = std::make_shared<juce::AudioBuffer<float>> (juce::jmin (2, (int) reader->numChannels), len);
            reader->read (a.buffer.get(), 0, len, 0, true, true);
            a.rate = reader->sampleRate;
        }
    }
    cache[key] = a;
    return a;
}

static int parseKey (juce::String s)
{
    s = s.trim().toLowerCase();
    if (s.isEmpty())
        return -1;
    if (s.containsOnly ("0123456789-"))
        return s.getIntValue();

    static const int base[] = { 9, 11, 0, 2, 4, 5, 7 };   // a b c d e f g
    const int c = (int) s[0] - 'a';
    if (c < 0 || c > 6)
        return -1;

    int semi = base[c], i = 1;
    if (s[i] == '#')      { ++semi; ++i; }
    else if (s[i] == 'b') { --semi; ++i; }

    return (s.substring (i).getIntValue() + 1) * 12 + semi;   // c4 = 60
}

int SamplerAudioProcessor::addRegion (std::map<juce::String, juce::String>& m,
                                      const juce::File& dir, const juce::String& defaultPath)
{
    auto get = [&] (const char* k, const juce::String& d = {})
    {
        auto it = m.find (k);
        return it == m.end() ? d : it->second;
    };

    auto samplePath = get ("sample");
    if (samplePath.isEmpty())
        return 0;

    samplePath = (defaultPath + samplePath).replace ("\\", "/");
    auto audio = loadAudio (dir.getChildFile (samplePath));
    if (audio.buffer == nullptr)
        return 0;

    auto* r = new RegionSound();
    r->data = audio.buffer;
    r->sourceRate = audio.rate;

    int lo   = parseKey (get ("lokey", "0"));
    int hi   = parseKey (get ("hikey", "127"));
    int root = parseKey (get ("pitch_keycenter", "60"));
    const int key = parseKey (get ("key"));
    if (key >= 0)
    {
        lo = hi = key;
        if (m.find ("pitch_keycenter") == m.end())
            root = key;
    }

    r->loKey   = juce::jlimit (0, 127, lo < 0 ? 0 : lo);
    r->hiKey   = juce::jlimit (0, 127, hi < 0 ? 127 : hi);
    r->rootKey = juce::jlimit (0, 127, root < 0 ? 60 : root);
    r->loVel   = juce::jlimit (0, 127, get ("lovel", "0").getIntValue());
    r->hiVel   = juce::jlimit (0, 127, get ("hivel", "127").getIntValue());

    const auto mode = get ("loop_mode").toLowerCase();
    r->loop = (mode == "loop_continuous" || mode == "loop_sustain");

    const int len = audio.buffer->getNumSamples();
    r->loopStart = juce::jlimit (0, len - 2, get ("loop_start", "0").getIntValue());
    r->loopEnd   = juce::jlimit (1, len - 1, get ("loop_end", juce::String (len - 1)).getIntValue());
    r->gain      = juce::Decibels::decibelsToGain (get ("volume", "0").getFloatValue());

    synth.addSound (r);
    return 1;
}

int SamplerAudioProcessor::loadSfz (const juce::File& sfz)
{
    using Map = std::map<juce::String, juce::String>;

    // Tanggalin ang comments, gawing iisang linya
    juce::StringArray lines;
    lines.addLines (sfz.loadFileAsString());
    juce::String text;
    for (auto l : lines)
    {
        const int c = l.indexOf ("//");
        if (c >= 0) l = l.substring (0, c);
        text << l << " ";
    }

    Map control, global, group, region;
    Map* cur = nullptr;
    bool haveRegion = false;
    int count = 0;
    const auto dir = sfz.getParentDirectory();

    auto flush = [&]
    {
        if (! haveRegion) return;
        Map m = global;
        for (auto& kv : group)  m[kv.first] = kv.second;
        for (auto& kv : region) m[kv.first] = kv.second;
        count += addRegion (m, dir, control["default_path"]);
        haveRegion = false;
        region.clear();
    };

    const int n = text.length();
    auto isOpStart = [&] (int i)
    {
        int j = i;
        while (j < n && (juce::CharacterFunctions::isLetterOrDigit (text[j]) || text[j] == '_')) ++j;
        return j > i && j < n && text[j] == '=';
    };

    int i = 0;
    while (i < n)
    {
        const auto ch = text[i];
        if (juce::CharacterFunctions::isWhitespace (ch)) { ++i; continue; }

        if (ch == '<')
        {
            const int e = text.indexOfChar (i, '>');
            if (e < 0) break;
            const auto h = text.substring (i + 1, e).trim().toLowerCase();
            i = e + 1;

            flush();
            if (h == "region")       { region.clear(); haveRegion = true; cur = &region; }
            else if (h == "group")   { group.clear();  cur = &group; }
            else if (h == "global")  { global.clear(); cur = &global; }
            else if (h == "control") { cur = &control; }
            else                     { cur = nullptr; }
            continue;
        }

        const int eq = text.indexOfChar (i, '=');
        if (eq < 0) break;
        const auto name = text.substring (i, eq).trim().toLowerCase();

        const int v = eq + 1;
        int e = v;
        while (e < n)
        {
            if (text[e] == '<') break;
            if (juce::CharacterFunctions::isWhitespace (text[e]) && e + 1 < n && isOpStart (e + 1)) break;
            ++e;
        }

        if (cur != nullptr)
            (*cur)[name] = text.substring (v, e).trim();
        i = e;
    }
    flush();
    return count;
}

juce::String SamplerAudioProcessor::loadFiles (const juce::StringArray& paths)
{
    synth.allNotesOff (0, false);
    synth.clearSounds();
    cache.clear();

    int regions = 0;

    if (paths.size() == 1 && juce::File (paths[0]).hasFileExtension ("sfz"))
    {
        regions = loadSfz (juce::File (paths[0]));
    }
    else
    {
        int n = 0;
        for (auto& p : paths)
        {
            juce::File f (p);
            if (f.hasFileExtension ("sfz")) continue;

            auto a = loadAudio (f);
            if (a.buffer == nullptr) continue;

            auto* r = new RegionSound();
            r->data = a.buffer;
            r->sourceRate = a.rate;

            if (paths.size() == 1)               // isang WAV: buong keyboard, root = C4
            {
                r->loKey = 0; r->hiKey = 127; r->rootKey = 60;
            }
            else                                 // maraming WAV: bawat isa sa sariling key mula C2
            {
                const int k = 36 + n;
                if (k > 127) { delete r; break; }
                r->loKey = r->hiKey = r->rootKey = k;
            }
            ++n;
            synth.addSound (r);
            ++regions;
        }
    }

    loadedPaths = paths;
    statusText = regions > 0 ? juce::String (regions) + " region(s) na-load"
                             : "Walang na-load. Tingnan ang mga path ng samples.";
    return statusText;
}

//==============================================================================
void SamplerAudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("sources", loadedPaths.joinIntoString ("\n"), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, dest);
}

void SamplerAudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
    {
        auto state = juce::ValueTree::fromXml (*xml);
        if (state.hasType (apvts.state.getType()))
        {
            apvts.replaceState (state);
            const auto src = state.getProperty ("sources").toString();
            if (src.isNotEmpty())
                loadFiles (juce::StringArray::fromLines (src));
        }
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SamplerAudioProcessor();
}
