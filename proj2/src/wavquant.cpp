#include <bitset>
#include <iostream>
#include <numeric>
#include <sndfile.hh>
#include <vector>

using namespace std;

constexpr size_t FRAMES_BUFFER_SIZE =
    65536;  // Buffer for reading/writing frames

constexpr int REDUCTFACTOR = 4;

int main(int argc, char *argv[]) {
    if (argc < 3) {
        cerr << "Usage: wavquant <quant size> <input file> <output file>"
             << endl;
        return 1;
    }

    SndfileHandle sndFileIn{argv[argc - 2]};
    if (sndFileIn.error()) {
        cerr << "Error: invalid input file" << endl;
        return 1;
    }

    if ((sndFileIn.format() & SF_FORMAT_TYPEMASK) != SF_FORMAT_WAV) {
        cerr << "Error: file is not in WAV format" << endl;
        return 1;
    }

    if ((sndFileIn.format() & SF_FORMAT_SUBMASK) != SF_FORMAT_PCM_16) {
        cerr << "Error: file is not in PCM_16 format" << endl;
        return 1;
    }

    SndfileHandle sndFileOut{argv[argc - 1], SFM_WRITE, sndFileIn.format(), 1,
                             sndFileIn.samplerate() / 4};
    if (sndFileOut.error()) {
        cerr << "Error: invalid output file" << endl;
        return 1;
    }

    int numQuantBits{stoi(argv[argc - 3])};
    if (numQuantBits <= 0 || numQuantBits > sizeof(short) * 8) {
        cerr << "Error: invalid quant. bits number, must be between 1 and 16"
             << endl;
        return 1;
    }

    size_t nFrames;
    size_t n{};
    short tmpFreq{};
    vector<short> sampleReduct;

    vector<short> samples(FRAMES_BUFFER_SIZE * sndFileIn.channels());
    vector<short> mySamples;
    while ((nFrames = sndFileIn.readf(samples.data(), FRAMES_BUFFER_SIZE))) {
        for (auto s : samples) {
            tmpFreq += s;
            if (++n % sndFileIn.channels() == 0) {
                tmpFreq /= sndFileIn.channels();
                tmpFreq = (tmpFreq & (-1 << sizeof(short) * 8 - numQuantBits));
                sampleReduct.push_back(tmpFreq);
                if (sampleReduct.size() >= REDUCTFACTOR) {
                    mySamples.push_back(
                        accumulate(begin(sampleReduct), end(sampleReduct), 0) /
                        REDUCTFACTOR);
                    sampleReduct.clear();
                }
                tmpFreq = 0;
            }
        }
        sndFileOut.writef(mySamples.data(), nFrames / REDUCTFACTOR);
        mySamples.clear();
    }

    return 0;
}
