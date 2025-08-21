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
            
            // Special handling for very low thresholds to prevent excessive sensitivity
            auto effectiveThreshold = threshold;
            if (threshold < -40.0f) {
                // For very low thresholds, add some hysteresis to prevent constant compression
                auto hysteresis = std::min(5.0f, std::abs(threshold + 40.0f) * 0.1f);
                if (overThreshold < hysteresis) {
                    overThreshold = 0.0f; // Ignore very small overages
                }
            }
            
            if (overThreshold > 0.0f) {
                // Protect against division by very small ratio values
                auto safeRatio = std::max(ratio, 1.0f);
                gainReduction = overThreshold - (overThreshold / safeRatio);
                
                // Limit maximum gain reduction to prevent extreme compression
                gainReduction = std::min(gainReduction, 60.0f);
            }
        }
        
        // Apply attack/release envelope with improved stability
        auto targetGainReduction = gainReduction;
        
        // Ensure coefficients are valid and prevent extreme values
        auto safeAttackCoeff = jlimit(0.001f, 0.95f, attackCoeff);
        auto safeReleaseCoeff = jlimit(0.001f, 0.95f, releaseCoeff);
        
        // Calculate envelope difference for stability check
        auto envelopeDiff = targetGainReduction - envelope;
        
        if (targetGainReduction > envelope)
        {
            // Attack phase - approaching compression
            envelope += safeAttackCoeff * envelopeDiff;
        }
        else
        {
            // Release phase - releasing compression
            // Use smaller coefficient for very small differences to prevent noise
            auto effectiveReleaseCoeff = safeReleaseCoeff;
            if (std::abs(envelopeDiff) < 0.1f) {
                effectiveReleaseCoeff *= 0.5f; // Slower release for small changes
            }
            envelope += effectiveReleaseCoeff * envelopeDiff;
        }
        
        // Ensure envelope stays within reasonable bounds with smoother clamping
        envelope = jlimit(0.0f, 60.0f, envelope);
        
        // Additional stability: prevent very small envelope values that could cause noise
        if (envelope < 0.001f && targetGainReduction < 0.001f) {
            envelope = 0.0f;
        }
        
        // Apply compression and makeup gain with safety limits
        auto gainInDb = -envelope + makeupGain;
        
        // Special handling for high makeup gain to prevent excessive amplification
        if (makeupGain > 10.0f) {
            // Reduce makeup gain effectiveness for very high values to prevent noise
            auto makeupReduction = (makeupGain - 10.0f) * 0.3f; // Reduce by 70%
            gainInDb = -envelope + (10.0f + makeupReduction);
        }
        
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
    
    /** Set attack time in milliseconds */
    void setAttack(float newAttack) 
    { 
        attack = newAttack; 
        updateCoefficients();
    }
    
    /** Set release time in milliseconds */
    void setRelease(float newRelease) 
    { 
        // Smooth parameter changes to prevent sudden jumps
        auto clampedRelease = jlimit(5.0f, 5000.0f, newRelease);
        
        // If the change is very large, smooth it
        if (std::abs(clampedRelease - release) > 50.0f) {
            auto diff = clampedRelease - release;
            release += diff * 0.1f; // Gradual change
        } else {
            release = clampedRelease;
        }
        
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
            // Ensure attack and release times are within safe bounds
            auto safeAttack = jlimit(1.0f, 1000.0f, attack);  // Increased minimum attack
            auto safeRelease = jlimit(10.0f, 5000.0f, release); // Increased minimum release
            
            // Calculate time constants in samples with more robust approach
            auto attackTimeInSamples = safeAttack * 0.001f * static_cast<float>(sampleRate);
            auto releaseTimeInSamples = safeRelease * 0.001f * static_cast<float>(sampleRate);
            
            // Prevent division by zero and ensure reasonable values
            attackTimeInSamples = std::max(attackTimeInSamples, 2.0f);  // Increased minimum
            releaseTimeInSamples = std::max(releaseTimeInSamples, 5.0f); // Increased minimum
            
            // Use more stable coefficient calculation
            // For exponential decay: coeff = 1 - exp(-1 / timeInSamples)
            attackCoeff = 1.0f - expf(-1.0f / attackTimeInSamples);
            releaseCoeff = 1.0f - expf(-1.0f / releaseTimeInSamples);
            
            // Clamp coefficients to safe range with more conservative limits
            attackCoeff = jlimit(0.01f, 0.8f, attackCoeff);   // More conservative
            releaseCoeff = jlimit(0.01f, 0.8f, releaseCoeff); // More conservative
            
            // Additional safety: if both attack and release are very fast, slow them down
            if (safeAttack < 2.0f && safeRelease < 20.0f) {
                attackCoeff *= 0.5f;  // Slow down attack
                releaseCoeff *= 0.5f; // Slow down release
            }
            
            // Debug output for release issues
            #ifdef DEBUG
            if (release != lastRelease) {
                printf("Release changed: %.2fms -> coeff: %.6f (samples: %.2f)\n", 
                       release, releaseCoeff, releaseTimeInSamples);
                lastRelease = release;
            }
            #endif
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
        // Convert normalized values to actual parameter ranges with safer limits
        threshold = jlimit(-60.0f, 0.0f, -60.0f + thresholdNorm * 60.0f);  // -60dB to 0dB
        ratio = jlimit(1.0f, 20.0f, 1.0f + ratioNorm * 19.0f);            // 1:1 to 20:1
        attack = jlimit(0.5f, 100.0f, 0.5f + attackNorm * 99.5f);         // 0.5ms to 100ms (more conservative)
        release = jlimit(5.0f, 1000.0f, 5.0f + releaseNorm * 995.0f);     // 5ms to 1000ms (more conservative)
        makeupGain = jlimit(0.0f, 18.0f, makeupNorm * 18.0f);             // 0dB to 18dB (reduced from 24dB)
        
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
    
    // Debug variables
    #ifdef DEBUG
    float lastRelease = -1.0f;
    #endif
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Compressor)
};
