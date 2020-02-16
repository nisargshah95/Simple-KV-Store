#include <chrono> 
#include <thread> 
#include <random>
#include <cassert>
#include <cstdlib>     /* srand, rand */
#include <fstream>
#include <time.h>       /* time */
#include <memory>
#include <unistd.h> // getopt
#include <vector>

#include "client.h"

using namespace std;


unique_ptr<KeyValueStoreClient> client;
ofstream results ("results.txt");

string *keys;
string *values;
char *writes;
int *updates;

// thread local read and write latencies
vector< vector<uint64_t> >r_latencies;
vector< vector<uint64_t> >w_latencies;
vector<double> throughputs;

struct ConfigOptions {
    int threads = 1;
    int num_ops = 0;
    int ops_per_thread = 0;
    int num_elems = 0;
    int percent_writes = 0;
    int value_size = 512;
    int load = 1;
    string server_addr;
    bool Validate () {
        return ((threads >0) && (num_ops >= 0)
             && (num_elems >= 0) && (!server_addr.empty()));
    }
};

void PrintUsage () {
    cerr << "Usage: ./bench -s <server_addr:port> -e num_elems -n num_ops [-t threads=1] "
        << "[-v val_size=512] [-w %_writes=0] [-l load_data=1]" << endl;
}

void GenerateValue(string& value, int size) {
    //std::string str_value;
    //str_value.append(size, 'X');
    //std::copy(str_value.begin(), str_value.begin()+options.value_size, value.begin());
    value.append(size, 'X');
}

void GeneratePaddedStr(string& s, int i, int padding) {
    string i_str = to_string(i);
    s.append(padding-i_str.size(), '0');
    s.append(i_str.data(), i_str.size());
    // cout << "generated str: " << s << endl;
}

void PopulateKeysAndValuesAndOps(const ConfigOptions& options) {

    std::random_device device;
    int random_seed = device();
    std::mt19937 generator(random_seed);

    for (int i = 0; i < options.num_elems + options.num_ops; i++) {
        GeneratePaddedStr(keys[i], i, 128);
        GeneratePaddedStr(values[i], i, options.value_size);
    }

    srand (time(NULL));
    // Populate ops with reads/writes as per write_percent.
    std::uniform_int_distribution<int> percent_distribution(1, 100);
    for (int i = 0; i < options.num_ops; i++) {
        if (percent_distribution(generator) <= options.percent_writes) {
            if (rand() % 100 < 50)
                writes[i] = 'w'; // writes
            else
                writes[i] = 'u'; // updates
        }
        else 
            writes[i] = 'r';
    }

    // indices to keys that will be updated
    srand (time(NULL));
    for (int i = 0; i < options.num_ops; i++) {
        updates[i] = rand() % options.num_elems;
    }

    // Debugging feature
    if (options.num_ops < 100 && options.num_ops > 0) {
        cout << "Keys:" << endl;
        for (int i = 0; i < options.num_ops; i++) {
            cout << keys[i] << " ";
        }
        cout << endl;
        cout << "Operations:" << endl;
        for (int i = 0; i < options.num_ops; i++) {
            cout << writes[i] << " ";
        }
        cout << endl;
    }
}

void bench(char write, string key, const string& value, int num_elems) {
    if (write == 'r') {
        // cout << "Read " << key << endl;
        string resp = client->Get(key);
        if (resp.empty()) {
            cerr << "Error reading key: " << key << endl;
        }
        // assert((const valueType)(values[0]) == value);
    } else {
        // We want to insert new keys.
        // [0..num_elems] already in the map
        // cout << "Inserting " << options.num_elems+key << endl;
        if (!client->Set(key, value)) {
            cerr << "Client set failed for key: " << key << endl;
        }
    }
};

void computeLatency(/*int thread_id,*/ const vector< vector<uint64_t> >& latencies) {
    
    //cout << "Sorting latencies..." << endl;
    // sort(latencies.begin(), latencies.end());
    // results[thread_id] << "99th percentile latency: "
    //     << latencies[99*latencies.size()/100] << " us\n";

    // if (latencies.size() % 2 == 0)
    //     results[thread_id] << "Median latency: " << (latencies[latencies.size()/2-1]
    //                                  + latencies[latencies.size()/2])/2 << " us\n";
    // else
    //     results[thread_id] << "Median latency: " << latencies[latencies.size()/2] << " us\n";

    double avg = 0;
    uint64_t size = 0;
    for (auto lat : latencies) {
        if (lat.empty()) continue;
        for (auto n : lat) {
            avg += n;
        }
        size += lat.size();
    }
    if (size == 0) return;
    avg /= size;
    // results[thread_id] << "Average latency: " << avg << " us\n";
    cout << "Average latency: " << avg << " us" << endl;
}

