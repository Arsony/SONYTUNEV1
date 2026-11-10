#pragma once
// ============================================================================
//  SonyTune DSP core  (CrushAudio)
//  Header-only, no JUCE dependency, so it can be unit-tested on its own.
//
//  Signal flow
//    input -> YIN pitch tracker (12 kHz decimated) -> pitch track (median-smoothed)
//          -> scale quantiser + retune smoothing  -> correction (semitones)
//          -> pitch-synchronous overlap-add (PSOLA) resynthesis, formants preserved
//          -> dry/wet mix (dry is delay-compensated)
//
//  Everything runs at a fixed latency (see Engine::getLatencySamples()).
//  Nothing allocates after prepare().
// ============================================================================

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <vector>

namespace sonytune
{

constexpr float kPi     = 3.14159265358979323846f;
constexpr float kMinF0  = 75.0f;    // lowest voice pitch tracked (Hz)
constexpr float kMaxF0  = 900.0f;   // highest voice pitch tracked (Hz)

// ============================================================================
//  Scales
// ============================================================================
constexpr int kNumScales = 10;

inline const char* scaleName (int i)
{
    static const char* names[kNumScales] = { "Chromatic", "Major", "Minor", "Harmonic Minor", "Major Pentatonic",
                                              "Minor Pentatonic", "Blues", "Dorian", "Phrygian", "Mixolydian" };
    return names[std::clamp (i, 0, kNumScales - 1)];
}

// bit n set = the note n semitones above the key is allowed
inline int scaleMask (int i)
{
    static const auto make = [] (std::initializer_list<int> notes)
    {
        int m = 0;
        for (int n : notes) m |= 1 << n;
        return m;
    };

    static const int masks[kNumScales] = {
        make ({ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 }),  // Chromatic
        make ({ 0, 2, 4, 5, 7, 9, 11 }),                   // Major
        make ({ 0, 2, 3, 5, 7, 8, 10 }),                   // Natural minor
        make ({ 0, 2, 3, 5, 7, 8, 11 }),                   // Harmonic minor
        make ({ 0, 2, 4, 7, 9 }),                          // Major pentatonic
        make ({ 0, 3, 5, 7, 10 }),                         // Minor pentatonic
        make ({ 0, 3, 5, 6, 7, 10 }),                      // Blues
        make ({ 0, 2, 3, 5, 7, 9, 10 }),                   // Dorian
        make ({ 0, 1, 3, 5, 7, 8, 10 }),                   // Phrygian
        make ({ 0, 2, 4, 5, 7, 9, 10 })                    // Mixolydian
    };
    return masks[std::clamp (i, 0, kNumScales - 1)];
}

inline bool isAllowedNote (int note, int key, int mask)
{
    const int pc = ((note - key) % 12 + 12) % 12;
    return (mask & (1 << pc)) != 0;
}

constexpr int kNoTarget = -1000;

// nearest allowed note to 'midi', with hysteresis so the target doesn't flutter
// between two notes when the singer sits near the boundary
inline int nearestAllowed (float midi, int key, int scaleIdx, int prevTarget, float hysteresis)
{
    const int mask = scaleMask (scaleIdx);
    const int base = (int) std::floor (midi);
    int best = base;
    float bestDist = 1.0e9f;

    for (int n = base - 7; n <= base + 8; ++n)
    {
        if (! isAllowedNote (n, key, mask)) continue;
        const float d = std::abs (midi - (float) n);
        if (d < bestDist) { bestDist = d; best = n; }
    }

    if (prevTarget != kNoTarget && isAllowedNote (prevTarget, key, mask))
        if (std::abs (midi - (float) prevTarget) <= bestDist + hysteresis)
            return prevTarget;

    return best;
}

// ============================================================================
//  YIN pitch tracker (runs on a ~12 kHz decimated copy of the signal)
// ============================================================================
class PitchTracker
{
public:
    struct Result
    {
        bool  ready  = false;   // a new estimate was produced on this sample
        bool  voiced = false;
        float f0     = 0.f;     // Hz
        float conf   = 0.f;     // 0..1
    };

