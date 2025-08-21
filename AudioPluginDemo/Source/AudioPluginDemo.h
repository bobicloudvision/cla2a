/*
  ==============================================================================

   This file is part of the JUCE framework examples.
   Copyright (c) Raw Material Software Limited

   The code included in this file is provided under the terms of the ISC license
   http://www.isc.org/downloads/software-support-policy/isc-license. Permission
   to use, copy, modify, and/or distribute this software for any purpose with or
   without fee is hereby granted provided that the above copyright notice and
   this permission notice appear in all copies.

   THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
   REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
   AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
   INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
   LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
   OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
   PERFORMANCE OF THIS SOFTWARE.

  ==============================================================================
*/

/*******************************************************************************
 The block below describes the properties of this PIP. A PIP is a short snippet
 of code that can be read by the Projucer and used to generate a JUCE project.

 BEGIN_JUCE_PIP_METADATA

 name:                  AudioPluginDemo
 version:               1.0.0
 vendor:                JUCE
 website:               http://juce.com
 description:           Simple compressor audio plugin.

 dependencies:          juce_audio_basics, juce_audio_devices, juce_audio_formats,
                        juce_audio_plugin_client, juce_audio_processors,
                        juce_audio_utils, juce_core, juce_data_structures,
                        juce_events, juce_graphics, juce_gui_basics, juce_gui_extra
 exporters:             xcode_mac, vs2022, linux_make, xcode_iphone, androidstudio

 moduleFlags:           JUCE_STRICT_REFCOUNTEDPOINTER=1

 type:                  AudioProcessor
 mainClass:             JuceDemoPluginAudioProcessor

 useLocalCopy:          1

 pluginCharacteristics: pluginIsEffect

 extraPluginFormats:    AUv3

 END_JUCE_PIP_METADATA

*******************************************************************************/

#pragma once

//==============================================================================
/** A simple compressor that applies dynamic range compression to audio. */
class JuceDemoPluginAudioProcessor final : public AudioProcessor
{
public:
    //==============================================================================
    JuceDemoPluginAudioProcessor()
        : AudioProcessor (getBusesProperties()),
          state (*this, nullptr, "state",
                 { std::make_unique<AudioParameterFloat> (ParameterID { "threshold", 1 }, "Threshold", NormalisableRange<float> (-60.0f, 0.0f), -20.0f),
                   std::make_unique<AudioParameterFloat> (ParameterID { "ratio", 1 }, "Ratio", NormalisableRange<float> (1.0f, 20.0f), 4.0f),
                   std::make_unique<AudioParameterFloat> (ParameterID { "attack", 1 }, "Attack", NormalisableRange<float> (0.1f, 100.0f), 10.0f),
                   std::make_unique<AudioParameterFloat> (ParameterID { "release", 1 }, "Release", NormalisableRange<float> (10.0f, 1000.0f), 100.0f),
                   std::make_unique<AudioParameterFloat> (ParameterID { "makeup", 1 }, "Makeup Gain", NormalisableRange<float> (0.0f, 24.0f), 0.0f) })
    {
        // Add a sub-tree to store the state of our UI
        state.state.addChild ({ "uiState", { { "width",  400 }, { "height", 300 } }, {} }, -1, nullptr);
    }

    //==============================================================================
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override
    {
        // Only mono/stereo and input/output must have same layout
        const auto& mainOutput = layouts.getMainOutputChannelSet();
        const auto& mainInput  = layouts.getMainInputChannelSet();

        // input and output layout must either be the same or the input must be disabled altogether
        if (! mainInput.isDisabled() && mainInput != mainOutput)
            return false;

        // only allow stereo and mono
        if (mainOutput.size() > 2)
            return false;

        return true;
    }

    void prepareToPlay (double newSampleRate, int /*samplesPerBlock*/) override
    {
        // Use this method as the place to do any pre-playback
        // initialisation that you need..
        sampleRate = newSampleRate;
        
        // Initialize compressor parameters
        updateCompressorParameters();
    }

    void releaseResources() override
    {
        // When playback stops, you can use this as an opportunity to free up any
        // spare memory, etc.
    }

    void reset() override
    {
        // Use this method as the place to clear any delay lines, buffers, etc, as it
        // means there's been a break in the audio's continuity.
        envelope = 0.0f;
    }

    bool supportsDoublePrecisionProcessing() const override { return true; }

    //==============================================================================
    void processBlock (AudioBuffer<float>& buffer, MidiBuffer& midiMessages) override
    {
        jassert (! isUsingDoublePrecision());
        process (buffer, midiMessages);
    }