void computeThroughput(const vector<double>& throughputs) {
    double avg = 0;
    for (auto t : throughputs) {
        avg += t;
    }
    avg /= throughputs.size();
    cout << "Average throughput: " << avg << " ops/s" << endl;
}

void ThreadWork(int thread_id, const ConfigOptions& options) {
    // results[thread_id] = ofstream(string("results").append(to_string(thread_id)));
    // assert(results[thread_id].good());

    int num_elems = options.num_elems, num_ops = options.num_ops,
        ops_per_thread = options.ops_per_thread;

    r_latencies[thread_id].reserve(num_elems + num_ops);
    w_latencies[thread_id].reserve(num_elems + num_ops);
    srand (time(NULL));

    // start clock for measuting throughput
    auto start = std::chrono::high_resolution_clock::now();

    uint64_t thread_offset = thread_id*ops_per_thread;
    for (uint64_t i = 0; i < ops_per_thread; i++) {
        auto offset = thread_offset+i;
        auto write = writes[offset];
        string key, value;

        if (write == 'r' || write == 'u') {
            // can also use index in updates for random reads
            key = keys[updates[offset]];
            value = values[updates[offset]];
        }
        else if (write == 'w') {
            key = keys[num_elems + offset];
            value = values[num_elems + offset];
        }

        std::chrono::time_point<std::chrono::high_resolution_clock> start, stop;
        // start clock for measuting latency
        start = std::chrono::high_resolution_clock::now();
        bench(write, key, value, num_elems);
        // stop clock for measuting latency
        stop = std::chrono::high_resolution_clock::now();

        auto latency = std::chrono::duration_cast<std::chrono::microseconds>
            (stop-start).count();
        if (write == 'r') {
            r_latencies[thread_id].push_back(latency);
        } else {
            w_latencies[thread_id].push_back(latency);
        }
    }

    // stop clock for measuting throughput
    auto stop = std::chrono::high_resolution_clock::now();
    auto latency = std::chrono::duration_cast<std::chrono::nanoseconds>
        (stop-start).count();
    //cout << "Latency: " << latency << " ns\n";
    // results[thread_id] << "Throughput: " << (options.num_ops*1E9/latency) << " op/s\n";
    throughputs[thread_id] = options.num_ops*1E9/latency;
}

void RunBenchmark (const ConfigOptions& options) {

    std::thread workers[options.threads];
    keys = new string [options.num_elems + options.num_ops]; // was num_ops
    values = new string [options.num_elems + options.num_ops]; // was num_ops
    writes = new char [options.num_ops];
    updates = new int [options.num_ops];
    // results = unique_ptr<ofstream>(new ofstream);
    r_latencies.resize(options.threads);
    w_latencies.resize(options.threads);
    throughputs.resize(options.threads);

    PopulateKeysAndValuesAndOps(options);

    // initialize kv store
    //GenerateValue(value, options.value_size);
    if (options.load) {
        for (int i = 0; i < options.num_elems; i++) {
            // cout << "Inserting " << i << endl;
            // string value_str (value.begin(), value.end());
            if(!client->Set(keys[i], values[i])) {
                cerr << "Client set failed for key: " << keys[i] << endl;
            }
        }
    }

    // Don't run benchmark if we're only loading data
    if (options.num_ops == 0) return;

    for (int i=0; i < options.threads; i++) {
        workers[i] = std::thread(ThreadWork, i, options);
    }
    for (int i=0; i < options.threads; i++) {
        workers[i].join();
    }

    // results[thread_id] << "Read latency: " << endl;
    cout << "Read latency: " << endl;
    computeLatency(/*thread_id,*/ r_latencies);
    // results[thread_id] << "Write latency: " << endl;
    cout << "Write latency: " << endl;
    computeLatency(/*thread_id,*/ w_latencies);
    cout << "Throughput: " << endl;
    computeThroughput(throughputs);
}

bool GetInputArgs(int argc, char **argv, ConfigOptions& options) {
    int opt;
    while ((opt = getopt(argc, argv, "e:n:s:t:v:w:l:")) != -1) {
        switch (opt) {
            case 'e':
                options.num_elems = atol(optarg);
                break;
            case 'n':
                options.num_ops = atol(optarg);
                break;
            case 's':
                options.server_addr = string(optarg);
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
            case 'l':
                options.load = atoi(optarg);
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
                    options.server_addr, grpc::InsecureChannelCredentials())));

    RunBenchmark(options);

    return 0;
}
