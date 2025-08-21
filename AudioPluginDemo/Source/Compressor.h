#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>
#include <algorithm>

//==============================================================================
/** A standalone compressor DSP class that can be used independently of the GUI.
    
    This class encapsulates all the compressor logic and can be easily tested,
    reused, or integrated into other projects.
*/
class Compressor
{
public:
    //==============================================================================
    Compressor() = default;
    
    /** Constructor with initial parameters */
    Compressor(float initialThreshold, float initialRatio, float initialAttack, 
               float initialRelease, float initialMakeupGain)
        : threshold(initialThreshold)
        , ratio(initialRatio)
        , attack(initialAttack)
        , release(initialRelease)
        , makeupGain(initialMakeupGain)
    {
    }
    
    ~Compressor() = default;
    
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
        
        // Clamp input level to reasonable range to prevent extreme calculations
        inputLevel = jlimit(-120.0f, 20.0f, inputLevel);
        
        // Calculate gain reduction
        auto gainReduction = 0.0f;
        if (inputLevel > threshold)
        {
            auto overThreshold = inputLevel - threshold;
            
            // Keep original behavior - hysteresis was causing artifacts
            
            if (overThreshold > 0.0f) {
                // Protect against division by very small ratio values
                auto safeRatio = std::max(ratio, 1.0f);
                gainReduction = overThreshold - (overThreshold / safeRatio);
                
                // Limit maximum gain reduction to prevent extreme compression
                gainReduction = std::min(gainReduction, 60.0f);
            }
        }
        
        // Apply attack/release envelope - simplified approach
        auto targetGainReduction = gainReduction;
        
        if (targetGainReduction > envelope)
        {
            // Attack phase
            envelope += attackCoeff * (targetGainReduction - envelope);
        }
        else
        {
            // Release phase  
            envelope += releaseCoeff * (targetGainReduction - envelope);
        }
        
        // Simple bounds check
        envelope = jlimit(0.0f, 60.0f, envelope);
        
        // Apply compression and makeup gain with safety limits
        auto gainInDb = -envelope + makeupGain;
        
        // Keep original makeup gain behavior
        
        // Limit total gain to prevent extreme amplification
        gainInDb = jlimit(-60.0f, 20.0f, gainInDb);
        
        auto compressedGain = powf(10.0f, gainInDb / 20.0f);
        
        // Final safety check on gain value
        if (!std::isfinite(compressedGain))
            compressedGain = 1.0f;
        
        // Limit gain to reasonable range
        compressedGain = jlimit(0.001f, 10.0f, compressedGain);
        
        auto output = input * compressedGain;
        
        // Final output safety check
        if (!std::isfinite(output))
            return 0.0f;
            
        // Soft limiting to prevent harsh clipping
        return softLimit(output);
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
    /** Set compressor parameters and update coefficients */
    void setParameters(float newThreshold, float newRatio, float newAttack, 
                      float newRelease, float newMakeupGain)
    {
        threshold = newThreshold;
        ratio = newRatio;
        attack = newAttack;
        release = newRelease;
        makeupGain = newMakeupGain;
        
        updateCoefficients();
    }
    
    /** Set threshold in dB */
    void setThreshold(float newThreshold) 
    { 
        threshold = newThreshold; 
        updateCoefficients();
    }
    
    /** Set compression ratio */
    void setRatio(float newRatio) 
    { 
        ratio = newRatio; 
        updateCoefficients();
    }
    
    /** Set ratio by predefined preset index */
    void setRatioPreset(int presetIndex)
    {
        static const float ratioPresets[] = { 1.0f, 2.0f, 3.0f, 4.0f, 6.0f, 8.0f, 10.0f, 20.0f };
        if (presetIndex >= 0 && presetIndex < 8) {
            ratio = ratioPresets[presetIndex];
            updateCoefficients();
        }
    }
    
    /** Get current ratio preset index */
    int getRatioPresetIndex() const
    {
        static const float ratioPresets[] = { 1.0f, 2.0f, 3.0f, 4.0f, 6.0f, 8.0f, 10.0f, 20.0f };
        for (int i = 0; i < 8; ++i) {
            if (std::abs(ratio - ratioPresets[i]) < 0.1f) {
                return i;
            }
        }
        return 3; // Default to 4:1 if no match
    }
    
