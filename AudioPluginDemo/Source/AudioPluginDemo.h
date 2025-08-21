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
    
    // Make envelope accessible for UI display
    float getCurrentEnvelope() const { return envelope; }
    float getCurrentThreshold() const { return threshold; }
    float getCurrentRatio() const { return ratio; }

private:
    //==============================================================================
    /** This is the editor component that our filter will display. */
    class JuceDemoPluginAudioProcessorEditor final : public AudioProcessorEditor,
                                                     private Value::Listener,
                                                     private Timer
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
            // Set up sliders with better styling
            setupSlider (thresholdSlider, "THRESHOLD", -20.0f);
            setupSlider (ratioSlider, "RATIO", 4.0f);
            setupSlider (attackSlider, "ATTACK", 10.0f);
            setupSlider (releaseSlider, "RELEASE", 100.0f);
            setupSlider (makeupSlider, "MAKEUP", 0.0f);

            // Add visual elements
            addAndMakeVisible (titleLabel);
            addAndMakeVisible (compressionMeter);
            addAndMakeVisible (inputMeter);
            addAndMakeVisible (outputMeter);

            // Style the title
            titleLabel.setText ("COMPRESSOR", dontSendNotification);
            titleLabel.setFont (FontOptions (36.0f, Font::bold));
            titleLabel.setJustificationType (Justification::centred);
            titleLabel.setColour (Label::textColourId, Colours::lightblue);
            titleLabel.setColour (Label::backgroundColourId, Colours::transparentBlack);
            
            // Add a subtle glow effect to the title
            titleLabel.setLookAndFeel (new GlowingLabelLookAndFeel());

            // Set resize limits for this plug-in
            setResizeLimits (700, 700, 1200, 1000);
            setResizable (true, owner.wrapperType != wrapperType_AudioUnitv3);

            lastUIWidth .referTo (owner.state.state.getChildWithName ("uiState").getPropertyAsValue ("width",  nullptr));
            lastUIHeight.referTo (owner.state.state.getChildWithName ("uiState").getPropertyAsValue ("height", nullptr));

            // set our component's initial size to be the last one that was stored in the filter's settings
            setSize (lastUIWidth.getValue(), lastUIHeight.getValue());

            lastUIWidth. addListener (this);
            lastUIHeight.addListener (this);

            // Start timer for meter updates
            startTimerHz (30);
        }

        ~JuceDemoPluginAudioProcessorEditor() override {}

        //==============================================================================
        void paint (Graphics& g) override
        {
            auto bounds = getLocalBounds().toFloat();
            
            // Create a more sophisticated gradient background
            ColourGradient backgroundGrad (Colour (0xFF0a0a0a), 0.0f, 0.0f,
                                          Colour (0xFF2a2a2a), bounds.getWidth(), bounds.getHeight(), false);
            backgroundGrad.addColour (0.5f, Colour (0xFF1a1a1a));
            g.setGradientFill (backgroundGrad);
            g.fillAll();
            
            // Add subtle grid pattern
            g.setColour (Colours::white.withAlpha (0.03f));
            for (int i = 0; i < getWidth(); i += 20)
            {
                g.drawVerticalLine (i, 0.0f, (float) getHeight());
            }
            for (int i = 0; i < getHeight(); i += 20)
            {
                g.drawHorizontalLine (i, 0.0f, (float) getWidth());
            }
            
            // Draw main panel with glow effect
            auto panelBounds = bounds.reduced (8.0f);
            
            // Outer glow
            g.setColour (Colours::lightblue.withAlpha (0.1f));
            g.drawRoundedRectangle (panelBounds.expanded (4.0f), 12.0f, 3.0f);
            
            // Main panel
            ColourGradient panelGrad (Colour (0xFF1e1e1e), panelBounds.getX(), panelBounds.getY(),
                                      Colour (0xFF1e1e1e), panelBounds.getRight(), panelBounds.getBottom(), false);
            panelGrad.addColour (0.5f, Colour (0xFF2a2a2a));
            g.setGradientFill (panelGrad);
            g.fillRoundedRectangle (panelBounds, 12.0f);
            
            // Inner highlight
            g.setColour (Colours::white.withAlpha (0.05f));
            g.drawRoundedRectangle (panelBounds.reduced (1.0f), 11.0f, 1.0f);
            
            // Draw separator lines with glow
            g.setColour (Colours::lightblue.withAlpha (0.3f));
            g.drawHorizontalLine (130, 8.0f, (float) getWidth() - 8.0f);
            g.drawHorizontalLine (getHeight() - 130, 8.0f, (float) getWidth() - 8.0f);
            
            // Add corner accents
            g.setColour (Colours::lightblue.withAlpha (0.4f));
            g.fillEllipse (8.0f, 8.0f, 8.0f, 8.0f);
            g.fillEllipse ((float) getWidth() - 16.0f, 8.0f, 8.0f, 8.0f);
            g.fillEllipse (8.0f, (float) getHeight() - 16.0f, 8.0f, 8.0f);
            g.fillEllipse ((float) getWidth() - 16.0f, (float) getHeight() - 16.0f, 8.0f, 8.0f);
        }

        void resized() override
        {
            auto bounds = getLocalBounds().reduced (25);

            // Title at the top
            titleLabel.setBounds (bounds.removeFromTop (70));

            // Top section with meters
            auto topSection = bounds.removeFromTop (140);
            inputMeter.setBounds (topSection.removeFromLeft (topSection.getWidth() / 3).reduced (15));
            compressionMeter.setBounds (topSection.removeFromLeft (topSection.getWidth() / 2).reduced (15));
            outputMeter.setBounds (topSection.reduced (15));

            // Middle section with sliders - give them much more height
            auto middleSection = bounds.removeFromTop (280);
            
            // Top row of sliders - each slider gets 120px height
            auto topRow = middleSection.removeFromTop (140);
            thresholdSlider.setBounds (topRow.removeFromLeft (topRow.getWidth() / 3).reduced (20));
            ratioSlider.setBounds (topRow.removeFromLeft (topRow.getWidth() / 2).reduced (20));
            attackSlider.setBounds (topRow.reduced (20));

            // Bottom row of sliders - each slider gets 120px height
            auto bottomRow = middleSection.removeFromTop (140);
            releaseSlider.setBounds (bottomRow.removeFromLeft (bottomRow.getWidth() / 2).reduced (20));
            makeupSlider.setBounds (bottomRow.reduced (20));

            lastUIWidth  = getWidth();
            lastUIHeight = getHeight();
        }

        void timerCallback() override
        {
            // Get real compressor values from the processor
            auto& processor = getProcessor();
            auto envelope = processor.getCurrentEnvelope();
            auto threshold = processor.getCurrentThreshold();
            auto makeup = makeupSlider.getValue();
            
            // Convert envelope to dB for display (envelope is negative gain reduction)
            auto compressionDb = -envelope;
            
            // Update meters with real values
            compressionMeter.setValue (compressionDb);
            inputMeter.setValue (threshold + 5.0f);  // Show input relative to threshold
            outputMeter.setValue (compressionDb + makeup);
        }

        // called when the stored window size changes
        void valueChanged (Value&) override
        {
            setSize (lastUIWidth.getValue(), lastUIHeight.getValue());
        }

    private:
        //==============================================================================
        JuceDemoPluginAudioProcessor& getProcessor() const
        {
            return static_cast<JuceDemoPluginAudioProcessor&> (processor);
        }

        void setupSlider (Slider& slider, const String& label, double defaultValue)
        {
            addAndMakeVisible (slider);
            slider.setSliderStyle (Slider::LinearVertical);
            slider.setTextBoxStyle (Slider::TextBoxBelow, false, 150, 30);
            
            // Set minimum height for the slider to ensure it's tall enough
            slider.setSize (slider.getWidth(), 200);
            
            // Enhanced slider colors with glow effects
            slider.setColour (Slider::thumbColourId, Colours::lightblue);
            slider.setColour (Slider::trackColourId, Colours::white.withAlpha (0.2f));
            slider.setColour (Slider::backgroundColourId, Colours::black.withAlpha (0.3f));
            slider.setColour (Slider::textBoxTextColourId, Colours::white);
            slider.setColour (Slider::textBoxBackgroundColourId, Colours::black.withAlpha (0.7f));
            slider.setColour (Slider::textBoxOutlineColourId, Colours::lightblue.withAlpha (0.5f));
            
            // Create and style the label with glow effect
            auto* labelComponent = new Label (label, label);
            labelComponent->setFont (FontOptions (18.0f, Font::bold));
            labelComponent->setColour (Label::textColourId, Colours::lightblue);
            labelComponent->setColour (Label::backgroundColourId, Colours::transparentBlack);
            labelComponent->setJustificationType (Justification::centred);
            labelComponent->attachToComponent (&slider, false);
            addAndMakeVisible (labelComponent);
            
            // Add hover effect
            slider.addMouseListener (new SliderHoverEffect (slider), false);
        }

        // Custom meter component
        class LevelMeter : public Component, public Timer
        {
        public:
            LevelMeter (const String& name) : meterName (name)
            {
                setSize (100, 100);
                startTimerHz (60); // Smooth animation
            }

            ~LevelMeter() override
            {
                stopTimer();
            }

            void setValue (float newValue)
            {
                targetValue = newValue;
            }

            void paint (Graphics& g) override
            {
                auto bounds = getLocalBounds().toFloat();
                
                // Background with gradient
                ColourGradient meterGrad (Colour (0xFF0a0a0a), bounds.getX(), bounds.getY(),
                                         Colour (0xFF0a0a0a), bounds.getRight(), bounds.getBottom(), false);
                meterGrad.addColour (0.5f, Colour (0xFF1a1a1a));
                g.setGradientFill (meterGrad);
                g.fillRoundedRectangle (bounds, 8.0f);
                
                // Outer glow
                g.setColour (Colours::lightblue.withAlpha (0.2f));
                g.drawRoundedRectangle (bounds.expanded (3.0f), 11.0f, 2.5f);
                
                // Border
                g.setColour (Colours::white.withAlpha (0.3f));
                g.drawRoundedRectangle (bounds, 8.0f, 2.0f);
                
                // Meter bar background
                auto meterHeight = bounds.getHeight() - 35;
                auto meterWidth = bounds.getWidth() - 16;
                auto meterX = bounds.getX() + 8;
                auto meterY = bounds.getY() + 30;
                
                // Draw meter background with subtle pattern
                g.setColour (Colours::black.withAlpha (0.6f));
                g.fillRoundedRectangle (meterX, meterY, meterWidth, meterHeight, 4.0f);
                
                // Add subtle lines for scale
                g.setColour (Colours::white.withAlpha (0.1f));
                for (int i = 1; i < 5; ++i)
                {
                    auto lineY = meterY + (meterHeight * i / 5.0f);
                    g.drawHorizontalLine (lineY, meterX, meterX + meterWidth);
                }
                
                // Normalize value from dB to 0-1 range
                auto normalizedValue = jlimit (0.0f, 1.0f, (currentValue + 60.0f) / 60.0f);
                
                // Draw meter level with enhanced colors and glow
                auto levelHeight = normalizedValue * meterHeight;
                auto levelY = meterY + meterHeight - levelHeight;
                
                // Color gradient based on level
                Colour levelColour;
                if (normalizedValue > 0.8f)
                {
                    levelColour = Colours::red;
                    // Add red glow
                    g.setColour (Colours::red.withAlpha (0.3f));
                    g.fillRoundedRectangle (meterX - 3, levelY - 3, meterWidth + 6, levelHeight + 6, 4.0f);
                }
                else if (normalizedValue > 0.6f)
                {
                    levelColour = Colours::yellow;
                }
                else if (normalizedValue > 0.3f)
                {
                    levelColour = Colours::lightgreen;
                }
                else
                {
                    levelColour = Colours::lightblue;
                }
                
                // Main meter bar
                g.setColour (levelColour);
                g.fillRoundedRectangle (meterX, levelY, meterWidth, levelHeight, 4.0f);
                
                // Inner highlight
                g.setColour (levelColour.brighter (0.3f));
                g.fillRoundedRectangle (meterX + 2, levelY + 2, meterWidth - 4, jmax (4.0f, levelHeight - 4), 3.0f);
                
                // Draw label with glow
                g.setColour (Colours::lightblue);
                g.setFont (FontOptions (14.0f, Font::bold));
                g.drawText (meterName, bounds.removeFromTop (25).toFloat(), Justification::centred);
                
                // Add value display
                g.setColour (Colours::white);
                g.setFont (FontOptions (12.0f, Font::bold));
                auto valueText = String (roundToInt (currentValue)) + " dB";
                g.drawText (valueText, bounds.removeFromBottom (20).toFloat(), Justification::centred);
            }
            
            void timerCallback() override
            {
                // Smooth value interpolation
                auto diff = targetValue - currentValue;
                currentValue += diff * 0.1f;
                
                if (std::abs (diff) > 0.1f)
            repaint();
        }

    private:
            String meterName;
            float currentValue = -60.0f;
            float targetValue = -60.0f;
        };

        Label titleLabel;
        LevelMeter inputMeter { "INPUT" };
        LevelMeter compressionMeter { "COMP" };
        LevelMeter outputMeter { "OUTPUT" };

        Slider thresholdSlider, ratioSlider, attackSlider, releaseSlider, makeupSlider;
        AudioProcessorValueTreeState::SliderAttachment thresholdAttachment, ratioAttachment, 
                                                      attackAttachment, releaseAttachment, makeupAttachment;

        // these are used to persist the UI's size - the values are stored along with the
        // filter's other parameters, and the UI component will update them when it gets
        // resized.
        Value lastUIWidth, lastUIHeight;

        // Custom LookAndFeel for glowing effects
        class GlowingLabelLookAndFeel : public LookAndFeel_V4
        {
        public:
            void drawLabel (Graphics& g, Label& label) override
            {
                g.setColour (label.findColour (Label::textColourId));
                g.setFont (label.getFont());
                
                // Draw glow effect
                g.setColour (label.findColour (Label::textColourId).withAlpha (0.3f));
                g.drawText (label.getText(), label.getLocalBounds().expanded (2, 2), label.getJustificationType());
                
                // Draw main text
                g.setColour (label.findColour (Label::textColourId));
                g.drawText (label.getText(), label.getLocalBounds(), label.getJustificationType());
            }
        };
        
        // Hover effect for sliders
        class SliderHoverEffect : public MouseListener
        {
        public:
            SliderHoverEffect (Slider& s) : slider (s) {}
            
            void mouseEnter (const MouseEvent&) override
            {
                slider.setColour (Slider::thumbColourId, Colours::white);
                slider.repaint();
            }
            
            void mouseExit (const MouseEvent&) override
            {
                slider.setColour (Slider::thumbColourId, Colours::lightblue);
                slider.repaint();
            }
            
        private:
            Slider& slider;
        };
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
                // Note: envelope is negative, so we subtract it to get the compressed gain
                auto compressedGain = powf (10.0f, (-envelope + makeupGain) / 20.0f);
                channelData[sample] = input * compressedGain;
            }
        }
    }

    void updateCompressorParameters()
    {
        // Get normalized values (0.0 to 1.0) and convert to actual ranges
        auto thresholdNorm = state.getParameter ("threshold")->getValue();
        auto ratioNorm = state.getParameter ("ratio")->getValue();
        auto attackNorm = state.getParameter ("attack")->getValue();
        auto releaseNorm = state.getParameter ("release")->getValue();
        auto makeupNorm = state.getParameter ("makeup")->getValue();
        
        // Convert normalized values to actual parameter ranges
        threshold = -60.0f + thresholdNorm * 60.0f;  // -60dB to 0dB
        ratio = 1.0f + ratioNorm * 19.0f;           // 1:1 to 20:1
        attack = 0.1f + attackNorm * 99.9f;         // 0.1ms to 100ms
        release = 10.0f + releaseNorm * 990.0f;     // 10ms to 1000ms
        makeupGain = makeupNorm * 24.0f;            // 0dB to 24dB
        
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
