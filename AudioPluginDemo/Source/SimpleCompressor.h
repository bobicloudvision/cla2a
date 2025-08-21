#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>
#include <algorithm>

//==============================================================================
/** A simple, classic compressor design without complex logic.
    
    This is a clean, straightforward implementation that focuses on
    getting the basics right rather than trying to handle every edge case.
*/
class SimpleCompressor
{
public:
    //==============================================================================
    SimpleCompressor() = default;
    ~SimpleCompressor() = default;
    
    //==============================================================================
    /** Prepare the compressor for playback */
    void prepareToPlay(double newSampleRate)
    {
        sampleRate = newSampleRate;
        updateCoefficients();
        reset();
    }
    
    /** Reset the compressor state */
    void reset()
    {
        envelope = 0.0f;
    }
    
    /** Process a single sample through the compressor */
    float processSample(float input)
    {
        // Safety check for invalid input
        if (!std::isfinite(input))
            return 0.0f;
        
        // Calculate input level in dB with safety limits
        auto absInput = std::abs(input);
        absInput = std::max(absInput, 1e-10f);  // Prevent log of zero
        auto inputLevel = 20.0f * log10f(absInput);
        
        // Clamp input level to reasonable range
        inputLevel = std::max(-120.0f, std::min(20.0f, inputLevel));
        
        // Calculate gain reduction
        auto gainReduction = 0.0f;
        if (inputLevel > threshold)
        {
            auto overThreshold = inputLevel - threshold;
            gainReduction = overThreshold - (overThreshold / ratio);
            
            // Limit maximum gain reduction to prevent extreme compression
            gainReduction = std::min(gainReduction, 60.0f);
        }
        
        // Apply attack/release envelope
        if (gainReduction > envelope)
        {
            // Attack phase
            envelope = envelope + attackCoeff * (gainReduction - envelope);
        }
        else
        {
            // Release phase
            envelope = envelope + releaseCoeff * (gainReduction - envelope);
        }
        
        // Bounds check on envelope
        envelope = std::max(0.0f, std::min(60.0f, envelope));
        
        // Apply compression and makeup gain with safety limits
        auto gainInDb = -envelope + makeupGain;
        
        // Limit total gain to prevent clipping
        gainInDb = std::max(-60.0f, std::min(20.0f, gainInDb));
        
        auto compressedGain = powf(10.0f, gainInDb / 20.0f);
        
        // Final safety check on gain value
        if (!std::isfinite(compressedGain))
            compressedGain = 1.0f;
        
        // Limit gain to reasonable range
        compressedGain = std::max(0.001f, std::min(10.0f, compressedGain));
        
        auto output = input * compressedGain;
        
        // Final output safety check and soft limiting
        if (!std::isfinite(output))
            return 0.0f;
        
        // Soft limiting to prevent hard clipping
        if (std::abs(output) > 0.95f)
        {
            // Simple tanh soft limiting
            output = std::tanh(output * 0.8f) * 0.95f;
        }
        
        return output;
    }
    
    /** Process a buffer of samples */
    template<typename FloatType>
    void processBuffer(juce::AudioBuffer<FloatType>& buffer)
    {
        auto numSamples = buffer.getNumSamples();
        auto numChannels = buffer.getNumChannels();
        
        for (auto channel = 0; channel < numChannels; ++channel)
        {
            auto channelData = buffer.getWritePointer(channel);
            
            for (auto sample = 0; sample < numSamples; ++sample)
            {
                channelData[sample] = processSample(static_cast<float>(channelData[sample]));
            }
        }
    }
    
    //==============================================================================
    /** Set compressor parameters */
    void setParameters(float newThreshold, float newRatio, float newAttack, 
                      float newRelease, float newMakeupGain)
    {
        threshold = newThreshold;
        ratio = std::max(newRatio, 1.0f);
        attack = std::max(newAttack, 0.1f);    // Minimum 0.1ms to prevent instability
        release = std::max(newRelease, 1.0f);  // Minimum 1.0ms to prevent instability
        makeupGain = newMakeupGain;
        
        updateCoefficients();
    }
    
    void setThreshold(float newThreshold) { threshold = newThreshold; }
    void setRatio(float newRatio) { ratio = std::max(newRatio, 1.0f); updateCoefficients(); }
    void setAttack(float newAttack) { attack = std::max(newAttack, 0.1f); updateCoefficients(); }
    void setRelease(float newRelease) { release = std::max(newRelease, 1.0f); updateCoefficients(); }
    void setMakeupGain(float newMakeupGain) { makeupGain = newMakeupGain; }
    
    //==============================================================================
    /** Get current parameter values */
    float getThreshold() const { return threshold; }
    float getRatio() const { return ratio; }
    float getAttack() const { return attack; }
    float getRelease() const { return release; }
    float getMakeupGain() const { return makeupGain; }
    float getCurrentGainReduction() const { return -envelope; }
    float getCurrentEnvelope() const { return envelope; }
    float getCurrentInputLevel() const { return 0.0f; }  // Simple version doesn't track this
    float getCurrentOutputLevel() const { return 0.0f; } // Simple version doesn't track this
    
private:
    //==============================================================================
    /** Update attack/release coefficients */
    void updateCoefficients()
    {
        if (sampleRate > 0.0)
        {
            // Simple, classic coefficient calculation
            auto attackSamples = attack * 0.001f * static_cast<float>(sampleRate);
            auto releaseSamples = release * 0.001f * static_cast<float>(sampleRate);
            
            // Standard exponential coefficient formula
            attackCoeff = 1.0f - expf(-1.0f / attackSamples);
            releaseCoeff = 1.0f - expf(-1.0f / releaseSamples);
        }
    }
    
    //==============================================================================
    // Parameters
    float threshold = -20.0f;      // dB
    float ratio = 4.0f;           // compression ratio
    float attack = 10.0f;         // milliseconds
    float release = 100.0f;       // milliseconds
    float makeupGain = 0.0f;      // dB
    
    // Coefficients
    float attackCoeff = 0.0f;
    float releaseCoeff = 0.0f;
    
    // State
    float envelope = 0.0f;
    double sampleRate = 44100.0;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SimpleCompressor)
};
