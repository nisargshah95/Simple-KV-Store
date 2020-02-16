#pragma once

#include <cassert>
#include <experimental/filesystem>
#include <fstream>
#include <memory>
#include <map>

#include "common.h"
#include "tbb/concurrent_hash_map.h"

class LogStorage/*: public Persistence*/ {
private:
    std::shared_ptr<std::ofstream> outLog;
    std::shared_ptr<std::ifstream> inLog;
    std::experimental::filesystem::path logPath;

public:
    LogStorage(const std::string& filePath) {
        //checkInit(filePath);

        logPath = filePath;
        outLog = std::shared_ptr<std::ofstream>(new std::ofstream (
                    filePath,
                    std::ofstream::out | std::ofstream::app | std::ofstream::binary));
        inLog = std::shared_ptr<std::ifstream>(new std::ifstream (
                    filePath, std::ifstream::in | std::ofstream::binary));
        assert(outLog->good());
        assert(inLog->good());
    }

    uint64_t write(const std::string& key, const std::string& value) {

        // Write key size and value size
        size_t keySize = key.size();
        size_t valueSize = value.size();
        
        if (!(outLog->write((char *)(&keySize), sizeof(size_t)))) {
            std::cerr << "Write key size failed!\n";
            return -1;
        }
        if (!(outLog->write((char *)(&valueSize), sizeof(size_t)))) {
            std::cerr << "Write value size failed!\n";
            return -1;
        }

        // write key and get starting offset for value
        if (!(outLog->write(key.data(), keySize))) {
            std::cerr << "Write key failed!\n";
            return -1;
        }
        uint64_t offset = (uint64_t) outLog->tellp();

        // write value
        if (!(outLog->write(value.data(), valueSize))) {
            std::cerr << "Write value failed!\n";
            return -1;
        }
        return offset;
    }

    /*
     * Read all key-value pairs from disk
     * Used to re-construct map in memory after crash
     */
    void readAll(tbb::concurrent_hash_map<std::string, kv_pair>& kvStore) {
        std::cout << "[readAll]" << std::endl;

        // Seek to beginning if not already there
        inLog->seekg(0, inLog->beg);

        // Marks offset until which the contents of log are consistent and not corrupt
        uint64_t consistentOffset = 0;
        bool corruptBytes = false;

        while (!inLog->eof()) {

            // Read key and value size
            size_t keySize = 0;
            size_t valueSize = 0;

            if (!(inLog->read((char *)(&keySize), sizeof(size_t)))) {
                // TODO: Currently this will occur on every startup
                std::cerr << "Read key size failed!\n";
                corruptBytes = true;
                break;
            }
            if (!(inLog->read((char *)(&valueSize), sizeof(size_t)))) {
                std::cerr << "Read value size failed!\n";
                corruptBytes = true;
                break;
            }

            // std::cout << "key size: " << keySize
            //           << ", value size: " << valueSize << std::endl;

            // Now read key and value
            char *key = new char[keySize+1];
            char *value = new char[valueSize+1];

            if (!(inLog->read(key, keySize))) {
                std::cerr << "Read key size failed!\n";
                corruptBytes = true;
                break;
            }
            key[keySize] = 0; // read does not add null character at the end
            if (!(inLog->read(value, valueSize))) {
                std::cerr << "Read key size failed!\n";
                corruptBytes = true;
                break;
            }
            value[valueSize] = 0;

            consistentOffset += 2*sizeof(size_t) + keySize + valueSize;

            // std::cout << "key: " << key
            //           << ", value: " << value << std::endl;

            // TODO: convert char * -> string each time
            // Is there a way to use char * everywhere?
            std::string keyStr = std::string(key);
            tbb::concurrent_hash_map<std::string, kv_pair>::accessor a;
            kvStore.insert(a, keyStr);
            a->second = {keyStr, std::string(value)};
            a.release();
        }

        if (corruptBytes) {
            std::cerr << "Truncating log to " << consistentOffset << " bytes\n";
            std::experimental::filesystem::resize_file(logPath, consistentOffset);
        }
        std::cout << "Read " << kvStore.size() << " kv pairs" << std::endl;
    }

    std::shared_ptr<std::ofstream> getOutStream() {
        return outLog;
    }
    std::shared_ptr<std::ifstream> getInStream() {
        return inLog;
    }
};
