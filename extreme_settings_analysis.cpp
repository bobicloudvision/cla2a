#include <iostream>
#include <iomanip>
#include <cmath>
#include <algorithm>

/**
 * Analysis of extreme compressor settings without JUCE dependencies
 * Settings: Threshold -35dB, Attack 0ms, Release 1ms, Makeup 0dB
 */

// Simplified compressor calculation functions
float calculateGainReduction(float inputDb, float thresholdDb, float ratio)
{
    if (inputDb > thresholdDb) {
        float overThreshold = inputDb - thresholdDb;
        return overThreshold - (overThreshold / ratio);
    }
    return 0.0f;
}

float dbToLinear(float db) {
    return std::pow(10.0f, db / 20.0f);
}

float linearToDb(float linear) {
    return 20.0f * std::log10(std::max(linear, 1e-10f));
}

bool willTriggerExtremeSaturation(float attack, float release, float ratio) {
    return (attack <= 2.0f && release < 10.0f && ratio > 8.0f);
}

int main()
{
    std::cout << "=== EXTREME COMPRESSOR SETTINGS ANALYSIS ===" << std::endl;
    std::cout << std::endl;
    
    // Your specified settings
    float threshold = -35.0f;  // dB
    float attack = 0.0f;       // ms  
    float release = 1.0f;      // ms
    float makeupGain = 0.0f;   // dB
    
    std::cout << "🎛️  YOUR SETTINGS:" << std::endl;
    std::cout << "   Threshold: " << threshold << " dB" << std::endl;
    std::cout << "   Attack: " << attack << " ms" << std::endl;
    std::cout << "   Release: " << release << " ms" << std::endl;
    std::cout << "   Makeup Gain: " << makeupGain << " dB" << std::endl;
    std::cout << std::endl;
    
    // Test different ratios
    std::vector<float> ratios = {2.0f, 4.0f, 8.0f, 10.0f, 20.0f};
    
    std::cout << "📊 COMPRESSION BEHAVIOR AT DIFFERENT RATIOS:" << std::endl;
    std::cout << std::endl;
    
    // Test input levels
    std::vector<std::pair<std::string, float>> testInputs = {
        {"Very Quiet", -60.0f},
        {"Quiet", -40.0f},
        {"Medium", -20.0f},
        {"Loud", -6.0f},
        {"Very Loud", -1.0f}
    };
    
    for (auto ratio : ratios) {
        std::cout << "--- RATIO " << ratio << ":1 ---" << std::endl;
        
        bool extremeSaturation = willTriggerExtremeSaturation(attack, release, ratio);
        if (extremeSaturation) {
            std::cout << "🔥 EXTREME SATURATION MODE ACTIVE!" << std::endl;
        }
        
        std::cout << std::fixed << std::setprecision(1);
        std::cout << "Input Level -> Gain Reduction -> Output Level" << std::endl;
        
        for (auto& input : testInputs) {
            float inputDb = input.second;
            float gainReduction = calculateGainReduction(inputDb, threshold, ratio);
            float outputDb = inputDb - gainReduction + makeupGain;
            
            std::cout << std::setw(12) << input.first 
                      << " (" << std::setw(5) << inputDb << " dB) -> "
                      << std::setw(4) << gainReduction << " dB -> "
                      << std::setw(5) << outputDb << " dB";
            
            if (gainReduction > 0) {
                std::cout << " ✓ COMPRESSED";
            }
            std::cout << std::endl;
        }
        std::cout << std::endl;
    }
    
    std::cout << "⚡ WHAT MAKES THESE SETTINGS EXTREME:" << std::endl;
    std::cout << std::endl;
    
    std::cout << "🎯 THRESHOLD -35 dB:" << std::endl;
    std::cout << "   • EXTREMELY low threshold" << std::endl;
    std::cout << "   • Even whisper-quiet sounds get compressed" << std::endl;
    std::cout << "   • Almost all audio content will trigger compression" << std::endl;
    std::cout << "   • Creates heavily processed, consistent sound" << std::endl;
    std::cout << std::endl;
    
    std::cout << "⚡ ATTACK 0 ms (INSTANT):" << std::endl;
    std::cout << "   • Zero attack time = instantaneous compression" << std::endl;
    std::cout << "   • Can create audible 'clicks' on transients" << std::endl;
    std::cout << "   • May cause pumping artifacts" << std::endl;
    std::cout << "   • Special anti-pop smoothing will activate in the code" << std::endl;
    std::cout << std::endl;
    
    std::cout << "💨 RELEASE 1 ms (ULTRA-FAST):" << std::endl;
    std::cout << "   • Extremely fast gain recovery" << std::endl;
    std::cout << "   • Creates rapid gain modulation" << std::endl;
    std::cout << "   • Can cause 'breathing' and distortion effects" << std::endl;
    std::cout << "   • Combined with 0ms attack = maximum instability" << std::endl;
    std::cout << std::endl;
    
    std::cout << "🔥 EXTREME SATURATION CONDITIONS:" << std::endl;
    std::cout << "   With ratios 8:1 and above, your settings trigger:" << std::endl;
    std::cout << "   • Aggressive harmonic distortion" << std::endl;
    std::cout << "   • Rich even and odd harmonics" << std::endl;
    std::cout << "   • Asymmetric saturation for character" << std::endl;
    std::cout << "   • Analog-style compression artifacts" << std::endl;
    std::cout << std::endl;
    
    std::cout << "🎵 EXPECTED SONIC CHARACTERISTICS:" << std::endl;
    std::cout << "   🔊 Heavily squashed dynamics" << std::endl;
    std::cout << "   🌊 Pumping and breathing artifacts" << std::endl;
    std::cout << "   🎸 Harmonic saturation and distortion" << std::endl;
    std::cout << "   📊 Very consistent output level" << std::endl;
    std::cout << "   📻 Vintage/lo-fi character" << std::endl;
    std::cout << "   ⚡ Aggressive, in-your-face sound" << std::endl;
    std::cout << std::endl;
    
    std::cout << "⚠️  USAGE WARNINGS:" << std::endl;
    std::cout << "   • These are VERY aggressive settings!" << std::endl;
    std::cout << "   • Will heavily color and distort your audio" << std::endl;
    std::cout << "   • May cause audible artifacts and pumping" << std::endl;
    std::cout << "   • Use with caution on important material" << std::endl;
    std::cout << "   • Perfect for creative/experimental effects" << std::endl;
    std::cout << "   • Great for vintage/lo-fi/grunge sounds" << std::endl;
    std::cout << std::endl;
    
    std::cout << "💡 PRACTICAL APPLICATIONS:" << std::endl;
    std::cout << "   🎤 Vocal effects (robotic, distorted)" << std::endl;
    std::cout << "   🥁 Drum crushing and saturation" << std::endl;
    std::cout << "   🎸 Guitar/bass distortion effects" << std::endl;
    std::cout << "   🎹 Synth/electronic music processing" << std::endl;
    std::cout << "   📻 Lo-fi/vintage aesthetic" << std::endl;
    std::cout << "   🎵 Creative sound design" << std::endl;
    
    return 0;
}
