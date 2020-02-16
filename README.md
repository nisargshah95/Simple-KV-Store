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
cd cmake/build && cmake ../.. && make`
```

