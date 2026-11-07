#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace mvst;

static inline float dbToGain(float db) { return std::pow(10.0f, db / 20.0f); }

juce::AudioProcessorValueTreeState::ParameterLayout MixingVSTProcessor::createLayout() {
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    for (auto& s : floatSpecs()) {
        juce::NormalisableRange<float> r(s.mn, s.mx);
        if (s.centre > 0) r.setSkewForCentre(s.centre);
        l.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{s.id, 1}, U(s.name), r, s.def));
    }
    for (auto& t : toggleSpecs())
        l.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{t.id, 1}, t.id, t.def));
    l.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"atKey", 1}, "Tonalidad", keyNames(), 0));
    l.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"atScale", 1}, "Escala", scaleNames(), 0));
    l.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"dlyDiv", 1}, "Tiempo", divNames(), 1));
    return l;
}

MixingVSTProcessor::MixingVSTProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                      .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "MixingVST", createLayout()) {
    anaBuf.assign(4096, 0.f); yinX.assign(4096, 0.f); yinD.assign(4096, 0.f); yinCm.assign(4096, 0.f);
}

bool MixingVSTProcessor::isBusesLayoutSupported(const BusesLayout& l) const {
    auto o = l.getMainOutputChannelSet();
    return (o == juce::AudioChannelSet::mono() || o == juce::AudioChannelSet::stereo()) && o == l.getMainInputChannelSet();
}

void MixingVSTProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    sr = sampleRate;
    for (auto* b : { &hp1, &hp2, &mud, &box, &dsHp, &warm, &pres, &air, &revHp, &revLp }) b->reset();
    revHp.highPass(sr, 300, 0.707); revLp.lowPass(sr, 8500, 0.707);
    ps[0].prepare(sr); ps[1].prepare(sr);
    setLatencySamples(ps[0].dryDelay);
    reverb.setSampleRate(sr); reverb.reset();
    revBuf.setSize(2, juce::jmax(1, samplesPerBlock));
    for (auto& d : dly) d.assign((size_t) (sr * 2.6) + 8, 0.f);
    dlyWp = 0; dlyLp[0] = dlyLp[1] = 0;
    inSm.reset(sr, 0.05); outSm.reset(sr, 0.05);
    inSm.setCurrentAndTargetValue(1.0f); outSm.setCurrentAndTargetValue(1.0f);
    std::fill(anaBuf.begin(), anaBuf.end(), 0.f);
    gEnv = 0; gGain = 1; dsEnv = 0; limG = 1; logR = logTarget = 0; hopCount = 0; unvoiced = 0;
    comp1.gr = comp2.gr = 0;
}

void MixingVSTProcessor::analysePitch() {
    const int Wn = 1024;
    const int tauMax = juce::jmin((int) (sr / 70.0), 4096 - Wn - 2);
    const int tauMin = juce::jmax(2, (int) (sr / 900.0));
    const int M = Wn + tauMax;
    for (int i = 0; i < M; ++i) yinX[(size_t) i] = anaBuf[(size_t) ((anaWp - M + i) & 4095)];
    double energy = 0;
    for (int i = 0; i < Wn; ++i) energy += yinX[(size_t) i] * yinX[(size_t) i];
    energy /= Wn;
    bool voiced = false; float f0 = 0;
    if (energy > 1.0e-5) {
        for (int tau = 1; tau <= tauMax; ++tau) {
            float s = 0;
            for (int j = 0; j < Wn; ++j) { float d = yinX[(size_t) j] - yinX[(size_t) (j + tau)]; s += d * d; }
            yinD[(size_t) tau] = s;
        }
        yinCm[0] = 1; float run = 0;
        for (int tau = 1; tau <= tauMax; ++tau) {
            run += yinD[(size_t) tau];
            yinCm[(size_t) tau] = run > 0 ? yinD[(size_t) tau] * tau / run : 1.0f;
        }
        for (int tau = tauMin; tau < tauMax; ++tau) {
            if (yinCm[(size_t) tau] < 0.15f) {
                while (tau + 1 < tauMax && yinCm[(size_t) (tau + 1)] < yinCm[(size_t) tau]) ++tau;
                float s0 = yinCm[(size_t) (tau - 1)], s1 = yinCm[(size_t) tau], s2 = yinCm[(size_t) (tau + 1)];
                float den = 2.0f * (s0 - 2.0f * s1 + s2);
                float tf = tau + (std::abs(den) > 1.0e-9f ? (s0 - s2) / den : 0.0f);
                f0 = (float) (sr / tf); voiced = f0 > 60.0f && f0 < 1000.0f;
                break;
            }
        }
    }
    if (voiced) {
        unvoiced = 0;
        float midi = 69.0f + 12.0f * std::log2(f0 / 440.0f);
        auto mask = scaleMask(curScale);
        int base = (int) std::floor(midi + 0.5f), bn = base; float best = 1.0e9f;
        for (int k = -7; k <= 7; ++k) {
            int nn = base + k, pc = (((nn - curKey) % 12) + 12) % 12;
            if (!mask[(size_t) pc]) continue;
            float dist = std::abs(nn - midi);
            if (dist < best) { best = dist; bn = nn; }
        }
        logTarget = (bn - midi) / 12.0;
        pitchIn.store(midi); pitchOut.store((float) bn);
    } else if (++unvoiced > 4) {
        logTarget = 0; pitchIn.store(-1.0f); pitchOut.store(-1.0f);
    }
}

