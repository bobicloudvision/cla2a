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
   OTHER TORT ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
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
 description:           Simple compressor audio plugin with separated GUI and DSP logic.

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

#include "Compressor.h"
#include "CompressorEditor.h"

//==============================================================================
/** A simple compressor that applies dynamic range compression to audio.
    
    This processor uses a separate Compressor class for DSP logic and
    CompressorEditor for the GUI, demonstrating clean separation of concerns.
*/
class JuceDemoPluginAudioProcessor final : public AudioProcessor
{
public:
    //==============================================================================
    JuceDemoPluginAudioProcessor()
        : AudioProcessor (getBusesProperties()),
          state (*this, nullptr, "state",
                 { std::make_unique<AudioParameterFloat> (ParameterID { "threshold", 1 }, "Threshold", NormalisableRange<float> (-60.0f, 0.0f), -20.0f),
                   std::make_unique<AudioParameterFloat> (ParameterID { "ratio", 1 }, "Ratio", NormalisableRange<float> (1.0f, 20.0f), 4.0f),
                   std::make_unique<AudioParameterFloat> (ParameterID { "attack", 1 }, "Attack", NormalisableRange<float> (0.5f, 100.0f), 10.0f),
                   std::make_unique<AudioParameterFloat> (ParameterID { "release", 1 }, "Release", NormalisableRange<float> (5.0f, 1000.0f), 100.0f),
                   std::make_unique<AudioParameterFloat> (ParameterID { "makeup", 1 }, "Makeup Gain", NormalisableRange<float> (0.0f, 18.0f), 0.0f) })
    {
        // Add a sub-tree to store the state of our UI
        state.state.addChild ({ "uiState", { { "width",  700 }, { "height", 700 } }, {} }, -1, nullptr);
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
        // Initialize the compressor with the new sample rate
        compressor.prepareToPlay(newSampleRate);
    }

    void releaseResources() override
    {
        // When playback stops, you can use this as the opportunity to free up any
        // spare memory, etc.
    }

    void reset() override
    {
        // Reset the compressor state
        compressor.reset();
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
        return new JuceDemoPluginAudioProcessorEditor (*this, state);
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
    
    // Make envelope accessible for UI display
    float getCurrentEnvelope() const { return compressor.getCurrentEnvelope(); }
    float getCurrentThreshold() const { return compressor.getThreshold(); }
    float getCurrentRatio() const { return compressor.getRatio(); }

private:
    //==============================================================================
    /** This is the editor component that our filter will display. */
    using JuceDemoPluginAudioProcessorEditor = CompressorEditor;

    //==============================================================================
    template <typename FloatType>
    void process (AudioBuffer<FloatType>& buffer, MidiBuffer& /*midiMessages*/)
    {
        // In case we have more outputs than inputs, we'll clear any output
        // channels that didn't contain input data, (because these aren't
        // guaranteed to be empty - they may contain garbage).
        for (auto i = getTotalNumInputChannels(); i < getTotalNumOutputChannels(); ++i)
            buffer.clear (i, 0, buffer.getNumSamples());

        // Update compressor parameters if they've changed
        updateCompressorParameters();

        // Process the buffer through the compressor
        compressor.processBuffer(buffer);
    }

    void updateCompressorParameters()
    {
        // Get normalized values (0.0 to 1.0) and convert to actual ranges
        auto thresholdNorm = state.getParameter ("threshold")->getValue();
        auto ratioNorm = state.getParameter ("ratio")->getValue();
        auto attackNorm = state.getParameter ("attack")->getValue();
        auto releaseNorm = state.getParameter ("release")->getValue();
        auto makeupNorm = state.getParameter ("makeup")->getValue();
        
        // Update the compressor with new parameters
        compressor.updateFromNormalizedParameters(thresholdNorm, ratioNorm, attackNorm, releaseNorm, makeupNorm);
    }

    // The compressor instance
    Compressor compressor{-20.0f, 4.0f, 10.0f, 100.0f, 0.0f};

    static BusesProperties getBusesProperties()
    {
        return BusesProperties().withInput  ("Input",  AudioChannelSet::stereo(), true)
                                .withOutput ("Output", AudioChannelSet::stereo(), true);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (JuceDemoPluginAudioProcessor)
};
