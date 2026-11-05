// AJT JOY16 Advanced — sound engine.
// A line-for-line port of the AudioWorklet in vcs3-advanced/index.html; keep the two in step.
// Signals are in volts, using the levels and sensitivities of the 1970 VCS3 Users Manual.
#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
#include <algorithm>

namespace vcs3 {

constexpr double kPi = 3.14159265358979323846;
constexpr int NS = 18, ND = 16, MAXV = 8;
enum Col { MET = 0, OUT1, OUT2, ENV, RA, RB, REV, FIN, O1F, O2F, O3F, DEC, RMIX, FF, L1, L2 };

struct Rng {
    uint32_t s;
    explicit Rng(uint32_t seed = 0x9e3779b9u) : s(seed ? seed : 1) {}
    double next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s / 4294967296.0; } // [0,1)
    double bi() { return next() * 2.0 - 1.0; }
};

inline double blep(double t, double dt) {
    if (t < dt) { t /= dt; return t + t - t * t - 1; }
    if (t > 1 - dt) { t = (t - 1) / dt; return t * t + t + t + 1; }
    return 0;
}

struct Spring {
    std::vector<float> buf; int len = 8, i = 0, n; double a; std::vector<double> z; double lp = 0;
    Spring(double sr, double ms, int n_, double a_) : n(n_), a(a_), z(n_, 0.0) {
        len = std::max(8, (int) std::lround(sr * ms / 1000.0)); buf.assign(len, 0.f);
    }
    double tick(double x, double fb) {
        const double d = buf[i];
        double y = x + d * fb;
        for (int k = 0; k < n; ++k) { const double o = a * y + z[k]; z[k] = y - a * o; y = o; }
        lp += 0.4 * (y - lp);
        buf[i] = (float) lp;
        if (++i >= len) i = 0;
        return d;
    }
};

// Diode ladder (Civolani–Fontana style state equations), trapezoidal rule + Newton, 2x oversampled.
struct DiodeLadder {
    double sr, h, x[4] = {0, 0, 0, 0}, uPrev = 0; Rng* rng;
    static constexpr double ETA = 1.836, W180 = 0.97334;
    DiodeLadder(double sr_, Rng* r) : sr(sr_), h(1.0 / (2.0 * sr_)), rng(r) {}
    void rhs(const double* s, double u, double a, double k, double* out) const {
        const double t0 = std::tanh(u - k * s[3]), t12 = std::tanh(s[1] - s[0]), t23 = std::tanh(s[2] - s[1]),
                     t34 = std::tanh(s[3] - s[2]), t4 = std::tanh(s[3] / 3);
        out[0] = a * (t0 + t12); out[1] = a * (t23 - t12); out[2] = a * (t34 - t23); out[3] = a * (-t4 - t34);
    }
    void step(double u, double a, double k) {
        const double h2 = h / 2; double f[4], y[4], J[16], r[4];
        rhs(x, uPrev, a, k, f);
        for (int i = 0; i < 4; ++i) y[i] = x[i] + 2 * h2 * f[i];
        for (int it = 0; it < 4; ++it) {
            const double t0 = std::tanh(u - k * y[3]), t12 = std::tanh(y[1] - y[0]), t23 = std::tanh(y[2] - y[1]),
                         t34 = std::tanh(y[3] - y[2]), t4 = std::tanh(y[3] / 3);
            const double s0 = 1 - t0 * t0, s12 = 1 - t12 * t12, s23 = 1 - t23 * t23, s34 = 1 - t34 * t34, s4 = 1 - t4 * t4;
            const double c = h2 * a;
            r[0] = y[0] - x[0] - h2 * f[0] - c * (t0 + t12);
            r[1] = y[1] - x[1] - h2 * f[1] - c * (t23 - t12);
            r[2] = y[2] - x[2] - h2 * f[2] - c * (t34 - t23);
            r[3] = y[3] - x[3] - h2 * f[3] - c * (-t4 - t34);
            J[0] = 1 + c * s12; J[1] = -c * s12; J[2] = 0; J[3] = c * k * s0;
            J[4] = -c * s12; J[5] = 1 + c * (s23 + s12); J[6] = -c * s23; J[7] = 0;
            J[8] = 0; J[9] = -c * s23; J[10] = 1 + c * (s34 + s23); J[11] = -c * s34;
            J[12] = 0; J[13] = 0; J[14] = -c * s34; J[15] = 1 + c * (s4 / 3 + s34);
            double m = J[4] / J[0]; J[5] -= m * J[1]; J[7] -= m * J[3]; r[1] -= m * r[0];
            m = J[9] / J[5]; J[10] -= m * J[6]; J[11] -= m * J[7]; r[2] -= m * r[1];
            m = J[14] / J[10]; J[15] -= m * J[11]; r[3] -= m * r[2];
            const double d3 = r[3] / J[15];
            const double d2 = (r[2] - J[11] * d3) / J[10];
            const double d1 = (r[1] - J[6] * d2 - J[7] * d3) / J[5];
            const double d0 = (r[0] - J[1] * d1 - J[3] * d3) / J[0];
            y[0] -= d0; y[1] -= d1; y[2] -= d2; y[3] -= d3;
            if (std::abs(d3) + std::abs(d0) < 1e-9) break;
        }
        for (int i = 0; i < 4; ++i) x[i] = y[i];
        uPrev = u;
    }
    double process(double inp, double fc, double K) {
        const double fsO = 2 * sr; if (fc > fsO * 0.24) fc = fsO * 0.24;
        const double w = 2 * kPi * fc, warp = std::tan(w * h / 2) / (w * h / 2);
        const double a = w * warp / W180, k = (K + 0.5) * ETA;
        const double u = inp * 1.2 + rng->bi() * 0.5e-6;
        const double um = 0.5 * (uPrev + u);
        step(um, a, k); step(u, a, k);
        return (K + 0.5) * x[3] / (0.4 * 1.2);
    }
};

