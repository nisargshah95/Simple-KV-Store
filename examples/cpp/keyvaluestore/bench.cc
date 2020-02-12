#include <chrono> 
#include <thread> 
#include <random>
#include <cassert>
#include <climits>
#include <cstdio> // remove (file)
#include <memory>
#include <unistd.h> // getopt

#include "client.h"

using namespace std;

//common stuff
constexpr auto maxThreads = 8;
using bench_type1 = std::function<void(bool, string)>;
vector<int64_t> latencies;

// constexpr auto options.value_size = 512;
//using valueType = std::array<char, options.value_size>;
using valueType = string;

// Global Definitions
//
// Datastructures defined here to ensure they stay in scope for the benchmark lambda
unique_ptr<KeyValueStoreClient> client;

string *keys;
bool *writes;
valueType value;

struct ConfigOptions {
    // TODO: Create some sequential key/value patterns.
    int threads = 1;
    int num_ops = 0;
    int ops_per_thread = 0;
    int num_elems = 0;
    int percent_writes = 0;
    int value_size = 512;
    bool Validate () {
        return ((threads >0) && (num_ops > 0)
             && (num_elems > 0));
    }
};

void PrintUsage () {
    cerr << "Error!/ Correct Usage: ./bench"
         << " -e num_elems -n num_ops -t threads[=1] -v val_size[=512] -w pc_writes[=0]\n" << endl;
}

/*
void GenerateZeroValue(valueType& value, int size) {
    //std::string str_value;
    //str_value.append(size, '0');
    value.append(size, '0');
    //std::copy(str_value.begin(), str_value.begin()+options.value_size, value.begin());
}
*/

void GenerateValue(valueType& value, int size) {
    //std::string str_value;
    //str_value.append(size, 'X');
    //std::copy(str_value.begin(), str_value.begin()+options.value_size, value.begin());
    value.append(size, 'X');
}

void PopulateKeysAndOps(const ConfigOptions& options, string *keys, bool *writes) {
    std::random_device device;
    int random_seed = device();
    std::mt19937 generator(random_seed);
    std::uniform_int_distribution<int> 
        distribution(0, options.num_ops);

    for (int i = 0; i < options.num_ops; i++) {
        keys[i] = to_string(i);
    }

    // Populate ops with reads/writes as per write_percent.
    std::uniform_int_distribution<int> percent_distribution(1, 100);
    for (int i = 0; i < options.num_ops; i++) {
        if (percent_distribution(generator) <= options.percent_writes)
            writes[i] = true;
        else 
            writes[i] = false;
    }

    // Debugging feature
    if (options.num_ops < 100) {
        cout << "Keys:" << endl;
        for (int i = 0; i < options.num_ops; i++) {
            cout << keys[i] << " ";
        }
        cout << endl;
        cout << "Operations:" << endl;
        for (int i = 0; i < options.num_ops; i++) {
            cout << (writes[i] ? "w" : "r") << " ";
        }
        cout << endl;
    }
}

void ThreadWork(int thread_id, int ops_per_thread,
        bench_type1 benchOne) {

    uint64_t thread_offset = thread_id*ops_per_thread;
    for (uint64_t i = 0; i < ops_per_thread; i++) {
        auto offset = thread_offset+i;
        auto key = keys[offset];
        auto write = writes[offset];

        std::chrono::time_point<std::chrono::high_resolution_clock> start, stop;
        start = std::chrono::high_resolution_clock::now();
        benchOne(write, key);
        stop = std::chrono::high_resolution_clock::now();

        auto latency = std::chrono::duration_cast<std::chrono::nanoseconds>
            (stop-start).count();
        latencies[offset] = latency;
    }
}

