#include <cassert>
#include <fstream>
#include <memory>

class LogStorage/*: public Persistence*/ {
private:
    std::shared_ptr<std::ofstream> outLog;
    std::shared_ptr<std::ifstream> inLog;

public:
    LogStorage(const std::string& fileName) {
        outLog = std::shared_ptr<std::ofstream>(new std::ofstream (
                    fileName, std::ofstream::out | std::ofstream::app));
        inLog = std::shared_ptr<std::ifstream>(new std::ifstream (
                    fileName, std::ifstream::in));
        assert(outLog->good());
        assert(inLog->good());
    }
    // TODO: Possibly use char * for key value pairs for good IO performance?
    uint64_t write(const std::string& key, const std::string& value) {
        outLog->write(key.data(), key.size());
        uint64_t offset = (uint64_t) outLog->tellp();
        outLog->write(value.data(), value.size());
        return offset;
    }
    /*
    char * read(const std::string& key, const uint64_t& offset, const uint64_t& size) {
        char *buffer = new char [size];
        inLog.seekg(offset, inLog.beg);
        inLog.read(buffer, size);
        return buffer;
    }
    void readAll() {}
    */
    std::shared_ptr<std::ofstream> getOutStream() {
        return outLog;
    }
    std::shared_ptr<std::ifstream> getInStream() {
        return inLog;
    }
};