    void prepare (double sampleRate)
    {
        fs      = sampleRate;
        D       = std::max (1, (int) std::lround (fs / 12000.0));
        fsD     = fs / (double) D;
        tauMax  = (int) std::ceil (fsD / (double) kMinF0);
        tauMin  = std::max (2, (int) std::floor (fsD / (double) kMaxF0));
        W       = tauMax;
        frameLen = W + tauMax + 3;
        hopDec  = std::max (8, (int) std::lround (0.0027 * fsD));   // ~2.7 ms hop

        ring.assign ((size_t) frameLen, 0.f);
        frame.assign ((size_t) frameLen, 0.f);
        diff.assign ((size_t) tauMax + 2, 0.f);
        cmnd.assign ((size_t) tauMax + 2, 0.f);
        reset();
    }

    void reset()
    {
        std::fill (ring.begin(), ring.end(), 0.f);
        head = 0; filled = 0; since = 0; acc = 0.f; accCount = 0; xPrev = 0.f; yPrev = 0.f;
    }

    // how many full-rate samples *before the newest sample* the estimate is centred on
    int getCenterOffset() const { return D * (W + tauMax) / 2; }

    Result push (float x)
    {
        Result r;
        // DC blocker (removes rumble / offset that confuses the detector)
        const float y = x - xPrev + 0.995f * yPrev;
        xPrev = x; yPrev = y;

        acc += y;
        if (++accCount < D) return r;
        const float s = acc / (float) D;
        acc = 0.f; accCount = 0;

        ring[(size_t) head] = s;
        head = (head + 1) % frameLen;
        if (filled < frameLen) ++filled;
        if (++since < hopDec || filled < frameLen) return r;
        since = 0;

        for (int i = 0; i < frameLen; ++i)
            frame[(size_t) i] = ring[(size_t) ((head + i) % frameLen)];

        return analyse();
    }

private:
    Result analyse()
    {
        Result r;
        r.ready = true;

        float e = 0.f;
        for (int i = 0; i < frameLen; ++i) e += frame[(size_t) i] * frame[(size_t) i];
        const float rms = std::sqrt (e / (float) frameLen);
        if (rms < 0.0012f) return r;                       // silence

        // difference function
        for (int tau = 1; tau <= tauMax; ++tau)
        {
            float sum = 0.f;
            for (int j = 0; j < W; ++j)
            {
                const float d = frame[(size_t) j] - frame[(size_t) (j + tau)];
                sum += d * d;
            }
            diff[(size_t) tau] = sum;
        }

        // cumulative mean normalised difference
        cmnd[0] = 1.f;
        float run = 0.f;
        for (int tau = 1; tau <= tauMax; ++tau)
        {
            run += diff[(size_t) tau];
            cmnd[(size_t) tau] = run > 1.0e-12f ? diff[(size_t) tau] * (float) tau / run : 1.f;
        }

        int tauEst = -1;
        for (int tau = tauMin; tau <= tauMax; ++tau)
        {
            if (cmnd[(size_t) tau] < 0.15f)
            {
                while (tau + 1 <= tauMax && cmnd[(size_t) (tau + 1)] < cmnd[(size_t) tau]) ++tau;
                tauEst = tau;
                break;
            }
        }
        if (tauEst < 0)
        {
            int best = tauMin;
            for (int tau = tauMin; tau <= tauMax; ++tau)
                if (cmnd[(size_t) tau] < cmnd[(size_t) best]) best = tau;
            if (cmnd[(size_t) best] < 0.30f) tauEst = best;
        }
        if (tauEst < 0) return r;

        float period = (float) tauEst;
        if (tauEst > 1 && tauEst < tauMax)
        {
            const float s0 = cmnd[(size_t) (tauEst - 1)], s1 = cmnd[(size_t) tauEst], s2 = cmnd[(size_t) (tauEst + 1)];
            const float den = s0 + s2 - 2.f * s1;
            if (std::abs (den) > 1.0e-9f) period += std::clamp (0.5f * (s0 - s2) / den, -1.f, 1.f);
        }

        r.voiced = true;
        r.f0     = (float) (fsD / (double) period);
        r.conf   = 1.f - cmnd[(size_t) tauEst];
        return r;
    }