    void processBlock (AudioBuffer<double>& buffer, MidiBuffer& midiMessages) override
    {
        jassert (isUsingDoublePrecision());
        process (buffer, midiMessages);
    }

    //==============================================================================
    bool hasEditor() const override                                   { return true; }

    AudioProcessorEditor* createEditor() override
    {
        return new JuceDemoPluginAudioProcessorEditor (*this);
    }

    //==============================================================================
    const String getName() const override                             { return "AudioPluginDemo"; }
    bool acceptsMidi() const override                                 { return false; }
    bool producesMidi() const override                                { return false; }
    double getTailLengthSeconds() const override                      { return 0.0; }

    //==============================================================================
    int getNumPrograms() override                                     { return 0; }
    int getCurrentProgram() override                                  { return 0; }
    void setCurrentProgram (int) override                             {}
    const String getProgramName (int) override                        { return "None"; }
    void changeProgramName (int, const String&) override              {}

    //==============================================================================
    void getStateInformation (MemoryBlock& destData) override
    {
        // Store an xml representation of our state.
        if (auto xmlState = state.copyState().createXml())
            copyXmlToBinary (*xmlState, destData);
    }

    void setStateInformation (const void* data, int sizeInBytes) override
    {
        // Restore our plug-in's state from the xml representation stored in the above
        // method.
        if (auto xmlState = getXmlFromBinary (data, sizeInBytes))
            state.replaceState (ValueTree::fromXml (*xmlState));
    }

    //==============================================================================
    // These properties are public so that our editor component can access them
    AudioProcessorValueTreeState state;

private:
    //==============================================================================
    /** This is the editor component that our filter will display. */
    class JuceDemoPluginAudioProcessorEditor final : public AudioProcessorEditor,
                                                     private Value::Listener
    {
    public:
        JuceDemoPluginAudioProcessorEditor (JuceDemoPluginAudioProcessor& owner)
            : AudioProcessorEditor (owner),
              thresholdAttachment (owner.state, "threshold", thresholdSlider),
              ratioAttachment     (owner.state, "ratio",     ratioSlider),
              attackAttachment    (owner.state, "attack",    attackSlider),
              releaseAttachment   (owner.state, "release",   releaseSlider),
              makeupAttachment    (owner.state, "makeup",    makeupSlider)
        {
            // add some sliders..
            addAndMakeVisible (thresholdSlider);
            thresholdSlider.setSliderStyle (Slider::Rotary);

            addAndMakeVisible (ratioSlider);
            ratioSlider.setSliderStyle (Slider::Rotary);

            addAndMakeVisible (attackSlider);
            attackSlider.setSliderStyle (Slider::Rotary);

            addAndMakeVisible (releaseSlider);
            releaseSlider.setSliderStyle (Slider::Rotary);

            addAndMakeVisible (makeupSlider);
            makeupSlider.setSliderStyle (Slider::Rotary);

            // add some labels for the sliders..
            thresholdLabel.attachToComponent (&thresholdSlider, false);
            thresholdLabel.setFont (FontOptions (11.0f));

            ratioLabel.attachToComponent (&ratioSlider, false);
            ratioLabel.setFont (FontOptions (11.0f));

            attackLabel.attachToComponent (&attackSlider, false);
            attackLabel.setFont (FontOptions (11.0f));

            releaseLabel.attachToComponent (&releaseSlider, false);
            releaseLabel.setFont (FontOptions (11.0f));

            makeupLabel.attachToComponent (&makeupSlider, false);
            makeupLabel.setFont (FontOptions (11.0f));

            // set resize limits for this plug-in
            setResizeLimits (400, 300, 800, 600);
            setResizable (true, owner.wrapperType != wrapperType_AudioUnitv3);

            lastUIWidth .referTo (owner.state.state.getChildWithName ("uiState").getPropertyAsValue ("width",  nullptr));
            lastUIHeight.referTo (owner.state.state.getChildWithName ("uiState").getPropertyAsValue ("height", nullptr));

            // set our component's initial size to be the last one that was stored in the filter's settings
            setSize (lastUIWidth.getValue(), lastUIHeight.getValue());

            lastUIWidth. addListener (this);
            lastUIHeight.addListener (this);
        }

        ~JuceDemoPluginAudioProcessorEditor() override {}

        //==============================================================================
        void paint (Graphics& g) override
        {
            g.setColour (getLookAndFeel().findColour (ResizableWindow::backgroundColourId));
            g.fillAll();
        }

        void resized() override
        {
            // This lays out our child components...
            auto r = getLocalBounds().reduced (8);

            // Top row
            auto topRow = r.removeFromTop (80);
            thresholdSlider.setBounds (topRow.removeFromLeft (jmin (120, topRow.getWidth() / 3)));
            ratioSlider.setBounds (topRow.removeFromLeft (jmin (120, topRow.getWidth() / 2)));
            attackSlider.setBounds (topRow.removeFromLeft (jmin (120, topRow.getWidth())));

            // Bottom row
            auto bottomRow = r.removeFromTop (80);
            releaseSlider.setBounds (bottomRow.removeFromLeft (jmin (120, bottomRow.getWidth() / 2)));
            makeupSlider.setBounds (bottomRow.removeFromLeft (jmin (120, bottomRow.getWidth())));

            lastUIWidth  = getWidth();
            lastUIHeight = getHeight();
        }

        // called when the stored window size changes
        void valueChanged (Value&) override
        {
            setSize (lastUIWidth.getValue(), lastUIHeight.getValue());
        }

    private:
        Label thresholdLabel { {}, "Threshold (dB):" },
              ratioLabel     { {}, "Ratio:" },
              attackLabel    { {}, "Attack (ms):" },
              releaseLabel   { {}, "Release (ms):" },
              makeupLabel    { {}, "Makeup (dB):" };

        Slider thresholdSlider, ratioSlider, attackSlider, releaseSlider, makeupSlider;
        AudioProcessorValueTreeState::SliderAttachment thresholdAttachment, ratioAttachment, 
                                                      attackAttachment, releaseAttachment, makeupAttachment;

        // these are used to persist the UI's size - the values are stored along with the
        // filter's other parameters, and the UI component will update them when it gets
        // resized.
        Value lastUIWidth, lastUIHeight;
    };

