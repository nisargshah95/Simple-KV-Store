#pragma once

#include <cassert>
#include <fstream>
#include <memory>
#include <map>

#include "common.h"
#include "tbb/concurrent_hash_map.h"

class LogStorage/*: public Persistence*/ {
private:
    std::shared_ptr<std::ofstream> outLog;
    std::shared_ptr<std::ifstream> inLog;

public:
    LogStorage(const std::string& fileName) {
        //checkInit(fileName);

        outLog = std::shared_ptr<std::ofstream>(new std::ofstream (
                    fileName,
                    std::ofstream::out | std::ofstream::app | std::ofstream::binary));
        inLog = std::shared_ptr<std::ifstream>(new std::ifstream (
                    fileName, std::ifstream::in | std::ofstream::binary));
        assert(outLog->good());
        assert(inLog->good());
    }

    /*
     * On first run, reserve 2G space on disk for log file
     */
    /*
    void checkInit(const std::string& fileName) {
        std::ifstream f(fileName);
        if (f.good()) return;

        std::cout << "Running for the first time, initializing log..." << std::endl;
        uint64_t max_size = 1;
        max_size <<= 31; // 2G
        char *max_buf = new char [max_size];
        memset(max_buf, '\0', sizeof(char) * max_size);

        std::ofstream of (fileName, std::ofstream::out | std::ofstream::binary);
        of.write(max_buf, sizeof(char) * max_size);
        of.flush();
        std::cout << "Initialized log successfully" << std::endl;
    }
    */

    uint64_t write(const std::string& key, const std::string& value) {

        // Write key size and value size
        size_t keySize = key.size();
        size_t valueSize = value.size();
        outLog->write((char *)(&keySize), sizeof(size_t));
        outLog->write((char *)(&valueSize), sizeof(size_t));

        // write key and get starting offset for value
        outLog->write(key.data(), keySize);
        uint64_t offset = (uint64_t) outLog->tellp();

        // write value
        outLog->write(value.data(), valueSize);
        return offset;
    }

    /*
    char * read(const std::string& key, const uint64_t& offset, const uint64_t& size) {
        char *buffer = new char [size];
        inLog.seekg(offset, inLog.beg);
        inLog.read(buffer, size);
        return buffer;
    }
    */

    /*
     * Read all key-value pairs from disk
     * Used to re-construct map in memory after crash
     */
    void readAll(tbb::concurrent_hash_map<std::string, kv_pair>& kvStore) {
        std::cout << "[readAll]" << std::endl;

        // Seek to beginning if not already there
        inLog->seekg(0, inLog->beg);

        while (!inLog->eof()) {

            // Read key and value size
            size_t keySize = 0;
            size_t valueSize = 0;

            if (!(inLog->read((char *)(&keySize), sizeof(size_t)))) {
                std::cerr << "Read key size failed!\n";
                break;
            }
            if (!(inLog->read((char *)(&valueSize), sizeof(size_t)))) {
                std::cerr << "Read key size failed!\n";
                break;
            }

            std::cout << "key size: " << keySize
                      << ", value size: " << valueSize << std::endl;

            // Now read key and value
            char *key = new char[keySize+1];
            char *value = new char[valueSize+1];

            if (!(inLog->read(key, keySize))) {
                std::cerr << "Read key size failed!\n";
                break;
            }
            key[keySize] = 0; // read does not add null character at the end
            if (!(inLog->read(value, valueSize))) {
                std::cerr << "Read key size failed!\n";
                break;
            }
            value[valueSize] = 0;

            std::cout << "key: " << key
                      << ", value: " << value << std::endl;

            // TODO: convert char * -> string each time
            // Is there a way to use char * everywhere?
            std::string keyStr = std::string(key);
            tbb::concurrent_hash_map<std::string, kv_pair>::accessor a;
            kvStore.insert(a, keyStr);
            a->second = {keyStr, std::string(value)};
            a.release();
        }
    }

    std::shared_ptr<std::ofstream> getOutStream() {
        return outLog;
    }
    std::shared_ptr<std::ifstream> getInStream() {
        return inLog;
    }
};
