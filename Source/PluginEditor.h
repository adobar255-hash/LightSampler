#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class SamplerEditor : public juce::AudioProcessorEditor,
                      public juce::FileDragAndDropTarget
{
public:
    explicit SamplerEditor (SamplerAudioProcessor& p)
        : AudioProcessorEditor (&p), proc (p)
    {
        setSize (520, 300);

        addAndMakeVisible (loadButton);
        loadButton.setButtonText ("Load SFZ / WAV...");
        loadButton.onClick = [this]
        {
            chooser = std::make_unique<juce::FileChooser> ("Pumili ng .sfz o audio files", juce::File(),
                                                           "*.sfz;*.wav;*.aif;*.aiff;*.flac;*.ogg");
            juce::Component::SafePointer<SamplerEditor> safe (this);
            chooser->launchAsync (juce::FileBrowserComponent::openMode
                                  | juce::FileBrowserComponent::canSelectFiles
                                  | juce::FileBrowserComponent::canSelectMultipleItems,
                                  [safe] (const juce::FileChooser& fc)
                                  {
                                      juce::StringArray paths;
                                      for (auto& f : fc.getResults())
                                          paths.add (f.getFullPathName());
                                      if (safe != nullptr && paths.size() > 0)
                                          safe->load (paths);
                                  });
        };

        addAndMakeVisible (status);
        status.setText (proc.statusText, juce::dontSendNotification);
        status.setJustificationType (juce::Justification::centredLeft);

        static const char* ids[]   = { "attack", "decay", "sustain", "release", "gain" };
        static const char* names[] = { "Attack", "Decay", "Sustain", "Release", "Gain (dB)" };
        for (int i = 0; i < 5; ++i)
        {
            sliders[i].setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            sliders[i].setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 20);
            addAndMakeVisible (sliders[i]);

            labels[i].setText (names[i], juce::dontSendNotification);
            labels[i].setJustificationType (juce::Justification::centred);
            addAndMakeVisible (labels[i]);

            attachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
                proc.apvts, ids[i], sliders[i]);
        }
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff1e1e24));
        g.setColour (juce::Colours::white);
        g.setFont (juce::FontOptions (20.0f));
        g.drawText ("LightSampler", 160, 12, getWidth() - 170, 28, juce::Justification::centredLeft);
        g.setColour (juce::Colours::grey);
        g.setFont (juce::FontOptions (12.0f));
        g.drawText ("I-drag dito ang .sfz o .wav files", 10, getHeight() - 24, getWidth() - 20, 18,
                    juce::Justification::centred);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (10);
        auto top = area.removeFromTop (40);
        loadButton.setBounds (top.removeFromLeft (140).reduced (0, 4));
        area.removeFromTop (30);
        status.setBounds (10, 52, getWidth() - 20, 22);

        area.removeFromBottom (24);
        const int w = area.getWidth() / 5;
        for (int i = 0; i < 5; ++i)
        {
            auto col = area.removeFromLeft (w);
            labels[i].setBounds (col.removeFromTop (20));
            sliders[i].setBounds (col);
        }
    }

    bool isInterestedInFileDrag (const juce::StringArray& files) override
    {
        for (auto& f : files)
            if (juce::File (f).hasFileExtension ("sfz;wav;aif;aiff;flac;ogg"))
                return true;
        return false;
    }

    void filesDropped (const juce::StringArray& files, int, int) override { load (files); }

private:
    void load (const juce::StringArray& paths)
    {
        status.setText (proc.loadFiles (paths), juce::dontSendNotification);
    }

    SamplerAudioProcessor& proc;
    juce::TextButton loadButton;
    juce::Label status;
    juce::Slider sliders[5];
    juce::Label labels[5];
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachments[5];
    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SamplerEditor)
};