    //==============================================================================
    template <typename FloatType>
    void process (AudioBuffer<FloatType>& buffer, MidiBuffer& /*midiMessages*/)
    {
        auto numSamples = buffer.getNumSamples();
        auto numChannels = buffer.getNumChannels();

        // In case we have more outputs than inputs, we'll clear any output
        // channels that didn't contain input data, (because these aren't
        // guaranteed to be empty - they may contain garbage).
        for (auto i = getTotalNumInputChannels(); i < getTotalNumOutputChannels(); ++i)
            buffer.clear (i, 0, numSamples);

        // Update compressor parameters if they've changed
        updateCompressorParameters();

        // Process each channel
        for (auto channel = 0; channel < numChannels; ++channel)
        {
            auto channelData = buffer.getWritePointer (channel);
            
            for (auto sample = 0; sample < numSamples; ++sample)
            {
                auto input = channelData[sample];
                
                // Calculate input level in dB
                auto inputLevel = 20.0f * log10f (std::abs (input) + 1e-6f);
                
                // Calculate gain reduction
                auto gainReduction = 0.0f;
                if (inputLevel > threshold)
                {
                    auto overThreshold = inputLevel - threshold;
                    gainReduction = overThreshold - (overThreshold / ratio);
                }
                
                // Apply attack/release envelope
                auto targetGainReduction = gainReduction;
                if (targetGainReduction > envelope)
                {
                    envelope += attackCoeff * (targetGainReduction - envelope);
                }
                else
                {
                    envelope += releaseCoeff * (targetGainReduction - envelope);
                }
                
                // Apply compression and makeup gain
                auto compressedGain = powf (10.0f, (envelope + makeupGain) / 20.0f);
                channelData[sample] = input * compressedGain;
            }
        }
    }

    void updateCompressorParameters()
    {
        threshold = state.getParameter ("threshold")->getValue();
        ratio = state.getParameter ("ratio")->getValue();
        attack = state.getParameter ("attack")->getValue();
        release = state.getParameter ("release")->getValue();
        makeupGain = state.getParameter ("makeup")->getValue();
        
        // Convert to coefficients
        attackCoeff = 1.0f - expf (-2.2f / (attack * 0.001f * sampleRate));
        releaseCoeff = 1.0f - expf (-2.2f / (release * 0.001f * sampleRate));
    }

    // Compressor parameters
    float threshold = -20.0f;
    float ratio = 4.0f;
    float attack = 10.0f;
    float release = 100.0f;
    float makeupGain = 0.0f;
    
    // Calculated coefficients
    float attackCoeff = 0.0f;
    float releaseCoeff = 0.0f;
    
    // State variables
    float envelope = 0.0f;
    double sampleRate = 44100.0;

    static BusesProperties getBusesProperties()
    {
        return BusesProperties().withInput  ("Input",  AudioChannelSet::stereo(), true)
                                .withOutput ("Output", AudioChannelSet::stereo(), true);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (JuceDemoPluginAudioProcessor)
};
