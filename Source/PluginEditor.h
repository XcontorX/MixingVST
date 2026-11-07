#pragma once
#include "PluginProcessor.h"
#include <functional>
#include <memory>

namespace ui {
const juce::Colour bg1(0xff08040f), bg2(0xff1a0a2e), panel(0xff130a21), purple(0xffa23bff), purpleDim(0xff4b1f82),
                   green(0xff39ff14), greenSoft(0xff9bff7a), text(0xffeadcff);
inline juce::Font F(float h, bool bold = false) { return juce::Font(juce::FontOptions(h, bold ? juce::Font::bold : juce::Font::plain)); }
}

class MixLNF : public juce::LookAndFeel_V4 {
public:
    MixLNF();
    void drawRotarySlider(juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
    void drawToggleButton(juce::Graphics&, juce::ToggleButton&, bool, bool) override;
};

class Knob : public juce::Component, public juce::SettableTooltipClient {
public:
    Knob(juce::AudioProcessorValueTreeState& apvts, const mvst::FloatSpec& spec);
    void resized() override;
    void paint(juce::Graphics&) override;
    const mvst::FloatSpec& spec;
    juce::Slider slider;
    juce::AudioProcessorValueTreeState::SliderAttachment att;
};

class Card : public juce::Component, public juce::SettableTooltipClient {
public:
    Card(juce::AudioProcessorValueTreeState& apvts, const juce::String& num, const juce::String& title,
         const char* toggleId, const std::vector<const char*>& knobIds, const juce::String& tip);
    void resized() override;
    void paint(juce::Graphics&) override;
    void updateAlpha();
    bool active() const { return power.getToggleState(); }

    const float* phase = nullptr;
    std::function<void(juce::Graphics&, juce::Rectangle<float>)> extraPaint;
    std::function<void(juce::Rectangle<int>)> layoutExtra;
private:
    juce::String numStr, titleStr;
    juce::ToggleButton power;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> powerAtt;
    std::vector<std::unique_ptr<Knob>> knobs;
    juce::Rectangle<int> extraArea;
};

class MixingVSTEditor : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit MixingVSTEditor(MixingVSTProcessor&);
    ~MixingVSTEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    void timerCallback() override;
    float pv(const char* id) const { return proc.apvts.getRawParameterValue(id)->load(); }

    MixingVSTProcessor& proc;
    MixLNF lnf;
    juce::TooltipWindow tooltipWindow { this, 350 };
    float phase = 0;

    std::unique_ptr<Card> cards[10];
    juce::ComboBox presetBox, keyBox, scaleBox, divBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> keyAtt, scaleAtt, divAtt;
    std::unique_ptr<Knob> outKnob, ceilKnob;

    struct Particle { float x, y, v, s, ph; bool green; };
    std::vector<Particle> particles;
    // valores suavizados para medidores
    float inDb = -80, outDb = -80, gr1D = 0, gr2D = 0, dsD = 0, limD = 0, pitchInS = -1, pitchOutS = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MixingVSTEditor)
};
