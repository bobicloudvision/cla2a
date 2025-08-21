#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>

//==============================================================================
/** Custom Look and Feel for glowing labels */
class GlowingLabelLookAndFeel : public LookAndFeel_V4
{
public:
    void drawLabel(Graphics& g, Label& label) override
    {
        auto bounds = label.getLocalBounds().toFloat();
        
        // Draw glow effect
        g.setColour(label.findColour(Label::textColourId).withAlpha(0.3f));
        g.drawText(label.getText(), bounds.expanded(2.0f), Justification::centred);
        
        // Draw main text
        g.setColour(label.findColour(Label::textColourId));
        g.drawText(label.getText(), bounds, Justification::centred);
    }
};

//==============================================================================
/** Custom Look and Feel for slider hover effects */
class SliderHoverEffect : public LookAndFeel_V4
{
public:
    SliderHoverEffect()
    {
        setColour(Slider::thumbColourId, Colours::lightblue);
        setColour(Slider::trackColourId, Colours::lightblue.withAlpha(0.3f));
        setColour(Slider::backgroundColourId, Colours::darkgrey);
    }
    
    void drawSliderThumb(Graphics& g, float x, float y, float width, float height,
                         float sliderPos, float minSliderPos, float maxSliderPos,
                         const Slider::SliderStyle style, Slider& slider)
    {
        auto bounds = Rectangle<float>(x, y, width, height);
        
        // Add glow effect when hovering
        if (slider.isMouseOver())
        {
            g.setColour(Colours::lightblue.withAlpha(0.4f));
            g.fillEllipse(bounds.expanded(4.0f));
        }
        
        // Draw thumb
        g.setColour(slider.findColour(Slider::thumbColourId));
        g.fillEllipse(bounds);
        
        // Add highlight
        g.setColour(Colours::white.withAlpha(0.3f));
        g.fillEllipse(bounds.reduced(2.0f));
    }
};

//==============================================================================
/** Custom level meter component */
class LevelMeter : public Component, public Timer
{
public:
    LevelMeter(const String& labelText = "METER")
        : label(labelText)
    {
        addAndMakeVisible(label);
        addAndMakeVisible(valueLabel);
        
        label.setJustificationType(Justification::centred);
        label.setColour(Label::textColourId, Colours::white);
        label.setFont(FontOptions(12.0f, Font::bold));
        
        valueLabel.setJustificationType(Justification::centred);
        valueLabel.setColour(Label::textColourId, Colours::lightblue);
        valueLabel.setFont(FontOptions(10.0f, Font::plain));
        
        startTimerHz(60); // Smooth animation
    }
    
    ~LevelMeter() override { stopTimer(); }
    
    void setValue(float newValue)
    {
        targetValue = newValue;
    }
    
    void paint(Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        
        // Background gradient
        ColourGradient meterGrad(Colour(0xFF1a1a1a), 0.0f, 0.0f,
                                Colour(0xFF2a2a2a), bounds.getWidth(), bounds.getHeight(), false);
        meterGrad.addColour(0.5f, Colour(0xFF222222));
        g.setGradientFill(meterGrad);
        g.fillRoundedRectangle(bounds, 8.0f);
        
        // Outer glow
        g.setColour(Colours::lightblue.withAlpha(0.2f));
        g.drawRoundedRectangle(bounds, 8.0f, 2.0f);
        
        // Draw scale lines
        g.setColour(Colours::white.withAlpha(0.1f));
        for (int i = 0; i <= 10; ++i)
        {
            auto y = bounds.getY() + (bounds.getHeight() * i / 10.0f);
            g.drawHorizontalLine(y, bounds.getX() + 5, bounds.getRight() - 5);
        }
        
        // Draw meter bar
        auto meterHeight = bounds.getHeight() * 0.8f;
        auto meterY = bounds.getY() + (bounds.getHeight() - meterHeight) * 0.5f;
        auto meterBounds = Rectangle<float>(bounds.getX() + 10, meterY, bounds.getWidth() - 20, meterHeight);
        
        // Background for meter
        g.setColour(Colours::darkgrey);
        g.fillRoundedRectangle(meterBounds, 4.0f);
        
        // Calculate meter level (convert dB to 0-1 range)
        auto normalizedLevel = (currentValue + 60.0f) / 60.0f;
        normalizedLevel = jlimit(0.0f, 1.0f, normalizedLevel);
        
        // Draw meter bar with color coding
        auto barHeight = meterHeight * normalizedLevel;
        auto barY = meterY + meterHeight - barHeight;
        auto barBounds = Rectangle<float>(meterBounds.getX(), barY, meterBounds.getWidth(), barHeight);
        
        // Color based on level
        Colour barColour;
        if (normalizedLevel < 0.6f)
            barColour = Colours::green;
        else if (normalizedLevel < 0.8f)
            barColour = Colours::yellow;
        else
            barColour = Colours::red;
        
        g.setColour(barColour);
        g.fillRoundedRectangle(barBounds, 4.0f);
        
        // Add glow effect for red levels
        if (normalizedLevel > 0.8f)
        {
            g.setColour(Colours::red.withAlpha(0.3f));
            g.drawRoundedRectangle(barBounds.expanded(2.0f), 6.0f, 3.0f);
        }
        
        // Update value label
        valueLabel.setText(String(currentValue, 1) + " dB", dontSendNotification);
    }
    