// Parameter indices, resolved by name from the default snapshot (same ids as the page's knobs).
struct ParamIx {
    int o1f, o1shape, o1sin, o1saw, o2f, o2shape, o2sqr, o2tri, o3f, o3shape, o3sqr, o3tri, noiseCol, noiseLvl, ringLvl,
        filtF, filtRes, filtLvl, envA, envOn, envD, envOff, trapLvl, sigLvl, dkFreq, dkSpread, dkDyn, dkLvl, in1Lvl, in2Lvl,
        revMix, revLvl, out1Lvl, out1Filt, out1Pan, out2Lvl, out2Filt, out2Pan, stkXr, stkYr, master, analog, kbdGlide;
};

struct Note { int64_t id; int note; float vel; int ch; float bend = 0, press = 0; };

struct Shared {
    std::vector<double> tg, c; ParamIx I{};
    std::vector<int> ps, pd; std::vector<double> pg;
    bool hold = false, in1Sig = false, in2Key = false, in1Ext = false, in2Ext = false;
    double extL = 0, extR = 0; // track audio (FX edition), in volts
    double stkX = 0, stkY = 0, toneK = 0, acK = 0, dkK = 0, fmax = 0, an = 0, hf = 0, glK = 1;
    double p1l = 0, p1r = 0, p2l = 0, p2r = 0;
};

struct Voice {
    double sr; Rng rng;
    std::array<double, NS> src{}; std::array<double, ND> col{};
    double ph1, ph2, ph3, dkPh, nlp = 0; DiodeLadder vcf;
    double e = 0, ht = 0; int stage = 0; bool gate = false, pend = false;
    Spring sp1, sp2; double rhA = 0, rhB = 0, t1 = 0, t2 = 0;
    double kbdT = 0, kbd = 0, dyn = 0, press = 0, slide = 0, dkAmp = 0; bool dkHeld = false;
    const Note* note = nullptr; int64_t noteId = 0; bool active = false, released = true; double fade = 0; uint64_t age = 0;
    double dr[3] = {0, 0, 0}, warm[3], pm[3] = {1, 1, 1};
    double L = 0, R = 0, met = 0;