void MixingVSTProcessor::processBlock(juce::AudioBuffer<float>& buf, juce::MidiBuffer&) {
    juce::ScopedNoDenormals noDenormals;
    const int nCh = juce::jmin(buf.getNumChannels(), 2);
    const int n = buf.getNumSamples();
    for (int c = nCh; c < buf.getNumChannels(); ++c) buf.clear(c, 0, n);
    if (nCh == 0 || n == 0) return;

    auto v = [this](const char* id) { return apvts.getRawParameterValue(id)->load(); };
    auto on = [&](const char* id) { return v(id) > 0.5f; };

    const bool gateOn = on("gateOn"), eqOn = on("eqOn"), dsOn = on("dsOn"), atOn = on("atOn"), c1On = on("c1On"),
               c2On = on("c2On"), satOn = on("satOn"), tonOn = on("tonOn"), revOn = on("revOn"), dlyOn = on("dlyOn");
    inSm.setTargetValue(dbToGain(v("inGain")));
    outSm.setTargetValue(dbToGain(v("outGain")));

    const float gateThr = dbToGain(v("gateThr"));
    const float gateFloor = dbToGain(-30.0f);
    const float gAtkEnv = 1.0f - std::exp(-1.0f / (0.001f * (float) sr));
    const float gRelEnv = 1.0f - std::exp(-1.0f / (0.05f * (float) sr));
    const float gOpen = 1.0f - std::exp(-1.0f / (0.003f * (float) sr));
    const float gClose = 1.0f - std::exp(-1.0f / (0.001f * v("gateRel") * (float) sr));

    hp1.highPass(sr, v("lowCut"), 0.707); hp2.highPass(sr, v("lowCut"), 0.707);
    mud.peaking(sr, 250, 1.0, v("mud")); box.peaking(sr, 500, 1.2, v("box"));

    dsHp.highPass(sr, v("dsFreq"), 0.7);
    const float dsThr = -20.0f - v("dsAmt") * 0.25f;
    const float dsAtk = 1.0f - std::exp(-1.0f / (0.0005f * (float) sr));
    const float dsRel = 1.0f - std::exp(-1.0f / (0.03f * (float) sr));

    curKey = (int) v("atKey"); curScale = (int) v("atScale");
    const float atAmt = v("atAmt") / 100.0f;
    const double atTauMs = 2.0 * std::pow(150.0, v("atSpd") / 100.0);
    const double atCoef = 1.0 - std::exp(-1.0 / (atTauMs * 0.001 * sr));
    const int hop = juce::jmax(512, (int) (512.0 * sr / 48000.0));

    comp1.setup(sr, v("c1Thr"), v("c1Rat"), v("c1Atk"), v("c1Rel"));
    comp2.setup(sr, v("c2Thr"), v("c2Rat"), 3.0f, 80.0f);

    const float sd = v("satDrv") / 100.0f, satPre = 1.0f + sd * 6.0f, satMix = juce::jmin(1.0f, sd * 1.5f);

    warm.lowShelf(sr, 120, v("warm")); pres.peaking(sr, 3500, 0.8, v("pres")); air.highShelf(sr, 12000, v("air"));

    float* d[2] = { buf.getWritePointer(0), nCh > 1 ? buf.getWritePointer(1) : nullptr };
    float pkIn = 0, pkG1 = 0, pkG2 = 0, pkDs = 0;

    // ---------- Pasada 1: puerta > EQ > de-esser > autotune > comp > saturación > EQ tono ----------
    for (int i = 0; i < n; ++i) {
        float x[2] = {0, 0};
        const float ig = inSm.getNextValue();
        float lvl = 0;
        for (int c = 0; c < nCh; ++c) { x[c] = d[c][i] * ig; lvl = juce::jmax(lvl, std::abs(x[c])); }
        pkIn = juce::jmax(pkIn, lvl);

        if (gateOn) {
            gEnv += (lvl > gEnv ? gAtkEnv : gRelEnv) * (lvl - gEnv);
            float tgt = gEnv > gateThr ? 1.0f : gateFloor;
            gGain += (tgt > gGain ? gOpen : gClose) * (tgt - gGain);
            for (int c = 0; c < nCh; ++c) x[c] *= gGain;
        } else gGain = 1.0f;

        if (eqOn)
            for (int c = 0; c < nCh; ++c) {
                float s = hp2.process(hp1.process(x[c], c), c);
                x[c] = box.process(mud.process(s, c), c);
            }

        if (dsOn) {
            float hi[2] = {0, 0}, hl = 0;
            for (int c = 0; c < nCh; ++c) { hi[c] = dsHp.process(x[c], c); hl = juce::jmax(hl, std::abs(hi[c])); }
            dsEnv += (hl > dsEnv ? dsAtk : dsRel) * (hl - dsEnv);
            float envDb = 20.0f * std::log10(juce::jmax(dsEnv, 1.0e-6f));
            float over = envDb - dsThr;
            float grDb = over > 0 ? juce::jmin(over * 0.75f, 12.0f) : 0.0f;
            float g = dbToGain(-grDb);
            for (int c = 0; c < nCh; ++c) x[c] -= hi[c] * (1.0f - g);
            pkDs = juce::jmax(pkDs, grDb);
        }

        // Autotune (siempre con la misma latencia, esté activo o no)
        {
            float mono = nCh > 1 ? 0.5f * (x[0] + x[1]) : x[0];
            anaBuf[(size_t) (anaWp & 4095)] = mono; ++anaWp;
            if (atOn && ++hopCount >= hop) { hopCount = 0; analysePitch(); }
            if (!atOn) logTarget = 0;
            logR += atCoef * (logTarget - logR);
            double eff = logR * atAmt;
            double ratio = std::exp2(eff);
            float blend = juce::jlimit(0.0f, 1.0f, (float) ((std::abs(eff) * 1200.0 - 4.0) / 10.0));
            for (int c = 0; c < nCh; ++c) {
                float dry = 0;
                float wet = ps[c].process(x[c], ratio, dry);
                x[c] = atOn ? dry + (wet - dry) * blend : dry;
            }
        }

        if (c1On) pkG1 = juce::jmax(pkG1, comp1.process(x, nCh)); else comp1.gr = 0;
        if (c2On) pkG2 = juce::jmax(pkG2, comp2.process(x, nCh)); else comp2.gr = 0;

        if (satOn && sd > 0.0f)
            for (int c = 0; c < nCh; ++c) {
                float wet = std::tanh(satPre * x[c]) / satPre;
                x[c] += (wet - x[c]) * satMix;
            }

        if (tonOn)
            for (int c = 0; c < nCh; ++c) x[c] = air.process(pres.process(warm.process(x[c], c), c), c);

        for (int c = 0; c < nCh; ++c) d[c][i] = x[c];
    }

    // ---------- Reverb (envío paralelo) ----------
    const float revMix = v("revMix") / 100.0f;
    const bool doRev = revOn && revMix > 0.001f;
    if (doRev) {
        revBuf.setSize(2, n, false, false, true);
        for (int c = 0; c < 2; ++c) {
            auto* w = revBuf.getWritePointer(c);
            auto* s = d[juce::jmin(c, nCh - 1)];
            for (int i = 0; i < n; ++i) w[i] = revHp.process(s[i], c);
        }
        juce::Reverb::Parameters rp;
        rp.roomSize = 0.3f + 0.65f * v("revSize") / 100.0f;
        rp.damping = v("revDamp") / 100.0f;
        rp.wetLevel = 0.33f; rp.dryLevel = 0.0f; rp.width = 1.0f; rp.freezeMode = 0.0f;
        reverb.setParameters(rp);
        reverb.processStereo(revBuf.getWritePointer(0), revBuf.getWritePointer(1), n);
        for (int c = 0; c < 2; ++c) { auto* w = revBuf.getWritePointer(c); for (int i = 0; i < n; ++i) w[i] = revLp.process(w[i], c); }
    }

    // ---------- Delay (ping-pong sincronizado al BPM) + salida + limitador ----------
    double bpm = 120.0;
    if (auto* ph = getPlayHead()) if (auto pos = ph->getPosition()) if (auto b = pos->getBpm()) if (*b > 20.0) bpm = *b;
    static const float mult[4] = {1.0f, 0.5f, 0.75f, 0.25f};
    const int dsz = (int) dly[0].size();
    const int dSamp = juce::jlimit(1, dsz - 1, (int) (sr * 60.0 / bpm * mult[juce::jlimit(0, 3, (int) v("dlyDiv"))]));
    const float dMix = v("dlyMix") / 100.0f, dFb = v("dlyFb") / 100.0f;
    const float lpA = 1.0f - std::exp(-2.0f * juce::MathConstants<float>::pi * 4500.0f / (float) sr);
    const float ceilLin = dbToGain(v("ceil"));
    const float limRel = 1.0f - std::exp(-1.0f / (0.08f * (float) sr));
    float pkOut = 0, pkLim = 0;

    for (int i = 0; i < n; ++i) {
        float o[2] = { d[0][i], nCh > 1 ? d[1][i] : 0.0f };
        const float dryMono = nCh > 1 ? 0.5f * (o[0] + o[1]) : o[0];

        const int rp = (dlyWp - dSamp + dsz) % dsz;
        const float dl = dly[0][(size_t) rp], dr = nCh > 1 ? dly[1][(size_t) rp] : 0.0f;
        dlyLp[0] += lpA * (dl - dlyLp[0]);
        dlyLp[1] += lpA * (dr - dlyLp[1]);
        if (dlyOn) {
            if (nCh > 1) { dly[0][(size_t) dlyWp] = dryMono + dFb * dlyLp[1]; dly[1][(size_t) dlyWp] = dFb * dlyLp[0]; }
            else dly[0][(size_t) dlyWp] = dryMono + dFb * dlyLp[0];
            o[0] += dl * dMix; if (nCh > 1) o[1] += dr * dMix;
        } else { dly[0][(size_t) dlyWp] = 0; if (nCh > 1) dly[1][(size_t) dlyWp] = 0; }
        dlyWp = (dlyWp + 1) % dsz;

        if (doRev) for (int c = 0; c < nCh; ++c) o[c] += revBuf.getSample(c, i) * revMix;

        const float og = outSm.getNextValue();
        float pk = 0;
        for (int c = 0; c < nCh; ++c) { o[c] *= og; pk = juce::jmax(pk, std::abs(o[c])); }
        float need = pk > ceilLin ? ceilLin / pk : 1.0f;
        if (need < limG) limG = need; else limG += limRel * (need - limG);
        pkLim = juce::jmax(pkLim, -20.0f * std::log10(juce::jmax(limG, 1.0e-4f)));
        for (int c = 0; c < nCh; ++c) {
            float y = juce::jlimit(-ceilLin, ceilLin, o[c] * limG);
            d[c][i] = y; pkOut = juce::jmax(pkOut, std::abs(y));
        }
    }

    atomicMax(inPk, pkIn); atomicMax(outPk, pkOut); atomicMax(gr1, pkG1); atomicMax(gr2, pkG2);
    atomicMax(dsGr, pkDs); atomicMax(limGr, pkLim);
    gateG.store(gGain);
}

void MixingVSTProcessor::applyPreset(int index) {
    const auto& ps_ = presets();
    if (index < 0 || index >= (int) ps_.size()) return;
    auto setV = [this](const juce::String& id, float val) {
        if (auto* p = apvts.getParameter(id)) p->setValueNotifyingHost(p->convertTo0to1(val));
    };
    for (auto& s : floatSpecs()) setV(s.id, s.def);
    for (auto& t : toggleSpecs()) setV(t.id, t.def ? 1.0f : 0.0f);
    for (auto& kv : ps_[(size_t) index].v) setV(kv.first, kv.second);
}

juce::AudioProcessorEditor* MixingVSTProcessor::createEditor() { return new MixingVSTEditor(*this); }

void MixingVSTProcessor::getStateInformation(juce::MemoryBlock& dest) {
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary(*xml, dest);
}
void MixingVSTProcessor::setStateInformation(const void* data, int size) {
    if (auto xml = getXmlFromBinary(data, size))
        if (xml->hasTagName(apvts.state.getType())) apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new MixingVSTProcessor(); }
