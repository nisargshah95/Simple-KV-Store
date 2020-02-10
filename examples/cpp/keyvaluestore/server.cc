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

#include <cstring>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <grpcpp/grpcpp.h>

#ifdef BAZEL_BUILD
#include "examples/protos/keyvaluestore.grpc.pb.h"
#else
#include "keyvaluestore.grpc.pb.h"
#endif

#include "logstorage.h"
#include "common.h"

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

std::unique_ptr<LogStorage> log;

static const kv_pair kvs_map[] = {
    {"key1", "value1"}, {"key2", "value2"}, {"key3", "value3"},
    {"key4", "value4"}, {"key5", "value5"},
};

// TODO: We can use a computed hash instead of string here
// to improve lookup performance
std::map<std::string, kv_pair> kv_store = {
    {"key2", {"key2", "val2"}}
};

std::string get_value_from_map(const std::string& key) {
  /*for (size_t i = 0; i < sizeof(kvs_map) / sizeof(kv_pair); ++i) {
    if (strcmp(key, kvs_map[i].key) == 0) {
      return kvs_map[i].value;
    }
  }*/
  auto it = kv_store.find(key);
  if (it != kv_store.end()) {
      std::cout << "[Server] Found key: " << key
                << ", value: " << it->second.value << std::endl;
      return it->second.value;
  }
  std::cout << "[Server] Did not find key: " << key << std::endl;
  return std::string("");
}

void set_value_in_map(const std::string& key, const std::string& value) {
    std::cout << "[Server] Setting key: " << key
              << ", value: " << value << std::endl;
    kv_store[key] = /*kv*/ {key, value};
}

// Logic and data behind the server's behavior.
class KeyValueStoreServiceImpl final : public KeyValueStore::Service {
  Status GetValues(ServerContext* context,
                   ServerReaderWriter<Response, Request>* stream) override {
    Request request;
    while (stream->Read(&request)) {
      Response response;
      response.set_value(get_value_from_map(request.key()));
      stream->Write(response);
    }
    return Status::OK;
  }

  Status Get(ServerContext* context, const Request* request,
             Response* response) override {
    std::cout << "[Server] Get" << std::endl;
    auto value = get_value_from_map(request->key());
    response->set_value(value);
    return Status::OK;
  }

  Status Set(ServerContext* context, const KVPair* kvPair,
             Empty* response) override {
    std::cout << "[Server] Set" << std::endl;

    log->write(kvPair->key() , kvPair->value());
    // Flush explicitly to ensure persistence
    log->getOutStream()->flush();

    set_value_in_map(kvPair->key(), kvPair->value());
    return Status::OK;
  }

  Status GetPrefix(ServerContext* context, const Request* request, ServerWriter<Response>* writer) override {
    std::string prefixKey = request->key();	  
    for(auto x : kv_store) {
	Response response;
	if(x.first.find(prefixKey) == 0) {
	   response.set_value(x.second.value);
	   writer->Write(response);
	}
    }
    return Status::OK;
  }

};

void RunServer() {
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
  log = std::unique_ptr<LogStorage>(new LogStorage("/tmp/log.txt"));
  log->readAll(kv_store);

  // Finally assemble the server.
  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "Server listening on " << server_address << std::endl;

  // Wait for the server to shutdown. Note that some other thread must be
  // responsible for shutting down the server for this call to ever return.
  server->Wait();
}

int main(int argc, char** argv) {
  RunServer();

  return 0;
}