    Voice(double sr_, uint32_t seed) : sr(sr_), rng(seed), vcf(sr_, &rng), sp1(sr_, 25, 16, -0.62), sp2(sr_, 30, 16, -0.58) {
        ph1 = rng.next(); ph2 = rng.next(); ph3 = rng.next(); dkPh = rng.next();
        for (double& w : warm) w = rng.bi();
    }
    void block(double an, double wf) {
        for (int i = 0; i < 3; ++i) {
            dr[i] += rng.bi() * 0.055 - dr[i] * 0.0005;
            pm[i] = std::pow(2.0, an * (0.012 * dr[i] + 0.04 * warm[i] * wf));
        }
    }
    void tick(Shared& E) {
        const auto& c = E.c; const auto& I = E.I;
        col.fill(0);
        const size_t np = E.ps.size();
        for (size_t q = 0; q < np; ++q) col[E.pd[q]] += src[E.ps[q]] * E.pg[q];
        const double fmax = E.fmax, hf = E.hf, an = E.an;

        double f1 = c[I.o1f] * pm[0] * std::pow(2.0, -col[O1F] / 0.32); f1 /= 1 + f1 * hf; f1 = std::clamp(f1, 0.001, fmax);
        const double dt1 = f1 / sr; ph1 += dt1; if (ph1 >= 1) ph1 -= std::floor(ph1);
        double ph = ph1, sh = std::clamp(c[I.o1shape], 0.05, 0.95);
        const double w = ph < sh ? 0.5 * ph / sh : 0.5 + 0.5 * (ph - sh) / (1 - sh);
        const double sin1 = 1.5 * std::sin(2 * kPi * w);
        const double saw1 = 2 * (1 - 2 * ph + blep(ph, dt1));

        double f2 = c[I.o2f] * pm[1] * std::pow(2.0, -col[O2F] / 0.32); f2 /= 1 + f2 * hf; f2 = std::clamp(f2, 0.001, fmax);
        const double dt2 = f2 / sr; ph2 += dt2; if (ph2 >= 1) ph2 -= std::floor(ph2);
        ph = ph2; sh = std::clamp(c[I.o2shape], 0.03, 0.97);
        double sq2 = ph < sh ? 1 : -1; sq2 += blep(ph, dt2); sq2 -= blep(std::fmod(ph + 1 - sh, 1.0), dt2);
        const double tri2 = 1.5 * (1 + std::abs(2 * sh - 1)) * (ph < sh ? -1 + 2 * ph / sh : 1 - 2 * (ph - sh) / (1 - sh));
        sq2 *= 2;

        double f3 = c[I.o3f] * pm[2] * std::pow(2.0, -col[O3F] / 0.26); f3 /= 1 + f3 * hf; f3 = std::clamp(f3, 0.001, fmax);
        const double dt3 = f3 / sr; ph3 += dt3; if (ph3 >= 1) ph3 -= std::floor(ph3);
        ph = ph3; sh = std::clamp(c[I.o3shape], 0.03, 0.97);
        double sq3 = ph < sh ? 1 : -1; sq3 += blep(ph, dt3); sq3 -= blep(std::fmod(ph + 1 - sh, 1.0), dt3);
        const double tri3 = 1.5 * (1 + std::abs(2 * sh - 1)) * (ph < sh ? -1 + 2 * ph / sh : 1 - 2 * (ph - sh) / (1 - sh));
        sq3 *= 2;

        const double wn = rng.bi();
        nlp += 0.02 * (wn - nlp);
        const double nc = c[I.noiseCol];
        const double noise = 1.5 * (nc < 0 ? wn * (1 + nc) + nlp * 4 * (-nc) : wn * (1 - nc) + (wn - nlp) * nc) * c[I.noiseLvl] / 10;

        rhA += E.acK * (col[RA] - rhA); rhB += E.acK * (col[RB] - rhB);
        const double ra = col[RA] - rhA, rb = col[RB] - rhB;
        double ring = ra * rb * 5.33 * c[I.ringLvl] / 10;
        ring += an * 0.02 * (ra + rb) * c[I.ringLvl] / 10;
        ring = 3.5 * std::tanh(ring / 3.5);

        double fc = c[I.filtF] * std::pow(2.0, -col[FF] / 0.2); if (fc < 5) fc = 5;
        const double filt = vcf.process(col[FIN] * 0.5, fc, c[I.filtRes] * 0.83) * 2 * c[I.filtLvl] / 10;

        if (pend) { pend = false; stage = 1; ht = 0; }
        const bool g = gate || E.hold;
        const bool manual = E.tg[I.envOff] >= 6;
        const double ov = c[I.envOff], offT = ov <= 5 ? 0.01 * std::pow(500.0, ov / 5) : 5;
        double decT = c[I.envD] * std::pow(2.0, col[DEC] / 0.4); decT = std::clamp(decT, 0.003, 300.0);
        const double dtS = 1 / sr;
        switch (stage) {
            case 0: if (!manual) { stage = 4; ht = 0; } break;
            case 1:
                ht += dtS; e += dtS / std::max(c[I.envA], 0.002);
                if (ht >= c[I.envOn] && !g) e -= dtS / decT;
                if (e >= 1) e = 1;
                if (e >= 1 || ht >= c[I.envA]) stage = 2;
                if (e < 0) { e = 0; stage = manual ? 0 : 4; ht = 0; }
                break;
            case 2: ht += dtS; if (ht >= c[I.envOn] && !g) stage = 3; break;
            case 3: e -= dtS / decT; if (e <= 0) { e = 0; stage = manual ? 0 : 4; ht = 0; } break;
            case 4: ht += dtS; if (manual) stage = 0; else if (ht >= offT) { stage = 1; ht = 0; } break;
            default: break;
        }
        const double trap = 3 * (1 - 2 * e) * c[I.trapLvl] / 10;
        const double envSig = col[ENV] * e * c[I.sigLvl] / 10;

        const double rin = col[REV];
        const double wet = (sp1.tick(rin * 0.5, 0.9) + sp2.tick(rin * 0.5, 0.89)) * 1.2;
        const double mix = std::clamp(c[I.revMix] / 10 + col[RMIX] / 4, 0.0, 1.0);
        const double rev = (rin * (1 - mix) + wet * mix) * c[I.revLvl] / 10;

        const double g1 = std::clamp((c[I.out1Lvl] - 4) / 6 + col[L1] / 4, 0.0, 1.5);
        double a1 = col[OUT1] * g1;
        t1 += E.toneK * (a1 - t1);
        const double tf1 = c[I.out1Filt]; a1 = tf1 < 0 ? a1 * (1 + tf1) + t1 * (-tf1) : a1 * (1 - tf1) + (a1 - t1) * tf1;
        const double g2 = std::clamp((c[I.out2Lvl] - 4) / 6 + col[L2] / 4, 0.0, 1.5);
        double a2 = col[OUT2] * g2;
        t2 += E.toneK * (a2 - t2);
        const double tf2 = c[I.out2Filt]; a2 = tf2 < 0 ? a2 * (1 + tf2) + t2 * (-tf2) : a2 * (1 - tf2) + (a2 - t2) * tf2;
        L = a1 * E.p1l + a2 * E.p2l; R = a1 * E.p1r + a2 * E.p2r;
        met = col[MET];

        kbd += (kbdT - kbd) * E.glK;
        const double semis = -kbd * 12;
        const double fdk = std::min(fmax, c[I.dkFreq] * std::pow(2.0, semis * (12 + c[I.dkSpread]) / 144));
        const double dtk = fdk / sr; dkPh += dtk; if (dkPh >= 1) dkPh -= std::floor(dkPh);
        const double dkSaw = 2 * (2 * dkPh - 1 - blep(dkPh, dtk));
        const double dkT = dkHeld ? std::clamp(0.6 + 0.4 * dyn * c[I.dkDyn] / 10, 0.0, 1.0) : 0;
        dkAmp += (dkT - dkAmp) * E.dkK;
        const double dkMix = (dkSaw * c[I.dkLvl] / 10 + col[MET]) * dkAmp;

        src[0] = sin1 * c[I.o1sin] / 10; src[1] = saw1 * c[I.o1saw] / 10;
        src[2] = sq2 * c[I.o2sqr] / 10; src[3] = tri2 * c[I.o2tri] / 10;
        src[4] = sq3 * c[I.o3sqr] / 10; src[5] = tri3 * c[I.o3tri] / 10;
        src[6] = noise;
        src[7] = (E.in1Ext ? E.extL : E.in1Sig ? dkMix : kbd) * c[I.in1Lvl] / 10;
        src[8] = (E.in2Ext ? E.extR : E.in2Key ? kbd : dyn) * c[I.in2Lvl] / 10;
        src[9] = filt; src[10] = trap; src[11] = envSig; src[12] = ring; src[13] = rev;
        src[14] = E.stkX; src[15] = E.stkY;
        src[16] = press; src[17] = slide;
    }
};