    double fs = 48000.0, fsD = 12000.0;
    int D = 4, tauMin = 13, tauMax = 160, W = 160, frameLen = 323, hopDec = 32;
    int head = 0, filled = 0, since = 0, accCount = 0;
    float acc = 0.f, xPrev = 0.f, yPrev = 0.f;
    std::vector<float> ring, frame, diff, cmnd;
};

// ============================================================================
//  Engine: tracker + corrector + PSOLA resynthesis
// ============================================================================
class Engine
{
public:
    struct Params
    {
        int   key         = 0;       // 0 = C ... 11 = B
        int   scale       = 0;       // index into scaleMask()
        float speedMs     = 15.f;    // retune time constant, 0 = instant (hard tune)
        float amount      = 1.f;     // 0..1 correction strength
        float humanize    = 0.3f;    // 0..1 how much natural vibrato/inflection is kept
        float formantSemis = 0.f;    // -5..+5 formant shift
        float mix         = 1.f;     // 0..1 wet
        bool  fast        = false;   // FLOW: false = MID, true = FAST
        bool  hq          = false;   // false = LIVE (lower latency), true = HQ (look-ahead)
        float refA        = 440.f;
    };

    static constexpr int kVizSize = 512;
    std::atomic<int>   vizWrite { 0 };
    std::array<std::atomic<float>, kVizSize> vizRaw, vizOut;   // midi, -1 = unvoiced

    Engine()
    {
        // the editor can open before audio starts, so never leave these uninitialised
        for (auto& v : vizRaw) v.store (-1.f);
        for (auto& v : vizOut) v.store (-1.f);
    }

    void prepare (double sampleRate, int numChannels)
    {
        fs  = sampleRate;
        nCh = std::clamp (numChannels, 1, 2);
        tracker.prepare (fs);
        inRing[0].assign ((size_t) kRing, 0.f);
        inRing[1].assign ((size_t) kRing, 0.f);
        monoRing.assign ((size_t) kRing, 0.f);
        acc[0].assign ((size_t) kAcc, 0.f);
        acc[1].assign ((size_t) kAcc, 0.f);
        wsum.assign ((size_t) kAcc, 0.f);
        configure();
        resetState();
        prepared = true;
    }

    int getLatencySamples() const { return L; }

    void setParams (const Params& p)
    {
        const bool modeChanged = prepared && (p.hq != P.hq);
        P = p;
        P.amount       = std::clamp (P.amount, 0.f, 1.f);
        P.humanize     = std::clamp (P.humanize, 0.f, 1.f);
        P.mix          = std::clamp (P.mix, 0.f, 1.f);
        P.formantSemis = std::clamp (P.formantSemis, -5.f, 5.f);
        formantRatio   = std::exp2 ((double) P.formantSemis / 12.0);
        if (modeChanged) { configure(); resetState(); }
    }

    // in-place processing; io[ch][n]
    void process (float* const* io, int numCh, int numSamples)
    {
        if (! prepared) return;
        const int chs = std::min (numCh, nCh);

        for (int n = 0; n < numSamples; ++n)
        {
            const int64_t idx = count;
            float mono = 0.f;
            for (int ch = 0; ch < chs; ++ch)
            {
                const float x = io[ch][n];
                inRing[ch][(size_t) (idx & kRingMask)] = x;
                mono += x;
            }
            mono /= (float) chs;
            monoRing[(size_t) (idx & kRingMask)] = mono;
            now = idx;
            ++count;

            const auto res = tracker.push (mono);
            if (res.ready)
            {
                TrackEntry& e = track[(size_t) (trackW & kTrackMask)];
                e.time   = now - (int64_t) tracker.getCenterOffset();
                e.voiced = res.voiced;
                e.midi   = res.voiced ? (float) (69.0 + 12.0 * std::log2 ((double) res.f0 / (double) P.refA)) : 0.f;
                ++trackW;
            }

            while (nextC - (double) maxHalf - 2.0 <= (double) now)
                placeGrain (chs);

            const size_t oi = (size_t) (now & kAccMask);
            const float w = std::max (wsum[oi], 0.25f);
            for (int ch = 0; ch < chs; ++ch)
            {
                const float wet = acc[ch][oi] / w;
                const float dry = inAt (ch, now - (int64_t) L);
                io[ch][n] = dry + (wet - dry) * P.mix;
                acc[ch][oi] = 0.f;
            }
            wsum[oi] = 0.f;

            if ((count & 255) == 0)
            {
                const int vw = vizWrite.load (std::memory_order_relaxed);
                vizRaw[(size_t) (vw % kVizSize)].store (lastVoiced ? lastRaw : -1.f, std::memory_order_relaxed);
                vizOut[(size_t) (vw % kVizSize)].store (lastVoiced ? lastOut : -1.f, std::memory_order_relaxed);
                vizWrite.store (vw + 1, std::memory_order_release);
            }
        }
    }

private:
    static constexpr int     kRing = 1 << 16, kAcc = 1 << 13, kTrackSize = 1024;
    static constexpr int64_t kRingMask = kRing - 1, kAccMask = kAcc - 1, kTrackMask = kTrackSize - 1;