    void resized() override
    {
        auto bounds = getLocalBounds();
        label.setBounds(bounds.removeFromTop(20));
        valueLabel.setBounds(bounds.removeFromBottom(20));
    }
    
    void timerCallback() override
    {
        // Smooth interpolation for meter value
        auto diff = targetValue - currentValue;
        currentValue += diff * 0.1f;
        
        if (std::abs(diff) > 0.1f)
            repaint();
    }
    
private:
    Label label;
    Label valueLabel;
    float currentValue = -60.0f;
    float targetValue = -60.0f;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LevelMeter)
};

//==============================================================================
/** The main compressor editor component */
class CompressorEditor : public AudioProcessorEditor, private Value::Listener, private Timer
{
public:
    CompressorEditor(AudioProcessor& processor, AudioProcessorValueTreeState& processorState)
        : AudioProcessorEditor(processor),
          processorState(processorState),
          thresholdAttachment(processorState, "threshold", thresholdSlider),
          attackAttachment(processorState, "attack", attackSlider),
          releaseAttachment(processorState, "release", releaseSlider),
          makeupAttachment(processorState, "makeup", makeupSlider)
    {
        // Set up sliders with better styling
        setupThresholdSlider(thresholdSlider, "THRESHOLD", -20.0f);
        setupRatioComboBox();
        setupTimeSlider(attackSlider, "ATTACK", 10.0f, 0.0f, 400.0f);
        setupTimeSlider(releaseSlider, "RELEASE", 100.0f, 1.0f, 400.0f);
        setupSlider(makeupSlider, "MAKEUP", 0.0f);
        makeupSlider.setTextValueSuffix(" dB");

        // Add visual elements
        addAndMakeVisible(titleLabel);
        addAndMakeVisible(compressionMeter);
        addAndMakeVisible(inputMeter);
        addAndMakeVisible(outputMeter);

        // Style the title
        titleLabel.setText("COMPRESSOR", dontSendNotification);
        titleLabel.setFont(FontOptions(36.0f, Font::bold));
        titleLabel.setJustificationType(Justification::centred);
        titleLabel.setColour(Label::textColourId, Colours::lightblue);
        titleLabel.setColour(Label::backgroundColourId, Colours::transparentBlack);
        
        // Add a subtle glow effect to the title
        titleLabel.setLookAndFeel(new GlowingLabelLookAndFeel());

        // Set resize limits for this plug-in
        setResizeLimits(700, 700, 1200, 1000);
        setResizable(true, getProcessor().wrapperType != AudioPluginInstance::wrapperType_AudioUnitv3);

        lastUIWidth.referTo(processorState.state.getChildWithName("uiState").getPropertyAsValue("width", nullptr));
        lastUIHeight.referTo(processorState.state.getChildWithName("uiState").getPropertyAsValue("height", nullptr));

        // set our component's initial size to be the last one that was stored in the filter's settings
        setSize(lastUIWidth.getValue(), lastUIHeight.getValue());

        lastUIWidth.addListener(this);
        lastUIHeight.addListener(this);

        // Start timer for meter updates
        startTimerHz(30);
    }

