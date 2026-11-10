// Standalone harness: runs the SonyTune engine on a raw float32 file.
// usage: run_engine in.raw out.raw fs channels key scale speedMs amount humanize formant mix fast hq
#include "../Source/SonyTuneDSP.h"
#include <cstdio>
#include <cstdlib>
#include <vector>

int main (int argc, char** argv)
{
    if (argc < 14) { std::fprintf (stderr, "bad args\n"); return 1; }

    const double fs = std::atof (argv[3]);
    const int chans = std::atoi (argv[4]);

    sonytune::Engine::Params p;
    p.key = std::atoi (argv[5]);   p.scale = std::atoi (argv[6]);
    p.speedMs = (float) std::atof (argv[7]);  p.amount = (float) std::atof (argv[8]);
    p.humanize = (float) std::atof (argv[9]); p.formantSemis = (float) std::atof (argv[10]);
    p.mix = (float) std::atof (argv[11]);     p.fast = std::atoi (argv[12]) != 0;
    p.hq = std::atoi (argv[13]) != 0;

    FILE* f = std::fopen (argv[1], "rb");
    if (! f) return 2;
    std::fseek (f, 0, SEEK_END);
    const long bytes = std::ftell (f);
    std::fseek (f, 0, SEEK_SET);
    std::vector<float> data ((size_t) bytes / sizeof (float));
    if (std::fread (data.data(), sizeof (float), data.size(), f) != data.size()) return 3;
    std::fclose (f);

    const size_t frames = data.size() / (size_t) chans;
    std::vector<std::vector<float>> ch ((size_t) chans, std::vector<float> (frames));
    for (size_t i = 0; i < frames; ++i)
        for (int c = 0; c < chans; ++c) ch[(size_t) c][i] = data[i * (size_t) chans + (size_t) c];

    sonytune::Engine eng;
    eng.prepare (fs, chans);
    eng.setParams (p);
    std::printf ("latency %d\n", eng.getLatencySamples());

    const int block = 512;
    std::vector<float*> ptrs ((size_t) chans);
    for (size_t pos = 0; pos < frames; pos += (size_t) block)
    {
        const int n = (int) std::min<size_t> ((size_t) block, frames - pos);
        for (int c = 0; c < chans; ++c) ptrs[(size_t) c] = ch[(size_t) c].data() + pos;
        eng.process (ptrs.data(), chans, n);
    }

    for (size_t i = 0; i < frames; ++i)
        for (int c = 0; c < chans; ++c) data[i * (size_t) chans + (size_t) c] = ch[(size_t) c][i];

    f = std::fopen (argv[2], "wb");
    std::fwrite (data.data(), sizeof (float), data.size(), f);
    std::fclose (f);
    return 0;
}
