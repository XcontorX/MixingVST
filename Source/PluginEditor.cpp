#include "PluginEditor.h"

using namespace ui;
using juce::Rectangle; using juce::Graphics; using juce::String;

static const float PI = juce::MathConstants<float>::pi;

// ===================================================== LookAndFeel
MixLNF::MixLNF() {
    setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff0d0618));
    setColour(juce::ComboBox::textColourId, text);
    setColour(juce::ComboBox::outlineColourId, purple);
    setColour(juce::ComboBox::arrowColourId, green);
    setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xff0d0618));
    setColour(juce::PopupMenu::textColourId, text);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, purpleDim);
    setColour(juce::PopupMenu::highlightedTextColourId, green);
    setColour(juce::TooltipWindow::backgroundColourId, juce::Colour(0xff0d0618));
    setColour(juce::TooltipWindow::textColourId, text);
    setColour(juce::TooltipWindow::outlineColourId, green);
}

void MixLNF::drawRotarySlider(Graphics& g, int x, int y, int w, int h, float pos, float start, float end, juce::Slider& s) {
    auto b = Rectangle<float>((float) x, (float) y, (float) w, (float) h).reduced(3.0f);
    float r = juce::jmin(b.getWidth(), b.getHeight()) / 2.0f;
    auto c = b.getCentre();
    float ang = start + pos * (end - start);
    bool hot = s.isMouseOverOrDragging();

    if (hot) { g.setColour(green.withAlpha(0.12f)); g.fillEllipse(c.x - r - 3, c.y - r - 3, 2 * r + 6, 2 * r + 6); }
    juce::Path track; track.addCentredArc(c.x, c.y, r - 2, r - 2, 0, start, end, true);
    g.setColour(juce::Colour(0xff2a1646)); g.strokePath(track, juce::PathStrokeType(3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    juce::Path val; val.addCentredArc(c.x, c.y, r - 2, r - 2, 0, start, ang, true);
    g.setColour(purple.withAlpha(0.25f)); g.strokePath(val, juce::PathStrokeType(7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setGradientFill(juce::ColourGradient(purple, c.x - r, c.y + r, green, c.x + r, c.y - r, false));
    g.strokePath(val, juce::PathStrokeType(3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    float br = r - 8;
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff3a1d63), c.x, c.y - br, juce::Colour(0xff0e0719), c.x, c.y + br, false));
    g.fillEllipse(c.x - br, c.y - br, 2 * br, 2 * br);
    g.setColour(hot ? green : purple.withAlpha(0.7f)); g.drawEllipse(c.x - br, c.y - br, 2 * br, 2 * br, 1.2f);
    float px = c.x + std::sin(ang) * (br - 2), py = c.y - std::cos(ang) * (br - 2);
    float qx = c.x + std::sin(ang) * (br * 0.35f), qy = c.y - std::cos(ang) * (br * 0.35f);
    g.setColour(green); g.drawLine(qx, qy, px, py, 2.4f);
}

void MixLNF::drawToggleButton(Graphics& g, juce::ToggleButton& b, bool, bool) {
    auto r = b.getLocalBounds().toFloat().reduced(2.0f);
    bool on = b.getToggleState();
    auto col = on ? green : purpleDim.brighter(0.4f);
    if (on) { g.setColour(green.withAlpha(0.18f)); g.fillEllipse(r.expanded(2)); }
    g.setColour(col); g.drawEllipse(r.reduced(1), 1.6f);
    auto c = r.getCentre(); float rad = r.getWidth() * 0.27f;
    juce::Path p; p.addCentredArc(c.x, c.y, rad, rad, 0, 0.45f, 2 * PI - 0.45f, true);
    p.startNewSubPath(c.x, c.y - rad * 1.2f); p.lineTo(c.x, c.y - 0.5f);
    g.strokePath(p, juce::PathStrokeType(1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

// ===================================================== Knob
Knob::Knob(juce::AudioProcessorValueTreeState& a, const mvst::FloatSpec& s)
    : spec(s), att(a, s.id, slider) {
    slider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters(PI * 1.25f, PI * 2.75f, true);
    slider.setDoubleClickReturnValue(true, s.def);
    slider.onValueChange = [this] { repaint(); };
    auto tip = mvst::U(s.name) + ": " + mvst::U(s.tip) + "  (doble clic = valor por defecto)";
    slider.setTooltip(tip); setTooltip(tip);
    addAndMakeVisible(slider);
}
void Knob::resized() { slider.setBounds(getLocalBounds().withTrimmedTop(13).withTrimmedBottom(14)); }
void Knob::paint(Graphics& g) {
    auto b = getLocalBounds();
    g.setColour(text.withAlpha(0.9f)); g.setFont(F(10.5f, true));
    g.drawFittedText(mvst::U(spec.name), b.removeFromTop(13), juce::Justification::centred, 1);
    g.setColour(greenSoft); g.setFont(F(10.5f));
    g.drawText(String(slider.getValue(), spec.decimals) + mvst::U(spec.unit), b.removeFromBottom(14), juce::Justification::centred);
}

// ===================================================== Card
static const mvst::FloatSpec* findSpec(const char* id) {
    for (auto& s : mvst::floatSpecs()) if (juce::String(s.id) == id) return &s;
    return nullptr;
}
static String toggleTip(const char* id) {
    for (auto& t : mvst::toggleSpecs()) if (juce::String(t.id) == id) return mvst::U(t.tip);
    return {};
}

Card::Card(juce::AudioProcessorValueTreeState& a, const String& num, const String& title, const char* toggleId,
           const std::vector<const char*>& knobIds, const String& tip)
    : numStr(num), titleStr(title) {
    for (auto id : knobIds) {
        if (auto* sp = findSpec(id)) { knobs.push_back(std::make_unique<Knob>(a, *sp)); addAndMakeVisible(*knobs.back()); }
    }
    power.setTooltip(toggleTip(toggleId));
    addAndMakeVisible(power);
    powerAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(a, toggleId, power);
    power.onStateChange = [this] { updateAlpha(); repaint(); };
    setTooltip(tip);
    updateAlpha();
}
void Card::updateAlpha() { for (auto& k : knobs) k->setAlpha(active() ? 1.0f : 0.4f); }

void Card::resized() {
    auto r = getLocalBounds();
    power.setBounds(r.getRight() - 28, 4, 22, 22);
    int n = (int) knobs.size(), per = (n == 3) ? 3 : 2, kw = getWidth() / per;
    for (int i = 0; i < n; ++i) {
        int row = i / per, col = i % per;
        int x = (n == 1) ? (getWidth() - kw) / 2 : col * kw;
        knobs[(size_t) i]->setBounds(x, 28 + row * 92, kw, 92);
    }
    int rows = (n + per - 1) / per, top = 28 + rows * 92 + 2;
    extraArea = Rectangle<int>(8, top, getWidth() - 16, getHeight() - top - 8);
    if (layoutExtra) layoutExtra(extraArea);
}

void Card::paint(Graphics& g) {
    auto b = getLocalBounds().toFloat().reduced(1.0f);
    bool on = active();
    float pulse = phase ? 0.5f + 0.5f * std::sin(*phase * 2.0f + (float) getX() * 0.01f) : 0.5f;
    g.setColour(panel.withAlpha(0.92f)); g.fillRoundedRectangle(b, 12.0f);
    g.setGradientFill(juce::ColourGradient(purple.withAlpha(on ? 0.5f + 0.3f * pulse : 0.2f), b.getX(), b.getY(),
                                           green.withAlpha(on ? 0.45f + 0.3f * pulse : 0.15f), b.getRight(), b.getBottom(), false));
    g.drawRoundedRectangle(b, 12.0f, 1.6f);
    g.setGradientFill(juce::ColourGradient(purple, 10, 6, green, 34, 22, false));
    g.setFont(F(12.0f, true)); g.drawText(numStr, 10, 5, 24, 18, juce::Justification::centredLeft);
    g.setColour(text); g.setFont(F(12.5f, true)); g.drawText(titleStr, 34, 5, getWidth() - 70, 18, juce::Justification::centredLeft);
    if (extraPaint && !extraArea.isEmpty()) {
        Graphics::ScopedSaveState ss(g);
        g.reduceClipRegion(extraArea);
        extraPaint(g, extraArea.toFloat());
        if (!on) { g.setColour(panel.withAlpha(0.65f)); g.fillRect(extraArea); }
    }
}

// ===================================================== helpers de dibujo
static float dbToNorm(float db, float lo, float hi) { return juce::jlimit(0.0f, 1.0f, (db - lo) / (hi - lo)); }

static void drawBar(Graphics& g, Rectangle<float> r, float v01, bool reverseColours = false) {
    g.setColour(juce::Colour(0xff0b0614)); g.fillRoundedRectangle(r, r.getHeight() / 2);
    auto f = r.withWidth(r.getWidth() * juce::jlimit(0.0f, 1.0f, v01));
    if (f.getWidth() > 1) {
        g.setGradientFill(juce::ColourGradient(reverseColours ? green : purple, r.getX(), 0, reverseColours ? purple : green, r.getRight(), 0, false));
        g.fillRoundedRectangle(f, r.getHeight() / 2);
    }
    g.setColour(purpleDim); g.drawRoundedRectangle(r, r.getHeight() / 2, 1.0f);
}

static void drawCurve(Graphics& g, Rectangle<float> r, const std::function<float(float)>& fn, float range) {
    g.setColour(juce::Colour(0x18ffffff));
    g.drawHorizontalLine((int) r.getCentreY(), r.getX(), r.getRight());
    for (float f : { 100.0f, 1000.0f, 10000.0f }) {
        float x = r.getX() + std::log10(f / 20.0f) / 3.0f * r.getWidth();
        g.drawVerticalLine((int) x, r.getY(), r.getBottom());
    }
    juce::Path p;
    for (int i = 0; i <= 100; ++i) {
        float t = i / 100.0f, f = 20.0f * std::pow(1000.0f, t);
        float db = juce::jlimit(-range, range, fn(f));
        float x = r.getX() + t * r.getWidth(), y = r.getCentreY() - db / range * r.getHeight() * 0.5f;
        if (i == 0) p.startNewSubPath(x, y); else p.lineTo(x, y);
    }
    g.setColour(purple.withAlpha(0.25f)); g.strokePath(p, juce::PathStrokeType(6.0f));
    g.setGradientFill(juce::ColourGradient(purple, r.getX(), 0, green, r.getRight(), 0, false));
    g.strokePath(p, juce::PathStrokeType(2.2f));
}

static void label(Graphics& g, const String& t, Rectangle<float> r, juce::Justification j = juce::Justification::centredLeft) {
    g.setColour(text.withAlpha(0.7f)); g.setFont(F(10.0f, true)); g.drawText(t, r, j);
}

// ===================================================== Editor
MixingVSTEditor::MixingVSTEditor(MixingVSTProcessor& p) : juce::AudioProcessorEditor(&p), proc(p) {
    setLookAndFeel(&lnf);
    auto& a = proc.apvts;
    auto mk = [&](int idx, const char* num, const char* title, const char* tog, std::vector<const char*> ks, const char* tip) {
        cards[idx] = std::make_unique<Card>(a, num, mvst::U(title), tog, ks, mvst::U(tip));
        cards[idx]->phase = &phase;
        addAndMakeVisible(*cards[idx]);
        return cards[idx].get();
    };

    // 01 Entrada + Puerta
    auto* c = mk(0, "01", "ENTRADA · PUERTA", "gateOn", {"inGain", "gateThr", "gateRel"},
        "Primer paso: ajusta el volumen de entrada y elimina el ruido de fondo y las respiraciones con la puerta de ruido.");
    c->extraPaint = [this](Graphics& g, Rectangle<float> r) {
        label(g, "NIVEL DE ENTRADA", r.removeFromTop(14));
        float thr = pv("gateThr");
        auto bar = r.removeFromTop(16).reduced(2, 1);
        drawBar(g, bar, dbToNorm(inDb, -80, 0));
        float tx = bar.getX() + bar.getWidth() * dbToNorm(thr, -80, 0);
        g.setColour(juce::Colours::white); g.drawVerticalLine((int) tx, bar.getY() - 2, bar.getBottom() + 2);
        r.removeFromTop(8);
        bool open = proc.gateG.load() > 0.5f;
        auto led = r.removeFromTop(20);
        g.setColour((open ? green : purple).withAlpha(0.3f)); g.fillEllipse(led.getX() + 4, led.getY() + 2, 16, 16);
        g.setColour(open ? green : purple); g.fillEllipse(led.getX() + 8, led.getY() + 6, 8, 8);
        g.setFont(F(11.0f, true)); g.drawText(open ? "PUERTA ABIERTA" : "PUERTA CERRADA", led.withTrimmedLeft(28), juce::Justification::centredLeft);
        label(g, mvst::U("Línea blanca = umbral"), r.removeFromTop(16));
    };

    // 02 EQ limpieza
    c = mk(1, "02", "EQ · LIMPIEZA", "eqOn", {"lowCut", "mud", "box"},
        "Quita lo que sobra de la voz: graves innecesarios, barro y sonido de caja. Una voz limpia suena más profesional.");
    c->extraPaint = [this](Graphics& g, Rectangle<float> r) {
        float fc = pv("lowCut"), m = pv("mud"), b = pv("box");
        drawCurve(g, r.reduced(2), [=](float f) {
            float hp = -10.0f * std::log10(1.0f + std::pow(fc / f, 8.0f));
            float lm = std::log2(f / 250.0f) / 0.7f, lb = std::log2(f / 500.0f) / 0.7f;
            return hp + m * std::exp(-lm * lm) + b * std::exp(-lb * lb);
        }, 12.0f);
    };

    // 03 De-esser
    c = mk(2, "03", "DE-ESSER", "dsOn", {"dsFreq", "dsAmt"},
        "Suaviza las eses (sss) y las t demasiado agudas para que la voz no pique en los oídos.");
    c->extraPaint = [this](Graphics& g, Rectangle<float> r) {
        label(g, "ESPECTRO · ZONA DE SIBILANCIA", r.removeFromTop(14));
        auto bars = r.removeFromTop(r.getHeight() - 22).reduced(2, 2);
        float fs = pv("dsFreq"), red = std::pow(10.0f, -dsD / 20.0f);
        const int N = 30; float bw = bars.getWidth() / N;
        for (int i = 0; i < N; ++i) {
            float f = 100.0f * std::pow(160.0f, i / (float) (N - 1));
            float h = (0.35f + 0.28f * std::sin(phase * 3.0f + i * 0.8f)) * (f > 3000 ? 1.25f : 1.0f);
            bool s = f >= fs; if (s) h *= red;
            float bh = juce::jlimit(0.03f, 1.0f, h) * bars.getHeight();
            g.setColour(s ? purple : green.withAlpha(0.8f));
            g.fillRoundedRectangle(bars.getX() + i * bw + 1, bars.getBottom() - bh, bw - 2, bh, 2.0f);
        }
        float mx = bars.getX() + std::log10(fs / 100.0f) / std::log10(160.0f) * bars.getWidth();
        g.setColour(juce::Colours::white.withAlpha(0.8f)); g.drawVerticalLine((int) mx, bars.getY(), bars.getBottom());
        label(g, "REDUCCION", r.removeFromTop(11));
        drawBar(g, r.reduced(2, 1), dsD / 12.0f);
    };

    // 04 Autotune
    c = mk(3, "04", "AUTOTUNE", "atOn", {"atAmt", "atSpd"},
        "Corrige la afinación hacia las notas de la escala elegida. Elige la tonalidad de tu canción y su escala (Mayor o Menor).");
    keyBox.addItemList(mvst::keyNames(), 1); scaleBox.addItemList(mvst::scaleNames(), 1);
    keyBox.setTooltip(mvst::U("Tonalidad: la nota base de tu canción (ej: si está en La menor, elige A y escala Menor). Si no la sabes, usa Cromática: afina a cualquier nota."));
    scaleBox.setTooltip(mvst::U("Escala: el conjunto de notas a las que se irá corrigiendo la voz. Mayor = alegre, Menor = triste/oscuro, Cromática = cualquier nota."));
    keyAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(a, "atKey", keyBox);
    scaleAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(a, "atScale", scaleBox);
    c->addAndMakeVisible(keyBox); c->addAndMakeVisible(scaleBox);
    c->layoutExtra = [this](Rectangle<int> r) {
        keyBox.setBounds(r.getX(), r.getY(), r.getWidth() / 2 - 8, 22);
        scaleBox.setBounds(r.getX() + r.getWidth() / 2 - 4, r.getY(), r.getWidth() / 2 + 4, 22);
    };
    c->extraPaint = [this](Graphics& g, Rectangle<float> r) {
        r.removeFromTop(28);
        label(g, "NOTAS PERMITIDAS · VOZ", r.removeFromTop(13));
        auto mask = mvst::scaleMask((int) pv("atScale")); int key = (int) pv("atKey");
        static const char* nn[12] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
        auto strip = r.removeFromTop(34).reduced(1, 0); float cw = strip.getWidth() / 12.0f;
        for (int pc = 0; pc < 12; ++pc) {
            bool ok = mask[(size_t) ((((pc - key) % 12) + 12) % 12)];
            auto cell = Rectangle<float>(strip.getX() + pc * cw + 1, strip.getY(), cw - 2, strip.getHeight());
            g.setColour(ok ? green.withAlpha(0.28f) : juce::Colour(0xff0d0618)); g.fillRoundedRectangle(cell, 3.0f);
            g.setColour(ok ? green : purpleDim); g.drawRoundedRectangle(cell, 3.0f, 1.0f);
            g.setFont(F(8.5f)); g.setColour(ok ? text : text.withAlpha(0.3f)); g.drawText(nn[pc], cell.removeFromBottom(11), juce::Justification::centred);
        }
        if (pitchInS >= 0) {
            float pf = std::fmod(pitchInS, 12.0f), pt = std::fmod(pitchOutS, 12.0f);
            float x1 = strip.getX() + (pf + 0.5f) * cw, x2 = strip.getX() + (pt + 0.5f) * cw;
            g.setColour(purple); g.fillEllipse(x1 - 5, strip.getY() + 6, 10, 10);
            g.setColour(green.withAlpha(0.35f)); g.fillEllipse(x2 - 8, strip.getY() + 3, 16, 16);
            g.setColour(green); g.fillEllipse(x2 - 4, strip.getY() + 7, 8, 8);
        }
        r.removeFromTop(4);
        g.setFont(F(9.5f)); g.setColour(purple); g.drawText(mvst::U("● tu voz"), r.removeFromLeft(r.getWidth() / 2), juce::Justification::centred);
        g.setColour(green); g.drawText(mvst::U("● nota corregida"), r, juce::Justification::centred);
    };

    // 05 Compresor 1
    c = mk(4, "05", "COMPRESOR 1 · NIVELA", "c1On", {"c1Thr", "c1Rat", "c1Atk", "c1Rel"},
        "Compresor suave que iguala el volumen: las partes bajas suben y las altas bajan para que la voz suene constante.");
    c->extraPaint = [this](Graphics& g, Rectangle<float> r) {
        label(g, mvst::U("REDUCCIÓN DE GANANCIA"), r.removeFromTop(12));
        drawBar(g, r.reduced(2, 2), gr1D / 12.0f);
    };

    // 06 Compresor 2
    c = mk(5, "06", "COMPRESOR 2 · CONTROL", "c2On", {"c2Thr", "c2Rat"},
        "Segundo compresor rápido y firme. Controla los picos para una voz estable, potente y comercial.");
    c->extraPaint = [this](Graphics& g, Rectangle<float> r) {
        auto bar = r.removeFromBottom(26);
        label(g, mvst::U("REDUCCIÓN DE GANANCIA"), bar.removeFromTop(12));
        drawBar(g, bar.reduced(2, 2), gr2D / 12.0f);
        auto box = r.reduced(r.getWidth() * 0.5f - juce::jmin(r.getHeight(), 70.0f) * 0.5f, 2);
        g.setColour(juce::Colour(0xff0b0614)); g.fillRoundedRectangle(box, 6.0f);
        g.setColour(purpleDim); g.drawRoundedRectangle(box, 6.0f, 1.0f);
        float thr = pv("c2Thr"), rat = pv("c2Rat");
        juce::Path p;
        for (int i = 0; i <= 60; ++i) {
            float in = -60.0f + i, out = in < thr ? in : thr + (in - thr) / rat;
            float x = box.getX() + (in + 60.0f) / 60.0f * box.getWidth(), y = box.getBottom() - (out + 60.0f) / 60.0f * box.getHeight();
            if (i == 0) p.startNewSubPath(x, y); else p.lineTo(x, y);
        }
        g.setGradientFill(juce::ColourGradient(purple, box.getX(), box.getBottom(), green, box.getRight(), box.getY(), false));
        g.strokePath(p, juce::PathStrokeType(2.2f));
        float lx = box.getX() + dbToNorm(inDb, -60, 0) * box.getWidth();
        g.setColour(juce::Colours::white); g.fillEllipse(lx - 3, box.getBottom() - 3 - 0.0f - (inDb < thr ? dbToNorm(inDb, -60, 0) : dbToNorm(thr + (inDb - thr) / rat, -60, 0)) * box.getHeight(), 6, 6);
    };

    // 07 Saturación
    c = mk(6, "07", "SATURACIÓN", "satOn", {"satDrv"},
        "Añade armónicos suaves (como un equipo analógico de estudio) para dar cuerpo, calidez y carácter a la voz.");
    c->extraPaint = [this](Graphics& g, Rectangle<float> r) {
        label(g, mvst::U("ONDA · ENTRADA vs SATURADA"), r.removeFromTop(13));
        r = r.reduced(2, 2);
        float sd = pv("satDrv") / 100.0f, pre = 1.0f + sd * 6.0f, mix = juce::jmin(1.0f, sd * 1.5f);
        juce::Path dry, wet;
        for (int i = 0; i <= 120; ++i) {
            float t = i / 120.0f, s = 0.95f * std::sin(2 * PI * (t * 1.5f + phase * 0.15f));
            float w = s + (std::tanh(pre * s) / pre * (1.0f + sd * 1.8f) - s) * mix;
            float x = r.getX() + t * r.getWidth();
            float yd = r.getCentreY() - s * r.getHeight() * 0.45f, yw = r.getCentreY() - juce::jlimit(-1.0f, 1.0f, w) * r.getHeight() * 0.45f;
            if (i == 0) { dry.startNewSubPath(x, yd); wet.startNewSubPath(x, yw); } else { dry.lineTo(x, yd); wet.lineTo(x, yw); }
        }
        g.setColour(purple.withAlpha(0.8f)); g.strokePath(dry, juce::PathStrokeType(1.6f));
        g.setColour(green.withAlpha(0.3f)); g.strokePath(wet, juce::PathStrokeType(6.0f));
        g.setColour(green); g.strokePath(wet, juce::PathStrokeType(2.0f));
    };

    // 08 EQ tono
    c = mk(7, "08", "EQ · COLOR Y BRILLO", "tonOn", {"warm", "pres", "air"},
        "Da color final a la voz: más calidez, más presencia para destacar sobre el beat y aire brillante de estudio.");
    c->extraPaint = [this](Graphics& g, Rectangle<float> r) {
        float w = pv("warm"), p = pv("pres"), a = pv("air");
        drawCurve(g, r.reduced(2), [=](float f) {
            float lp = std::log2(f / 3500.0f);
            return w / (1.0f + std::pow(f / 130.0f, 2.0f)) + p * std::exp(-lp * lp) + a / (1.0f + std::pow(9000.0f / f, 2.5f));
        }, 10.0f);
    };

    // 09 Reverb
    c = mk(8, "09", "REVERB", "revOn", {"revMix", "revSize", "revDamp"},
        "Añade espacio y profundidad a la voz, como si cantara en una sala. Se mezcla en paralelo, sin perder claridad.");
    c->extraPaint = [this](Graphics& g, Rectangle<float> r) {
        float mix = pv("revMix") / 60.0f, size = pv("revSize") / 100.0f;
        auto ctr = r.getCentre(); float maxR = juce::jmin(r.getWidth(), r.getHeight()) * 0.5f * (0.45f + 0.55f * size);
        for (int k = 0; k < 4; ++k) {
            float t = std::fmod(phase * 0.25f + k * 0.25f, 1.0f), rad = 6.0f + t * maxR;
            float al = (1.0f - t) * (0.25f + 0.75f * mix);
            g.setColour((k % 2 ? green : purple).withAlpha(al)); g.drawEllipse(ctr.x - rad * 1.5f, ctr.y - rad, rad * 3.0f, rad * 2.0f, 2.0f);
        }
        g.setColour(green); g.fillEllipse(ctr.x - 3, ctr.y - 3, 6, 6);
    };

    // 10 Delay
    c = mk(9, "10", "DELAY", "dlyOn", {"dlyMix", "dlyFb"},
        "Ecos sincronizados con el tempo de tu proyecto (rebotan de izquierda a derecha). Da amplitud y profundidad a las frases.");
    divBox.addItemList(mvst::divNames(), 1);
    divBox.setTooltip(mvst::U("Tiempo del eco respecto al tempo de la canción. 1/4 = un golpe; 1/8 = medio golpe; 1/8 con punto = efecto clásico de voces pop; 1/16 = rápido."));
    divAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(a, "dlyDiv", divBox);
    c->addAndMakeVisible(divBox);
    c->layoutExtra = [this](Rectangle<int> r) { divBox.setBounds(r.getX(), r.getY(), r.getWidth(), 22); };
    c->extraPaint = [this](Graphics& g, Rectangle<float> r) {
        r.removeFromTop(30);
        float fb = pv("dlyFb") / 100.0f, mix = pv("dlyMix") / 60.0f;
        int active = (int) std::fmod(phase * 1.6f, 6.0f);
        float cw = r.getWidth() / 6.0f;
        for (int k = 0; k < 6; ++k) {
            float amp = k == 0 ? 1.0f : std::pow(fb, (float) k) * (0.3f + 0.7f * mix);
            float rad = 3.0f + 13.0f * amp, x = r.getX() + (k + 0.5f) * cw, y = r.getCentreY() + (k % 2 ? 8.0f : -8.0f);
            bool hit = k == active;
            g.setColour((k % 2 ? green : purple).withAlpha(hit ? 0.5f : 0.2f)); g.fillEllipse(x - rad - 3, y - rad - 3, 2 * rad + 6, 2 * rad + 6);
            g.setColour(k % 2 ? green : purple); g.fillEllipse(x - rad, y - rad, 2 * rad, 2 * rad);
        }
    };

    // Salida
    outKnob = std::make_unique<Knob>(a, *findSpec("outGain")); ceilKnob = std::make_unique<Knob>(a, *findSpec("ceil"));
    addAndMakeVisible(*outKnob); addAndMakeVisible(*ceilKnob);

    // Presets
    presetBox.setTextWhenNothingSelected(mvst::U("Elige un preset..."));
    for (int i = 0; i < (int) mvst::presets().size(); ++i) presetBox.addItem(mvst::U(mvst::presets()[(size_t) i].name), i + 1);
    presetBox.setTooltip(mvst::U("Presets: configuraciones listas para cada estilo (pop, trap, R&B, rock, podcast). Elige uno y luego afina con las ruedas."));
    presetBox.onChange = [this] { int i = presetBox.getSelectedItemIndex(); if (i >= 0) proc.applyPreset(i); };
    addAndMakeVisible(presetBox);

    juce::Random rnd;
    for (int i = 0; i < 40; ++i)
        particles.push_back({ rnd.nextFloat() * 1080.0f, rnd.nextFloat() * 716.0f, 0.15f + rnd.nextFloat() * 0.5f, 1.5f + rnd.nextFloat() * 3.5f, rnd.nextFloat() * 6.28f, rnd.nextBool() });

    setSize(1080, 716);
    startTimerHz(30);
}

MixingVSTEditor::~MixingVSTEditor() { stopTimer(); setLookAndFeel(nullptr); }

void MixingVSTEditor::resized() {
    for (int i = 0; i < 10; ++i)
        cards[i]->setBounds(14 + (i % 5) * 212, 82 + (i / 5) * 258, 204, 250);
    presetBox.setBounds(getWidth() - 14 - 210, 40, 210, 26);
    outKnob->setBounds(getWidth() - 14 - 210, 598, 102, 92);
    ceilKnob->setBounds(getWidth() - 14 - 108, 598, 102, 92);
}

void MixingVSTEditor::timerCallback() {
    phase += 0.06f;
    auto toDb = [](float lin) { return 20.0f * std::log10(juce::jmax(lin, 1.0e-4f)); };
    inDb = juce::jmax(toDb(proc.inPk.exchange(0.0f)), inDb - 2.5f);
    outDb = juce::jmax(toDb(proc.outPk.exchange(0.0f)), outDb - 2.5f);
    gr1D = juce::jmax(proc.gr1.exchange(0.0f), gr1D - 0.8f);
    gr2D = juce::jmax(proc.gr2.exchange(0.0f), gr2D - 0.8f);
    dsD = juce::jmax(proc.dsGr.exchange(0.0f), dsD - 0.8f);
    limD = juce::jmax(proc.limGr.exchange(0.0f), limD - 0.5f);
    float pi_ = proc.pitchIn.load(), po = proc.pitchOut.load();
    if (pi_ < 0) { pitchInS = pitchOutS = -1; } else { pitchInS = pitchInS < 0 ? pi_ : pitchInS + 0.5f * (pi_ - pitchInS); pitchOutS = po; }
    for (auto& p : particles) {
        p.y -= p.v; p.x += std::sin(phase + p.ph) * 0.3f;
        if (p.y < -10) { p.y = (float) getHeight() + 10; p.x = juce::Random::getSystemRandom().nextFloat() * (float) getWidth(); }
    }
    repaint();
}

void MixingVSTEditor::paint(Graphics& g) {
    g.setGradientFill(juce::ColourGradient(bg1, 0, 0, bg2, 0, (float) getHeight(), false)); g.fillAll();
    // luna
    g.setGradientFill(juce::ColourGradient(purple.withAlpha(0.22f), (float) getWidth() - 120, 40, juce::Colours::transparentBlack, (float) getWidth() - 120, 200, true));
    g.fillEllipse((float) getWidth() - 260, -80, 280, 280);
    for (auto& p : particles) {
        auto col = p.green ? green : purple;
        g.setColour(col.withAlpha(0.08f)); g.fillEllipse(p.x - p.s * 2, p.y - p.s * 2, p.s * 4, p.s * 4);
        g.setColour(col.withAlpha(0.45f)); g.fillEllipse(p.x - p.s * 0.5f, p.y - p.s * 0.5f, p.s, p.s);
    }
    // cabecera
    float glow = 0.5f + 0.5f * std::sin(phase * 1.5f);
    g.setColour(purple.withAlpha(0.15f + 0.1f * glow)); g.setFont(F(38.0f, true)); g.drawText("MixingVST", 16, 4, 320, 44, juce::Justification::centredLeft);
    g.setGradientFill(juce::ColourGradient(purple.brighter(0.3f), 14, 8, green, 250, 44, false));
    g.setFont(F(36.0f, true)); g.drawText("MixingVST", 14, 4, 320, 44, juce::Justification::centredLeft);
    g.setColour(green.withAlpha(0.15f + 0.2f * glow)); g.fillRoundedRectangle(16, 50, 118, 22, 11.0f);
    g.setColour(green); g.drawRoundedRectangle(16, 50, 118, 22, 11.0f, 1.2f);
    g.setFont(F(13.0f, true)); g.drawText("by ikerjb_", 16, 50, 118, 22, juce::Justification::centred);
    g.setColour(text.withAlpha(0.6f)); g.setFont(F(11.5f));
    g.drawFittedText(mvst::U("Cadena de voz profesional todo en 1  ·  Sigue el orden 01 → 10  ·  Pasa el ratón sobre cualquier control para ver qué hace  ·  Doble clic en una rueda = valor por defecto"),
                     160, 30, getWidth() - 160 - 250, 40, juce::Justification::centredLeft, 2);
    label(g, "PRESET", Rectangle<float>((float) getWidth() - 224, 22, 210, 16), juce::Justification::centredLeft);

    // pie: medidores y visualizador
    auto foot = Rectangle<float>(14, 598, (float) getWidth() - 28, 92);
    g.setColour(panel.withAlpha(0.92f)); g.fillRoundedRectangle(foot, 12.0f);
    g.setGradientFill(juce::ColourGradient(purple.withAlpha(0.5f), foot.getX(), foot.getY(), green.withAlpha(0.45f), foot.getRight(), foot.getBottom(), false));
    g.drawRoundedRectangle(foot, 12.0f, 1.6f);
    label(g, mvst::U("ENTRADA"), Rectangle<float>(30, 608, 120, 14));
    drawBar(g, Rectangle<float>(30, 624, 300, 14), dbToNorm(inDb, -60, 0));
    label(g, "SALIDA", Rectangle<float>(30, 644, 120, 14));
    drawBar(g, Rectangle<float>(30, 660, 300, 14), dbToNorm(outDb, -60, 0), true);
    label(g, String::fromUTF8("LIMITADOR  -") + String(limD, 1) + " dB", Rectangle<float>(350, 608, 200, 14));
    drawBar(g, Rectangle<float>(350, 624, 200, 14), limD / 6.0f);
    g.setColour(text.withAlpha(0.55f)); g.setFont(F(10.5f));
    g.drawText(mvst::U("Objetivo: entrada ≈ -12 dB · salida sin saturar el limitador"), Rectangle<float>(350, 644, 320, 14), juce::Justification::centredLeft);
    g.drawText(mvst::U("Dueño: ikerjb_  ·  Listo para Spotify / YouTube"), Rectangle<float>(350, 660, 320, 14), juce::Justification::centredLeft);
    // visualizador
    auto vis = Rectangle<float>(690, 606, 190, 76);
    float lvl = dbToNorm(outDb, -60, 0);
    for (int i = 0; i < 24; ++i) {
        float h = (0.08f + 0.92f * lvl * (0.35f + 0.65f * std::abs(std::sin(phase * 2.2f + i * 0.55f)))) * vis.getHeight();
        float bw = vis.getWidth() / 24.0f;
        g.setGradientFill(juce::ColourGradient(green, 0, vis.getY(), purple, 0, vis.getBottom(), false));
        g.fillRoundedRectangle(vis.getX() + i * bw + 1, vis.getBottom() - h, bw - 2, h, 2.0f);
    }
}