    ~CompressorEditor() override {}

    //==============================================================================
    void paint(Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        
        // Create a more sophisticated gradient background
        ColourGradient backgroundGrad(Colour(0xFF0a0a0a), 0.0f, 0.0f,
                                      Colour(0xFF2a2a2a), bounds.getWidth(), bounds.getHeight(), false);
        backgroundGrad.addColour(0.5f, Colour(0xFF1a1a1a));
        g.setGradientFill(backgroundGrad);
        g.fillAll();
        
        // Add subtle grid pattern
        g.setColour(Colours::white.withAlpha(0.03f));
        for (int i = 0; i < getWidth(); i += 20)
        {
            g.drawVerticalLine(i, 0.0f, (float)getHeight());
        }
        for (int i = 0; i < getHeight(); i += 20)
        {
            g.drawHorizontalLine(i, 0.0f, (float)getWidth());
        }
        
        // Draw main panel with glow effect
        auto panelBounds = bounds.reduced(8.0f);
        
        // Outer glow
        g.setColour(Colours::lightblue.withAlpha(0.1f));
        g.drawRoundedRectangle(panelBounds.expanded(4.0f), 12.0f, 3.0f);
        
        // Main panel
        ColourGradient panelGrad(Colour(0xFF1e1e1e), panelBounds.getX(), panelBounds.getY(),
                                  Colour(0xFF1e1e1e), panelBounds.getRight(), panelBounds.getBottom(), false);
        panelGrad.addColour(0.5f, Colour(0xFF2a2a2a));
        g.setGradientFill(panelGrad);
        g.fillRoundedRectangle(panelBounds, 12.0f);
        
        // Inner highlight
        g.setColour(Colours::white.withAlpha(0.05f));
        g.drawRoundedRectangle(panelBounds.reduced(1.0f), 11.0f, 1.0f);
        
        // Draw separator lines with glow
        g.setColour(Colours::lightblue.withAlpha(0.3f));
        g.drawHorizontalLine(130, 8.0f, (float)getWidth() - 8.0f);
        g.drawHorizontalLine(getHeight() - 130, 8.0f, (float)getWidth() - 8.0f);
        
        // Add corner accents
        g.setColour(Colours::lightblue.withAlpha(0.4f));
        g.fillEllipse(8.0f, 8.0f, 8.0f, 8.0f);
        g.fillEllipse((float)getWidth() - 16.0f, 8.0f, 8.0f, 8.0f);
        g.fillEllipse(8.0f, (float)getHeight() - 16.0f, 8.0f, 8.0f);
        g.fillEllipse((float)getWidth() - 16.0f, (float)getHeight() - 16.0f, 8.0f, 8.0f);
    }

    void resized() override
    {
        auto bounds = getLocalBounds().reduced(25);

        // Title at the top
        titleLabel.setBounds(bounds.removeFromTop(70));

        // Top section with meters
        auto topSection = bounds.removeFromTop(140);
        auto meterWidth = topSection.getWidth() / 3;
        
        inputMeter.setBounds(topSection.removeFromLeft(meterWidth).reduced(10));
        compressionMeter.setBounds(topSection.removeFromLeft(meterWidth).reduced(10));
        outputMeter.setBounds(topSection.removeFromLeft(meterWidth).reduced(10));

        // Middle section with sliders
        auto middleSection = bounds.removeFromTop(400);
        auto topRow = middleSection.removeFromTop(200);
        auto bottomRow = middleSection.removeFromTop(200);

        // Top row sliders
        auto sliderWidth = topRow.getWidth() / 2;
        thresholdSlider.setBounds(topRow.removeFromLeft(sliderWidth).reduced(20));
        ratioComboBox.setBounds(topRow.removeFromLeft(sliderWidth).reduced(20));

        // Bottom row sliders
        auto bottomSliderWidth = bottomRow.getWidth() / 3;
        attackSlider.setBounds(bottomRow.removeFromLeft(bottomSliderWidth).reduced(15));
        releaseSlider.setBounds(bottomRow.removeFromLeft(bottomSliderWidth).reduced(15));
        makeupSlider.setBounds(bottomRow.removeFromLeft(bottomSliderWidth).reduced(15));
    }

