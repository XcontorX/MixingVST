#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Params.h"
#include <atomic>
#include <cmath>

namespace mvst {

struct Biquad {
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    double z1[2] = {0, 0}, z2[2] = {0, 0};
    void reset() { z1[0] = z1[1] = z2[0] = z2[1] = 0; }
    inline float process(float x, int ch) {
        double y = b0 * x + z1[ch];
        z1[ch] = b1 * x - a1 * y + z2[ch];
        z2[ch] = b2 * x - a2 * y;
        return (float) y;
    }
    void set(double nb0, double nb1, double nb2, double a0, double na1, double na2) {
        b0 = nb0 / a0; b1 = nb1 / a0; b2 = nb2 / a0; a1 = na1 / a0; a2 = na2 / a0;
    }
    void highPass(double sr, double f, double q) {
        f = juce::jlimit(10.0, sr * 0.45, f);
        double w = 2 * juce::MathConstants<double>::pi * f / sr, c = std::cos(w), al = std::sin(w) / (2 * q);
        set((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al);
    }
    void lowPass(double sr, double f, double q) {
        f = juce::jlimit(10.0, sr * 0.45, f);
        double w = 2 * juce::MathConstants<double>::pi * f / sr, c = std::cos(w), al = std::sin(w) / (2 * q);
        set((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + al, -2 * c, 1 - al);
    }
    void peaking(double sr, double f, double q, double db) {
        f = juce::jlimit(10.0, sr * 0.45, f);
        double A = std::pow(10.0, db / 40), w = 2 * juce::MathConstants<double>::pi * f / sr, c = std::cos(w), al = std::sin(w) / (2 * q);
        set(1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
    }
    void lowShelf(double sr, double f, double db) {
        f = juce::jlimit(10.0, sr * 0.45, f);
        double A = std::pow(10.0, db / 40), w = 2 * juce::MathConstants<double>::pi * f / sr, c = std::cos(w), s = std::sin(w);
        double beta = 2 * std::sqrt(A) * (s / 2 * std::sqrt(2.0));
        set(A * ((A + 1) - (A - 1) * c + beta), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - beta),
            (A + 1) + (A - 1) * c + beta, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - beta);
    }
    void highShelf(double sr, double f, double db) {
        f = juce::jlimit(10.0, sr * 0.45, f);
        double A = std::pow(10.0, db / 40), w = 2 * juce::MathConstants<double>::pi * f / sr, c = std::cos(w), s = std::sin(w);
        double beta = 2 * std::sqrt(A) * (s / 2 * std::sqrt(2.0));
        set(A * ((A + 1) + (A - 1) * c + beta), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - beta),
            (A + 1) - (A - 1) * c + beta, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - beta);
    }
};

struct Comp {
    double gr = 0, aAtk = 0.01, aRel = 0.001;
    float thr = -18, ratio = 3, knee = 6, makeup = 0;
    void setup(double sr, float t, float r, float atkMs, float relMs) {
        thr = t; ratio = juce::jmax(1.0f, r);
        aAtk = 1.0 - std::exp(-1.0 / (0.001 * atkMs * sr));
        aRel = 1.0 - std::exp(-1.0 / (0.001 * relMs * sr));
        makeup = juce::jmax(0.0f, -thr * (1.0f - 1.0f / ratio) * 0.4f);
    }
    float process(float* x, int nc) {
        float lvl = 0;
        for (int c = 0; c < nc; ++c) lvl = juce::jmax(lvl, std::abs(x[c]));
        float db = 20.0f * std::log10(juce::jmax(lvl, 1.0e-6f));
        float over = db - thr, xk;
        if (2 * over < -knee) xk = 0;
        else if (2 * std::abs(over) <= knee) { float t = over + knee * 0.5f; xk = t * t / (2 * knee); }
        else xk = over;
        double tgt = xk * (1.0f - 1.0f / ratio);
        gr += (tgt > gr ? aAtk : aRel) * (tgt - gr);
        float g = std::pow(10.0f, (float) (-gr + makeup) / 20.0f);
        for (int c = 0; c < nc; ++c) x[c] *= g;
        return (float) gr;
    }
};

// Cambiador de tono por línea de retardo con dos cabezales y ventana Hann (para el Autotune)
struct PitchShifter {
    static constexpr int mask = 16383;
    std::vector<float> buf;
    int wp = 0, W = 1000, dryDelay = 502;
    double phase = 0;
    void prepare(double sr) {
        buf.assign(mask + 1, 0.0f); wp = 0; phase = 0;
        W = (int) (0.03 * sr); dryDelay = W / 2 + 2;
    }
    float process(float x, double ratio, float& dry) {
        buf[(size_t) wp] = x;
        phase += (1.0 - ratio) / W;
        phase -= std::floor(phase);
        float y = 0;
        for (int h = 0; h < 2; ++h) {
            double p = phase + 0.5 * h; if (p >= 1.0) p -= 1.0;
            double rp = wp - (p * W + 2.0);
            int i0 = (int) std::floor(rp); double fr = rp - i0;
            float s0 = buf[(size_t) (i0 & mask)], s1 = buf[(size_t) ((i0 + 1) & mask)];
            float s = s0 + (float) fr * (s1 - s0);
            float g = (float) std::sin(juce::MathConstants<double>::pi * p);
            y += s * g * g;
        }
        dry = buf[(size_t) ((wp - dryDelay) & mask)];
        wp = (wp + 1) & mask;
        return y;
    }
};

} // namespace mvst

class MixingVSTProcessor : public juce::AudioProcessor {
public:
    MixingVSTProcessor();
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "MixingVST"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 3.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    void applyPreset(int index);

    juce::AudioProcessorValueTreeState apvts;
    // Medidores para la interfaz
    std::atomic<float> inPk{0}, outPk{0}, gr1{0}, gr2{0}, dsGr{0}, gateG{1}, limGr{0}, pitchIn{-1}, pitchOut{-1};

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void analysePitch();
    static void atomicMax(std::atomic<float>& a, float v) { if (v > a.load()) a.store(v); }

    double sr = 44100.0;
    // filtros
    mvst::Biquad hp1, hp2, mud, box, dsHp, warm, pres, air, revHp, revLp;
    mvst::Comp comp1, comp2;
    mvst::PitchShifter ps[2];
    juce::Reverb reverb;
    juce::AudioBuffer<float> revBuf;
    juce::SmoothedValue<float> inSm, outSm;
    // gate / deesser / limiter
    float gEnv = 0, gGain = 1, dsEnv = 0, limG = 1;
    // autotune
    std::vector<float> anaBuf, yinX, yinD, yinCm;
    int anaWp = 0, hopCount = 0, unvoiced = 0;
    double logR = 0, logTarget = 0;
    int curKey = 0, curScale = 0;
    // delay
    std::vector<float> dly[2];
    int dlyWp = 0;
    float dlyLp[2] = {0, 0};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MixingVSTProcessor)
};