    struct TrackEntry { int64_t time = 0; float midi = 0.f; bool voiced = false; };
    struct PitchInfo  { bool voiced = false; float midi = 0.f; };

    void configure()
    {
        maxHalf = (int) std::ceil (fs / (double) kMinF0);
        tu      = (int) std::lround (0.007 * fs);
        L       = (int) std::ceil (2.75 * (double) maxHalf) + (P.hq ? (int) std::lround (0.025 * fs) : 0);
        refSeg.assign ((size_t) maxHalf + 8, 0.f);
    }

    void resetState()
    {
        std::fill (inRing[0].begin(), inRing[0].end(), 0.f);
        std::fill (inRing[1].begin(), inRing[1].end(), 0.f);
        std::fill (monoRing.begin(), monoRing.end(), 0.f);
        std::fill (acc[0].begin(), acc[0].end(), 0.f);
        std::fill (acc[1].begin(), acc[1].end(), 0.f);
        std::fill (wsum.begin(), wsum.end(), 0.f);
        tracker.reset();
        count = 0; now = -1; trackW = 0;
        nextC = (double) maxHalf + 2.0;
        mark = 0; markValid = false;
        wasVoiced = false; gapSamples = 1.0e9; center = 0.f; corr = 0.f; targetNote = kNoTarget;
        lastVoiced = false; lastRaw = 0.f; lastOut = 0.f;
        for (auto& v : vizRaw) v.store (-1.f);
        for (auto& v : vizOut) v.store (-1.f);
        vizWrite.store (0);
    }

    float inAt (int ch, int64_t idx) const
    {
        if (idx < 0 || idx > now || idx < now - (int64_t) (kRing - 2)) return 0.f;
        return inRing[ch][(size_t) (idx & kRingMask)];
    }

    float monoAt (int64_t idx) const
    {
        if (idx < 0 || idx > now || idx < now - (int64_t) (kRing - 2)) return 0.f;
        return monoRing[(size_t) (idx & kRingMask)];
    }

    // median-smoothed pitch around input time tAn
    PitchInfo pitchAt (int64_t tAn) const
    {
        const double spanMs = (P.hq ? 14.0 : 6.0) * (P.fast ? 0.7 : 1.0);
        const int64_t span = (int64_t) (spanMs * 0.001 * fs);
        float vals[48];
        int nv = 0, nt = 0;
        const int64_t avail = std::min<int64_t> (trackW, kTrackSize);

        for (int64_t k = 0; k < avail; ++k)
        {
            const TrackEntry& e = track[(size_t) ((trackW - 1 - k) & kTrackMask)];
            if (e.time < tAn - span) break;
            if (e.time > tAn + span) continue;
            ++nt;
            if (e.voiced && nv < 48) vals[nv++] = e.midi;
        }

        PitchInfo pi;
        if (nt == 0 || nv * 2 <= nt) return pi;
        std::sort (vals, vals + nv);
        pi.voiced = true;
        pi.midi   = vals[nv / 2];
        return pi;
    }

    // waveform-similarity search so grains are cut at consistent phase of the voice cycle
    int64_t refineMark (int64_t prev, int64_t pred, double T0)
    {
        const int seg     = std::max (16, (int) T0);
        const int halfSeg = seg / 2;
        const int searchR = std::max (2, (int) (T0 * 0.25));

        int nRef = 0;
        double refE = 1.0e-9;
        for (int j = -halfSeg; j < halfSeg; j += 2)
        {
            const float a = monoAt (prev + j);
            refSeg[(size_t) nRef++] = a;
            refE += (double) a * a;
        }

        double bestScore = -1.0e30;
        int64_t best = pred;
        for (int s = -searchR; s <= searchR; ++s)
        {
            double num = 0.0, en = 1.0e-9;
            int k = 0;
            for (int j = -halfSeg; j < halfSeg; j += 2, ++k)
            {
                const float b = monoAt (pred + s + j);
                num += (double) refSeg[(size_t) k] * b;
                en  += (double) b * b;
            }
            const double score = num / std::sqrt (en * refE) - 0.05 * (double) std::abs (s) / (double) searchR;
            if (score > bestScore) { bestScore = score; best = pred + s; }
        }
        return best;
    }