    //==============================================================================
    void valueChanged(Value& value) override
    {
        if (value.refersToSameSourceAs(lastUIWidth))
            setSize(lastUIWidth.getValue(), getHeight());
        else if (value.refersToSameSourceAs(lastUIHeight))
            setSize(getWidth(), lastUIHeight.getValue());
    }

    void timerCallback() override
    {
        // Update meters with current values
        // For now, use slider values since we can't access processor-specific methods
        auto threshold = thresholdSlider.getValue();
        auto makeup = makeupSlider.getValue();

        // Simulate some compression activity for the meter
        static float meterValue = -20.0f;
        static float direction = 1.0f;
        
        // Create a simple oscillating pattern for demo purposes
        meterValue += direction * 0.5f;
        if (meterValue > -5.0f) direction = -1.0f;
        if (meterValue < -30.0f) direction = 1.0f;

        // Update meters with simulated values
        compressionMeter.setValue(meterValue);
        inputMeter.setValue(threshold + 5.0f);  // Show input relative to threshold
        outputMeter.setValue(meterValue + makeup);
    }

private:
    //==============================================================================
    AudioProcessor& getProcessor() const
    {
        return processor;
    }
    
    // Reference to the processor state
    AudioProcessorValueTreeState& processorState;

    void setupSlider(Slider& slider, const String& labelText, float defaultValue)
    {
        slider.setSliderStyle(Slider::LinearVertical);
        slider.setTextBoxStyle(Slider::TextBoxBelow, false, 150, 30);
        slider.setColour(Slider::textBoxOutlineColourId, Colours::lightblue.withAlpha(0.5f));
        slider.setColour(Slider::trackColourId, Colours::lightblue.withAlpha(0.3f));
        slider.setColour(Slider::backgroundColourId, Colours::darkgrey);
        slider.setValue(defaultValue);
        slider.setSize(slider.getWidth(), 200);

        addAndMakeVisible(slider);

        // Create and add label
        auto* label = new Label();
        label->setText(labelText, dontSendNotification);
        label->setFont(FontOptions(18.0f, Font::bold));
        label->setColour(Label::textColourId, Colours::white);
        label->setColour(Label::backgroundColourId, Colours::transparentBlack);
        label->setJustificationType(Justification::centred);
        label->attachToComponent(&slider, false);
        addAndMakeVisible(label);

        // Apply hover effect
        slider.setLookAndFeel(new SliderHoverEffect());
    }
    
    void setupThresholdSlider(Slider& slider, const String& labelText, float defaultValue)
    {
        slider.setSliderStyle(Slider::LinearVertical);
        slider.setTextBoxStyle(Slider::TextBoxBelow, false, 150, 30);
        slider.setColour(Slider::textBoxBackgroundColourId, Colours::black.withAlpha(0.7f));
        slider.setColour(Slider::textBoxOutlineColourId, Colours::lightblue.withAlpha(0.5f));
        slider.setColour(Slider::trackColourId, Colours::lightblue.withAlpha(0.3f));
        slider.setColour(Slider::backgroundColourId, Colours::darkgrey);
        slider.setValue(defaultValue);
        slider.setSize(slider.getWidth(), 200);
        
        // Add suffix for threshold values
        slider.setTextValueSuffix(" dB");

        addAndMakeVisible(slider);

        // Create and add label
        auto* label = new Label();
        label->setText(labelText, dontSendNotification);
        label->setFont(FontOptions(18.0f, Font::bold));
        label->setColour(Label::textColourId, Colours::white);
        label->setColour(Label::backgroundColourId, Colours::transparentBlack);
        label->setJustificationType(Justification::centred);
        label->attachToComponent(&slider, false);
        addAndMakeVisible(label);

        // Apply hover effect
        slider.setLookAndFeel(new SliderHoverEffect());
    }
    