class Engine {
public:
    Shared E;
    std::unordered_map<std::string, int> paramIndex;
    std::vector<Voice> voices;
    std::vector<Note> notes; // held notes, press order
    float chBend[17] = {}, chPress[17] = {}, chSlide[17] = {};
    int nv = 1; bool highPriority = true;
    double sr = 48000, ageS = 0, dcL = 0, dcR = 0, dxL = 0, dxR = 0; uint64_t clock = 0;
    bool first = true;
    std::vector<float> pinLv; int cnt = 0;
    // read by the editor for the lamps and pin glow
    std::array<float, MAXV> envOut{}, actOut{}; std::vector<float> pinOut; bool viewFresh = false;

    void prepare(double sampleRate, const std::vector<std::string>& names) {
        sr = sampleRate;
        paramIndex.clear();
        for (size_t i = 0; i < names.size(); ++i) paramIndex[names[i]] = (int) i;
        E.tg.assign(names.size(), 0.0); E.c.assign(names.size(), 0.0);
        auto ix = [&](const char* n) { auto it = paramIndex.find(n); return it == paramIndex.end() ? 0 : it->second; };
        auto& I = E.I;
        I = { ix("o1f"), ix("o1shape"), ix("o1sin"), ix("o1saw"), ix("o2f"), ix("o2shape"), ix("o2sqr"), ix("o2tri"),
              ix("o3f"), ix("o3shape"), ix("o3sqr"), ix("o3tri"), ix("noiseCol"), ix("noiseLvl"), ix("ringLvl"),
              ix("filtF"), ix("filtRes"), ix("filtLvl"), ix("envA"), ix("envOn"), ix("envD"), ix("envOff"), ix("trapLvl"), ix("sigLvl"),
              ix("dkFreq"), ix("dkSpread"), ix("dkDyn"), ix("dkLvl"), ix("in1Lvl"), ix("in2Lvl"), ix("revMix"), ix("revLvl"),
              ix("out1Lvl"), ix("out1Filt"), ix("out1Pan"), ix("out2Lvl"), ix("out2Filt"), ix("out2Pan"), ix("stkXr"), ix("stkYr"),
              ix("master"), ix("analog"), ix("kbdGlide") };
        voices.clear();
        for (int i = 0; i < MAXV; ++i) voices.emplace_back(sr, 0x1234567u + 7919u * (uint32_t) i);
        notes.clear(); notes.reserve(128);
        voices[0].active = true; voices[0].fade = 1;
        first = true;
    }
    void setParam(const std::string& name, double v) {
        auto it = paramIndex.find(name); if (it == paramIndex.end()) return;
        E.tg[it->second] = v;
    }
    void paramsDone() { if (first) { E.c = E.tg; first = false; } }
    void setPins(std::vector<int> s, std::vector<int> d, std::vector<double> g) {
        E.ps = std::move(s); E.pd = std::move(d); E.pg = std::move(g);
        pinLv.assign(E.ps.size(), 0.f); pinOut.assign(E.ps.size(), 0.f);
    }
    void setDk(bool in1Sig, bool in2Key, bool high, bool in1Ext = false, bool in2Ext = false) {
        E.in1Sig = in1Sig; E.in2Key = in2Key; E.in1Ext = in1Ext; E.in2Ext = in2Ext; highPriority = high;
    }
    void setStick(double x, double y) { stkX = x; stkY = y; }
    void setHold(bool h) { E.hold = h; }
    void trigger() { for (int i = 0; i < nv; ++i) { auto& v = voices[i]; v.pend = true; v.active = true; v.released = false; } }
    void setVoices(int n) {
        allOff(); nv = std::clamp(n, 1, MAXV);
        for (int i = 0; i < MAXV; ++i) voices[i].active = (nv == 1 && i == 0);
    }
    void allOff() { notes.clear(); for (auto& v : voices) release(v); }