void RunBenchmark (const ConfigOptions& options) {

    std::thread workers[maxThreads];
    std::chrono::time_point<std::chrono::high_resolution_clock> start, stop;
    bench_type1 benchOne = NULL;
    std::function<void(void)> checker;
    keys = new string [options.num_ops];
    writes = new bool [options.num_ops];
    latencies.resize(options.num_elems + options.num_ops/2 + 1);

    PopulateKeysAndOps(options, keys, writes);

    // Cannot use valueType as value because hash function is not defined for it
    // Convert value to string before insertion
    GenerateValue(value, options.value_size);
    for (int i = 0; i < options.num_elems; i++) {
        // cout << "Inserting " << i << endl;
        // string value_str (value.begin(), value.end());
        if(!client->Set(keys[i], value)) {
            cerr << "Client set failed for key: " << keys[i] << endl;
        }
    }
    benchOne = [&](bool write, string key) {
        if (write) {
            // We want to insert new keys.
            // [0..num_elems] already in the map
            // cout << "Inserting " << options.num_elems+key << endl;
            key = key.append(to_string(options.num_elems));
            if(!client->Set(key, value)) {
                cerr << "Client set failed for key: " << key << endl;
            }
        } else {
            // cout << "Read " << key << endl;
            string value = client->Get(key);
            if (value.empty()) {
                cerr << "Error reading key: " << key << endl;
            }
            // assert((const valueType)(values[0]) == value);
        }
    };
    // checker = [&]() {
        // assert(immer_map->size() == options.num_ops);
    // };

    start = std::chrono::high_resolution_clock::now();
    for (int i=0; i < options.threads; i++) {
        workers[i] = std::thread(ThreadWork, i, options.ops_per_thread, benchOne);
    }
    for (int i=0; i < options.threads; i++) {
        workers[i].join();
    }
    stop = std::chrono::high_resolution_clock::now();

    cout << "Sorting latencies..." << endl;
    sort(latencies.begin(), latencies.end());
    cout << "99th percentile latency: " << latencies[99*latencies.size()/100] << endl;
    if (latencies.size() % 2 == 0)
        cout << "Median latency: " << (latencies[latencies.size()/2-1]
                                     + latencies[latencies.size()/2])/2 << endl;
    else
        cout << "Median latency: " << latencies[latencies.size()/2] << endl;

    auto latency = std::chrono::duration_cast<std::chrono::nanoseconds>
        (stop-start).count();
    cout << "Latency: " << latency << " ns\n";
    cout << "Throughput: " << (options.num_ops*1E9/latency) << " op/s\n";

}

bool GetInputArgs(int argc, char **argv, ConfigOptions& options) {
    int opt;
    while ((opt = getopt(argc, argv, "e:n:t:v:w")) != -1) {
        switch (opt) {
            case 'e':
                options.num_elems = atol(optarg);
                break;
            case 'n':
                options.num_ops = atol(optarg);
                break;
            case 't':
                options.threads = atoi(optarg);
                break;
            case 'v':
                options.value_size = atoi(optarg);
                break;
            case 'w':
                options.percent_writes = atoi(optarg);
                break;
            default:
                PrintUsage();
                return false;
        }
    }

    options.ops_per_thread = options.num_ops / options.threads;
    if (options.ops_per_thread * options.threads != options.num_ops) {
        cerr << "Number of operations not divisble by number of threads"
            << endl;
        return false;
    }
    return true;
}

void PrintInputArgs(const ConfigOptions& options) {
    cout << "Configuration of current run:" << endl;
    cout << "Percent writes:" << options.percent_writes << "%" << endl;
    cout << "Number of elements:" << options.num_elems << endl;
    cout << "Number of operations:" << options.num_ops << endl;
    cout << "Number of threads:" << options.threads << endl;
    cout << "Value Size:" << options.value_size << endl;
}

int main (int argc, char **argv)
{
    ConfigOptions options;
    if (!GetInputArgs(argc, argv, options)) {
        cerr << "Exiting!" << endl;
        return 1;
    }
    PrintInputArgs(options);

    if (!options.Validate()) {
        PrintUsage();
        cerr << "Exiting!" << endl;
        return 1;
    }

    client = unique_ptr<KeyValueStoreClient>(new KeyValueStoreClient(
                grpc::CreateChannel(
                    "localhost:50051", grpc::InsecureChannelCredentials())));

    RunBenchmark(options);

    return 0;
}