    void advanceMark (double tDes, double T0, bool voiced)
    {
        if (! markValid || std::abs (tDes - (double) mark) > 6.0 * T0)
        {
            mark = (int64_t) std::llround (tDes);
            markValid = true;
            return;
        }
        for (int guard = 0; guard < 16; ++guard)
        {
            const int64_t pred = mark + (int64_t) std::llround (T0);
            if (std::abs ((double) pred - tDes) >= std::abs ((double) mark - tDes)) break;
            mark = voiced ? refineMark (mark, pred, T0) : pred;
        }
    }

    void placeGrain (int chs)
    {
        const int64_t c   = (int64_t) std::llround (nextC);
        const double  tAn = nextC - (double) L;
        const PitchInfo pi = pitchAt ((int64_t) std::llround (tAn));

        double T0    = (double) tu;
        double ratio = 1.0;

        if (pi.voiced)
        {
            const float actual = pi.midi;
            const double f0 = (double) P.refA * std::exp2 (((double) actual - 69.0) / 12.0);
            T0 = std::clamp (fs / f0, fs / (double) kMaxF0, fs / (double) kMinF0);
            const float dt = (float) (T0 / fs);

            const bool onset = (! wasVoiced);
            const float tcCenter = P.fast ? 0.070f : 0.030f;
            if (onset) { center = actual; targetNote = kNoTarget; }
            else       center += (actual - center) * (1.f - std::exp (-dt / tcCenter));

            targetNote = nearestAllowed (center, P.key, P.scale, targetNote, P.fast ? 0.35f : 0.20f);

            // output pitch = target + humanize * (natural deviation from the note centre)
            float shift = ((float) targetNote - center) - (1.f - P.humanize) * (actual - center);
            shift *= P.amount;

            if (onset)                    corr = (P.speedMs < 20.f) ? shift : 0.f;
            else if (P.speedMs <= 0.5f)   corr = shift;
            else                          corr += (shift - corr) * (1.f - std::exp (-dt * 1000.f / P.speedMs));
            corr = std::clamp (corr, -12.f, 12.f);

            wasVoiced  = true;
            gapSamples = 0.0;
            ratio      = std::exp2 ((double) corr / 12.0);
            lastVoiced = true; lastRaw = actual; lastOut = actual + corr;
        }
        else
        {
            gapSamples += T0;
            if (gapSamples > 0.030 * fs) { wasVoiced = false; lastVoiced = false; }
        }

        advanceMark (tAn, T0, pi.voiced);

        // ---- cut one Hann grain (2 periods) at the mark and add it at the output position
        const int    half = std::max (4, (int) std::lround (T0));
        const int    len  = 2 * half;
        const double r    = formantRatio;
        const float  w0   = 2.f * kPi / (float) len;

        for (int i = 0; i < len; ++i)
        {
            const float   w  = 0.5f - 0.5f * std::cos (w0 * ((float) i + 0.5f));
            const int64_t o  = c - half + i;
            const double  sp = (double) mark + (double) (i - half) * r;
            const int64_t i0 = (int64_t) std::floor (sp);
            const float   fr = (float) (sp - (double) i0);
            const size_t  oi = (size_t) (o & kAccMask);

            for (int ch = 0; ch < chs; ++ch)
                acc[ch][oi] += w * ((1.f - fr) * inAt (ch, i0) + fr * inAt (ch, i0 + 1));
            wsum[oi] += w;
        }

        nextC += T0 / ratio;
    }

    // ---- state
    double fs = 48000.0;
    int    nCh = 2, maxHalf = 640, tu = 336, L = 1760;
    bool   prepared = false;
    Params P;
    double formantRatio = 1.0;

    PitchTracker tracker;
    std::vector<float> inRing[2], monoRing, acc[2], wsum, refSeg;
    std::array<TrackEntry, kTrackSize> track {};
    int64_t trackW = 0, count = 0, now = -1, mark = 0;
    double  nextC = 0.0, gapSamples = 1.0e9;
    bool    markValid = false, wasVoiced = false, lastVoiced = false;
    float   center = 0.f, corr = 0.f, lastRaw = 0.f, lastOut = 0.f;
    int     targetNote = kNoTarget;
};

} // namespace sonytune