    double keyVolts(const Note& n) const { return -((n.note + n.bend + chBend[n.ch]) - 66) / 12.0; }
    void apply(Voice& v, const Note& n, bool retrig) {
        v.kbdT = keyVolts(n); v.dyn = n.vel; v.gate = true; v.dkHeld = true; v.noteId = n.id; v.note = &n;
        v.press = chPress[n.ch] + n.press; v.slide = chSlide[n.ch];
        if (retrig) v.pend = true;
        v.active = true; v.released = false; v.age = ++clock;
    }
    void release(Voice& v) { v.gate = false; v.dkHeld = false; v.noteId = 0; v.note = nullptr; v.released = true; }
    Note* findNote(int64_t id) { for (auto& n : notes) if (n.id == id) return &n; return nullptr; }
    const Note* monoPick() const {
        if (notes.empty()) return nullptr;
        if (!highPriority) return &notes.back();
        const Note* best = &notes[0];
        for (auto& n : notes) if (n.note > best->note) best = &n;
        return best;
    }
    void relink() { // note pointers move when the vector changes
        for (auto& v : voices) { v.note = nullptr; if (v.noteId) for (auto& n : notes) if (n.id == v.noteId) v.note = &n; }
    }
    void noteOn(int64_t id, int note, float vel, int ch) {
        notes.erase(std::remove_if(notes.begin(), notes.end(), [&](const Note& n) { return n.id == id; }), notes.end());
        notes.push_back({ id, note, vel, ch });
        relink();
        if (nv == 1) {
            auto& v = voices[0];
            const Note* pick = monoPick();
            const bool wasHeld = v.noteId != 0;
            apply(v, *pick, !wasHeld || pick->id == id);
            if (wasHeld && pick->id != id) v.pend = false;
            return;
        }
        Voice* target = nullptr;
        for (int i = 0; i < nv; ++i) if (!voices[i].active) { target = &voices[i]; break; }
        if (!target) for (int i = 0; i < nv; ++i) if (voices[i].noteId == 0 && (!target || voices[i].age < target->age)) target = &voices[i];
        if (!target) {
            for (int i = 0; i < nv; ++i) if (!target || voices[i].age < target->age) target = &voices[i];
            const int64_t stolen = target->noteId;
            notes.erase(std::remove_if(notes.begin(), notes.end(), [&](const Note& n) { return n.id == stolen; }), notes.end());
            relink();
        }
        apply(*target, *findNote(id), true);
    }
    void noteOff(int64_t id) {
        if (!findNote(id)) return;
        notes.erase(std::remove_if(notes.begin(), notes.end(), [&](const Note& n) { return n.id == id; }), notes.end());
        relink();
        if (nv == 1) {
            auto& v = voices[0];
            if (const Note* p = monoPick()) apply(v, *p, false); else release(v);
            return;
        }
        for (auto& v : voices) if (v.noteId == id) { release(v); break; }
    }
    void updateExpr(int ch) {
        for (auto& v : voices) if (v.note && (v.note->ch == ch || ch == 0)) {
            v.kbdT = keyVolts(*v.note); v.press = chPress[v.note->ch] + v.note->press; v.slide = chSlide[v.note->ch];
        }
    }
    void bend(int ch, float semis) { if (ch >= 0 && ch <= 16) { chBend[ch] = semis; updateExpr(ch); } }
    void pressure(int ch, float v) { if (ch >= 0 && ch <= 16) { chPress[ch] = v; updateExpr(ch); } }
    void notePressure(int64_t id, float v) { if (Note* n = findNote(id)) { n->press = v; updateExpr(n->ch); } }
    void slide(int ch, float v) { if (ch >= 0 && ch <= 16) { chSlide[ch] = v; updateExpr(ch); } }