    void setupTimeSlider(Slider& slider, const String& labelText, float defaultValue, float minValue, float maxValue)
    {
        slider.setSliderStyle(Slider::LinearVertical);
        slider.setTextBoxStyle(Slider::TextBoxBelow, false, 150, 30);
        slider.setColour(Slider::textBoxBackgroundColourId, Colours::black.withAlpha(0.7f));
        slider.setColour(Slider::textBoxOutlineColourId, Colours::lightblue.withAlpha(0.5f));
        slider.setColour(Slider::trackColourId, Colours::lightblue.withAlpha(0.3f));
        slider.setColour(Slider::backgroundColourId, Colours::darkgrey);
        
        // Set range in milliseconds
        slider.setRange(minValue, maxValue, 0.1);
        slider.setValue(defaultValue);
        slider.setSize(slider.getWidth(), 200);
        
        // Add suffix for time values
        slider.setTextValueSuffix(" ms");

        addAndMakeVisible(slider);

        // Create and add label
        auto* label = new Label();
        label->setText(labelText, dontSendNotification);
        label->setFont(FontOptions(18.0f, Font::bold));
        label->setColour(Label::textColourId, Colours::white);
        label->setColour(Label::backgroundColourId, Colours::transparentBlack);
        label->setJustificationType(Justification::centred);
        label->attachToComponent(&slider, false);
        addAndMakeVisible(label);

        // Apply hover effect
        slider.setLookAndFeel(new SliderHoverEffect());
    }
    
    void setupRatioComboBox()
    {
        // Add ratio options
        ratioComboBox.addItem("1:1", 1);
        ratioComboBox.addItem("2:1", 2);
        ratioComboBox.addItem("3:1", 3);
        ratioComboBox.addItem("4:1", 4);
        ratioComboBox.addItem("6:1", 5);
        ratioComboBox.addItem("8:1", 6);
        ratioComboBox.addItem("10:1", 7);
        ratioComboBox.addItem("20:1", 8);
        
        ratioComboBox.setSelectedId(4); // Default to 4:1
        ratioComboBox.setJustificationType(Justification::centred);
        ratioComboBox.setColour(ComboBox::backgroundColourId, Colours::black.withAlpha(0.7f));
        ratioComboBox.setColour(ComboBox::outlineColourId, Colours::lightblue.withAlpha(0.5f));
        ratioComboBox.setColour(ComboBox::textColourId, Colours::white);
        ratioComboBox.setColour(ComboBox::arrowColourId, Colours::lightblue);
        
        ratioComboBox.onChange = [this]() {
            auto selectedId = ratioComboBox.getSelectedId();
            if (selectedId > 0) {
                // Convert to preset index (0-7)
                auto presetIndex = selectedId - 1;
                // Update the processor's ratio parameter
                if (auto* ratioParam = processorState.getParameter("ratio")) {
                    ratioParam->setValueNotifyingHost(presetIndex / 7.0f); // Normalize to 0-1
                }
            }
        };
        
        addAndMakeVisible(ratioComboBox);
        
        // Create and add label
        auto* label = new Label();
        label->setText("RATIO", dontSendNotification);
        label->setFont(FontOptions(18.0f, Font::bold));
        label->setColour(Label::textColourId, Colours::white);
        label->setColour(Label::backgroundColourId, Colours::transparentBlack);
        label->setJustificationType(Justification::centred);
        label->attachToComponent(&ratioComboBox, false);
        addAndMakeVisible(label);
    }

    //==============================================================================
    // UI Components
    Slider thresholdSlider, attackSlider, releaseSlider, makeupSlider;
    ComboBox ratioComboBox;
    Label titleLabel;
    LevelMeter compressionMeter{"COMPRESSION"};
    LevelMeter inputMeter{"INPUT"};
    LevelMeter outputMeter{"OUTPUT"};

    // Parameter attachments
    AudioProcessorValueTreeState::SliderAttachment thresholdAttachment;
    AudioProcessorValueTreeState::SliderAttachment attackAttachment;
    AudioProcessorValueTreeState::SliderAttachment releaseAttachment;
    AudioProcessorValueTreeState::SliderAttachment makeupAttachment;

    // UI state
    Value lastUIWidth, lastUIHeight;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CompressorEditor)
};
