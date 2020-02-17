# Key-value-store-739

## Contributors:
* Aditya Rungta
* Nisarg Shah

## Build instructions:

### Install dependencies

#### Install cmake >= 3.10

```
pushd /tmp
wget https://github.com/Kitware/CMake/releases/download/v3.16.4/cmake-3.16.4.tar.gz
tar -xf cmake-3.16.4.tar.gz
pushd cmake-3.16.4
./bootstrap -- -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/tmp/cmake
make -j$(cat /proc/cpuinfo | grep -c proc)
make install
popd
popd

export PATH=/tmp/cmake/bin/:$PATH
```

Verify cmake version
```
cmake --version
```

#### Install gcc (>= 7)
```
sudo apt-get install -y software-properties-common
sudo add-apt-repository ppa:ubuntu-toolchain-r/test
sudo apt update
sudo apt install g++-7 -y
```

#### Install other dependencies
```
sudo apt-get update && sudo apt-get install libtbb-dev
```

#### Build KV store along with its dependencies (gRPC, etc.)
```
cd cmake/build && cmake ../.. && make -j$(cat /proc/cpuinfo | grep -c proc)
```

## Execute server/client

### Run test script
The script starts 10 clients that perform 10k writes and reads. Run the following script from repo -
```
bash test_script.sh
```

### Run server
```
cd <repo>/cmake/build/keyvaluestore
./kvserver <path to log file>
```
For example, path to log file can be set to /tmp/log.txt

### Run client
```
./kvclient -s <server_addr:port> -e num_elems -n num_ops [-t threads=1] [-v val_size=512] [-w %_writes=0] [-l load_data=1]

-s: server IP address
-e: Initial number of elements to load into the store
-n: Number of read/write/update ops to execute
-t: Number of concurrent client threads running the operations
-v: Value size used for all writes/updates
-w: Write % for operations
-l: Set to 0 if not loading num_elems before executing ops
```

* Example: Load 1M KV pairs with 4kb values into the store
```
$ ./kvclient -s localhost:50051 -e 1000000 -n 0 -v 4096
Configuration of current run:
Percent writes:0%
Number of elements:10
Number of operations:0
Number of threads:1
Value Size:512
```

* Read-only workload with 1M ops on a store with 100k values
```
$ ./kvclient -s localhost:50051 -e 100000 -n 1000000 -l 0
Configuration of current run:
Percent writes:0%
Number of elements:10
Number of operations:100
Number of threads:1
Value Size:512
Read latency: 
Average latency: 219.98 us
Write latency: 
Throughput: 
Average throughput: 4511.96 ops/s
```

* 50% write workload (value size 1024 bytes) with 1M ops on store with 100k keys
```
./kvclient -s localhost:50051 -e 100000 -n 1000000 -l 0 -w 50 -v 1024
Configuration of current run:
Percent writes:50%
Number of elements:10
Number of operations:100
Number of threads:1
Value Size:1024
Read latency: 
Average latency: 203.739 us
Write latency: 
Average latency: 299.37 us
Throughput: 
Average throughput: 3885.48 ops/s
```