    // render n samples; meter may be null. inL/inR (may be null) carry the track audio of the FX edition;
    // each input sample is read before the output sample at the same index is written, so they may alias outL/outR.
    static constexpr double kExtVolts = 2.5; // full-scale input ~ the oscillators' level
    void render(float* outL, float* outR, float* meter, int n, const float* inL = nullptr, const float* inR = nullptr) {
        auto& tg = E.tg; auto& c = E.c; const auto& I = E.I;
        if (c.empty()) { std::fill(outL, outL + n, 0.f); std::fill(outR, outR + n, 0.f); return; }
        const double sm = 1 - std::exp(-1 / (0.012 * sr));
        E.toneK = 1 - std::exp(-2 * kPi * 700 / sr);
        E.acK = 1 - std::exp(-2 * kPi * 5 / sr);
        E.dkK = 1 - std::exp(-1 / (0.004 * sr));
        E.fmax = sr * 0.45;
        E.an = tg[I.analog] / 10; E.hf = E.an / 200000;
        ageS += n / sr;
        const double wf = std::exp(-ageS / 90);
        const bool poly = nv > 1;
        for (int i = 0; i < nv; ++i) voices[i].block(E.an, wf);
        const double fadeK = 1 / (0.006 * sr);
        const size_t np = E.ps.size();
        for (int t = 0; t < n; ++t) {
            for (size_t j = 0; j < c.size(); ++j) c[j] += (tg[j] - c[j]) * sm;
            const double p1 = (c[I.out1Pan] + 1) * kPi / 4, p2 = (c[I.out2Pan] + 1) * kPi / 4;
            E.p1l = std::cos(p1); E.p1r = std::sin(p1); E.p2l = std::cos(p2); E.p2r = std::sin(p2);
            const double gl = c[I.kbdGlide]; E.glK = gl < 0.002 ? 1 : 1 - std::exp(-1 / (gl * sr));
            E.stkX = 2 * stkX * c[I.stkXr] / 10; E.stkY = 2 * stkY * c[I.stkYr] / 10;
            E.extL = inL ? inL[t] * kExtVolts : 0; E.extR = inR ? inR[t] * kExtVolts : E.extL;
            double l = 0, r = 0, met = 0;
            for (int i = 0; i < nv; ++i) {
                auto& v = voices[i];
                if (poly && v.active && v.released && (v.stage == 0 || v.stage == 4) && !v.pend) v.active = false;
                const double target = (!poly || v.active) ? 1 : 0;
                if (target == 0 && v.fade <= 0) continue;
                v.fade += target > v.fade ? fadeK : -fadeK; v.fade = std::clamp(v.fade, 0.0, 1.0);
                v.tick(E);
                l += v.L * v.fade; r += v.R * v.fade; met += v.met * v.fade;
                for (size_t q = 0; q < np; ++q) { const float a = (float) std::abs(v.src[E.ps[q]] * E.pg[q]); if (a > pinLv[q]) pinLv[q] = a; }
            }
            const double m = c[I.master] / 10 * 0.45 / (poly ? std::sqrt((double) nv) * 0.75 : 1);
            l *= m; r *= m;
            const double ol = l - dxL + 0.9995 * dcL; dxL = l; dcL = ol;
            const double orr = r - dxR + 0.9995 * dcR; dxR = r; dcR = orr;
            outL[t] = (float) std::tanh(ol * 0.9); outR[t] = (float) std::tanh(orr * 0.9);
            if (meter) meter[t] = (float) met;
        }
        if ((cnt += n) >= 1536) {
            cnt = 0;
            for (int i = 0; i < MAXV; ++i) { envOut[i] = i < nv ? (float) voices[i].e : 0.f; actOut[i] = (i < nv && voices[i].active) ? 1.f : 0.f; }
            pinOut = pinLv; std::fill(pinLv.begin(), pinLv.end(), 0.f);
            viewFresh = true;
        }
    }
private:
    double stkX = 0, stkY = 0;
};

} // namespace vcs3
