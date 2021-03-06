/*
 *
 * Copyright 2018 gRPC authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#include <chrono>
#include <cstring>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <mutex>
#include <grpcpp/grpcpp.h>

#include "keyvaluestore.grpc.pb.h"
#include "logstorage.h"
#include "common.h"
#include "tbb/concurrent_hash_map.h"

using ::google::protobuf::Empty;
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerReaderWriter;
using grpc::ServerWriter;
using grpc::Status;
using keyvaluestore::KeyValueStore;
using keyvaluestore::KVPair;
using keyvaluestore::Request;
using keyvaluestore::Response;

typedef std::chrono::high_resolution_clock hrc;

hrc::time_point start, stop;

std::unique_ptr<LogStorage> log;

typedef tbb::concurrent_hash_map<std::string, kv_pair> hashtable; 

hashtable kv_store;

std::mutex mtx;

std::string get_value_from_map(const std::string& key) {
  hashtable::const_accessor a;
  bool isPresent = kv_store.find(a, key);
  if (isPresent) {
      // std::cout << "[Server] Found key: " << key
      //           << ", value: " << a->second.value << std::endl;
      return a->second.value;
  }
  a.release();
  // std::cout << "[Server] Did not find key: " << key << std::endl;
  return std::string("");
}

void set_value_in_map(const std::string& key, const std::string& value) {
    hashtable::accessor a;
    kv_store.insert(a, key);
    a->second = /*kv*/ {key, value};
    a.release();
}

// Logic and data behind the server's behavior.
class KeyValueStoreServiceImpl final : public KeyValueStore::Service{

  Status Get(ServerContext* context, const Request* request,
             Response* response) override {
    // std::cout << "[Server] Get" << std::endl;
    auto value = get_value_from_map(request->key());
    response->set_value(value);
    return Status::OK;
  }

  Status Set(ServerContext* context, const KVPair* kvPair,
             Empty* response) override {
    //std::cout << "[Server] Setting key: " << kvPair->key()
    //          << ", value: " << kvPair->value() << std::endl;
    mtx.lock();
    // std::cout << "[Server] Set" << std::endl;

    if (log->write(kvPair->key() , kvPair->value()) == -1) {
        std::cerr << "Set for key: " << kvPair->key() << " failed" << std::endl;
        return Status::CANCELLED;
    }
    // Flush explicitly to ensure persistence
    log->getOutStream()->flush();

    set_value_in_map(kvPair->key(), kvPair->value());
    mtx.unlock();
    return Status::OK;
  }

  Status GetPrefix(ServerContext* context, const Request* request, ServerWriter<Response>* writer) override {
    std::string prefixKey = request->key();	 
    for(hashtable::iterator it = kv_store.begin(); it != kv_store.end(); it++) {
      Response response;
      if(it->first.find(prefixKey) == 0) {
         response.set_value(it->second.value);
         writer->Write(response);
      }
    } 
    return Status::OK;
  }

};

void RunServer(const std::string& logFile) {
  std::string server_address("0.0.0.0:50051");
  KeyValueStoreServiceImpl service;

  ServerBuilder builder;
  // Listen on the given address without any authentication mechanism.
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  // Register "service" as the instance through which we'll communicate with
  // clients. In this case, it corresponds to an *synchronous* service.
  builder.RegisterService(&service);

  // Initialize Log and in-memory cache
  // Do this before assembling server
  log = std::unique_ptr<LogStorage>(new LogStorage(logFile));
  log->readAll(kv_store);

  // Finally assemble the server.
  std::unique_ptr<Server> server(builder.BuildAndStart());
  stop = hrc::now();

  std::cout << "Startup time: " << std::chrono::duration<double, std::milli>(
            stop-start).count() << " ms" << std::endl;
  std::cout << "Server listening on " << server_address << std::endl;

  // Wait for the server to shutdown. Note that some other thread must be
  // responsible for shutting down the server for this call to ever return.
  server->Wait();
}

int main(int argc, char** argv) {
  if (argc != 2) {
    std::cerr << "Usage: ./kvserver <path to log file>\n";
    return 1;
  }
  start = hrc::now();
  RunServer(std::string(argv[1]));

  return 0;
}
