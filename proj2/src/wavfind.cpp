
#include <dirent.h>
#include <fstream>
#include <iostream>
#include <map>
#include <sndfile.hh>
#include <vector>

#include "headers/io.h"
#include "headers/vctQuant.h"

#define DEBUG 0

using namespace std;

void parseArguments(int argc, char* argv[], SndfileHandle& sndFileIn,
                    int& blockSize, float& overlap_factor, string& cbDir) {
    checkFileToRead(sndFileIn, argv[argc - 4]);
    int sndFileSize = sndFileIn.frames();

    // validate block size
    try {
        blockSize = stoi(argv[argc - 3]);
    } catch (...) {
        cerr << "Error: block size must be a valid number" << endl;
        exit(1);
    }
    if (blockSize <= 0) {
        cerr << "Error: block size must be larger than zero" << endl;
        exit(1);
    }
    if (blockSize > sndFileSize) {
        cerr << "Error: block size must be smaller than " << sndFileSize
             << endl;
        exit(1);
    }

    // validate overlap factor
    try {
        overlap_factor = stof(argv[argc - 2]);
    } catch (...) {
        cerr << "Error: overlap factor must be a valid number" << endl;
        exit(1);
    }
    if (overlap_factor < 0 || overlap_factor > 1) {
        cerr << "Error: overlap factor must be a number between 0 and 1"
             << endl;
        exit(1);
    }

    cbDir = argv[argc - 1];
}

int readCodeBook(string& filename, size_t blockSizeParam,
                 vector<vector<short>>& codeBook,
                 vector<vector<short>>& blocks);

int main(int argc, char* argv[]) {
    if (argc != 5) {
        cerr << "Usage: wavfind <input file> <block size> <overlap "
                "factor> <codebook directory>"
             << endl;
        return 1;
    }

    // parse and validate arguments
    SndfileHandle sndFileIn{argv[argc - 4]};
    int blockSize;
    float overlapFactor;
    string cbDir{};
    parseArguments(argc, argv, sndFileIn, blockSize, overlapFactor, cbDir);

    // retrieve all blocks
    vector<vector<short>> blocks;
    retrieveBlocks(blocks, sndFileIn, blockSize, overlapFactor);

    struct dirent* entry = nullptr;
    DIR* dp = nullptr;

    double minimum{};
    map<string, double> errors{};
    vector<vector<short>> codeBook;
    dp = opendir(cbDir.c_str());
    if (dp != nullptr) {
        while ((entry = readdir(dp)) != nullptr) {
            if (entry->d_type == DT_REG) {
                string name = entry->d_name;
                string filename = cbDir + "/" + name;

                if (readCodeBook(filename, blockSize, codeBook, blocks) != 0) {
                    continue;
                }

                // get error from blocks vs codeBook
                double er = calculateError(blocks, codeBook);
                minimum = er;
                errors.insert({entry->d_name, er});
                codeBook.clear();
            }
        }
    }

    string response{};
    for (auto& pair : errors) {
        if (pair.second < minimum) {
            response = pair.first;
            minimum = pair.second;
        }
    }

    closedir(dp);

    cout << response << ", " << minimum << endl;

    return 0;
}

#if DEBUG
int readCodeBook(string& filename, size_t blockSizeParam,
                 vector<vector<short>>& codeBook,
                 vector<vector<short>>& blocks) {
    ifstream file(filename);

    string line;
    while (getline(file, line)) {
        vector<short>
            centroid;  // TODO this can be destroyed after leaving the function
        size_t pos = 0, blockSize = 0;
        while ((pos = line.find(',')) != std::string::npos) {
            centroid.push_back(short(stoi(line.substr(0, pos))));
            blockSize++;
            line.erase(0, pos + 1);
        }
        centroid.push_back(short(stoi(line)));

        if (++blockSize != blockSizeParam) {
            cerr << "WARNING code book \"" << filename
                 << "\" with centroids with block size of " << blockSize
                 << " instead of " << blockSizeParam << ". Skipping." << endl;

            codeBook.clear();

            return 1;
        }

        codeBook.push_back(centroid);
    }

    file.close();

    return 0;
}
#else
int readCodeBook(string& filename, size_t blockSizeParam,
                 vector<vector<short>>& codeBook,
                 vector<vector<short>>& blocks) {
    ifstream file(filename, fstream::binary);

    size_t numOfCentroids, blockSize;

    file.read((char*)&blockSize, sizeof(size_t));

    if (blockSize != blockSizeParam) {
        cerr << "WARNING code book \"" << filename
             << "\" with centroids with block size of " << blockSize
             << " instead of " << blockSizeParam << ". Skipping." << endl;

        return 1;
    }

    file.read((char*)&numOfCentroids, sizeof(size_t));

    short frame;
    for (size_t i = 0; i < numOfCentroids; i++) {  // for each centroid
        codeBook.emplace_back();
        for (size_t j = 0; j < blockSize;
             j++) {  // for each frame in the centroid
            file.read((char*)&frame, sizeof(short));

            codeBook[i].push_back(frame);
        }
    }

    file.close();

    return 0;
}
#endif