    /** Get ratio preset name */
    static String getRatioPresetName(int presetIndex)
    {
        static const String presetNames[] = { "1:1", "2:1", "3:1", "4:1", "6:1", "8:1", "10:1", "20:1" };
        if (presetIndex >= 0 && presetIndex < 8) {
            return presetNames[presetIndex];
        }
        return "4:1";
    }
    
    /** Set attack time in milliseconds */
    void setAttack(float newAttack) 
    { 
        attack = newAttack; 
        updateCoefficients();
    }
    
    /** Set release time in milliseconds */
    void setRelease(float newRelease) 
    { 
        release = newRelease; 
        updateCoefficients();
    }
    
    /** Set makeup gain in dB */
    void setMakeupGain(float newMakeupGain) 
    { 
        makeupGain = newMakeupGain; 
    }
    
    //==============================================================================
    /** Get current parameter values */
    float getThreshold() const { return threshold; }
    float getRatio() const { return ratio; }
    float getAttack() const { return attack; }
    float getRelease() const { return release; }
    float getMakeupGain() const { return makeupGain; }
    
    /** Get current envelope value (for metering) */
    float getCurrentEnvelope() const { return envelope; }
    
    /** Get current gain reduction in dB (for metering) */
    float getCurrentGainReduction() const { return -envelope; }
    
    /** Get current input level in dB (for metering) */
    float getCurrentInputLevel() const { return inputLevel; }
    
    /** Get current output level in dB (for metering) */
    float getCurrentOutputLevel() const { return outputLevel; }
    
    //==============================================================================
    /** Update coefficients based on current parameters */
    void updateCoefficients()
    {
        if (sampleRate > 0.0)
        {
            // Use a simpler, more stable approach
            // Convert milliseconds to samples
            auto attackSamples = attack * 0.001f * static_cast<float>(sampleRate);
            auto releaseSamples = release * 0.001f * static_cast<float>(sampleRate);
            
            // Ensure minimum values to prevent instability
            attackSamples = std::max(attackSamples, 1.0f);
            releaseSamples = std::max(releaseSamples, 1.0f);
            
            // Simple exponential coefficient calculation
            attackCoeff = 1.0f - expf(-2.2f / attackSamples);
            releaseCoeff = 1.0f - expf(-2.2f / releaseSamples);
            
            // Simple safety limits
            attackCoeff = jlimit(0.001f, 0.999f, attackCoeff);
            releaseCoeff = jlimit(0.001f, 0.999f, releaseCoeff);
        }
    }
    
    /** Soft limiting function to prevent harsh clipping */
    float softLimit(float input)
    {
        // Tanh-based soft limiting
        if (std::abs(input) > 0.95f)
        {
            return std::tanh(input * 0.5f) * 0.95f;
        }
        return input;
    }
    
    /** Update coefficients from normalized parameter values (0.0 to 1.0) */
    void updateFromNormalizedParameters(float thresholdNorm, float ratioNorm, 
                                      float attackNorm, float releaseNorm, float makeupNorm)
    {
        // Convert normalized values to actual parameter ranges
        threshold = -60.0f + thresholdNorm * 60.0f;  // -60dB to 0dB
        ratio = 1.0f + ratioNorm * 19.0f;            // 1:1 to 20:1
        attack = 0.1f + attackNorm * 399.9f;         // 0.1ms to 400ms
        release = 1.0f + releaseNorm * 399.0f;       // 1ms to 400ms
        makeupGain = -30.0f + makeupNorm * 60.0f;    // -30dB to +30dB
        
        updateCoefficients();
    }
    
private:
    //==============================================================================
    // Compressor parameters
    float threshold = -20.0f;      // dB
    float ratio = 4.0f;           // compression ratio
    float attack = 10.0f;         // milliseconds
    float release = 100.0f;       // milliseconds
    float makeupGain = 0.0f;      // dB
    
    // Calculated coefficients
    float attackCoeff = 0.0f;
    float releaseCoeff = 0.0f;
    
    // State variables
    float envelope = 0.0f;        // current gain reduction
    double sampleRate = 44100.0;  // sample rate
    
    // Metering variables
    mutable float inputLevel = -60.0f;
    mutable float outputLevel = -60.0f;
    

    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Compressor)
};
