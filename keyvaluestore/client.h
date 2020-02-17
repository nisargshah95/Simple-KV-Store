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

#pragma once

#include <iostream>
#include <memory>
#include <vector>

#include <grpcpp/grpcpp.h>

#include "keyvaluestore.grpc.pb.h"

using ::google::protobuf::Empty;
using grpc::Channel;
using grpc::ClientContext;
using grpc::ClientReader;
using grpc::Status;
using keyvaluestore::KeyValueStore;
using keyvaluestore::KVPair;
using keyvaluestore::Request;
using keyvaluestore::Response;

class KeyValueStoreClient {
 public:
  KeyValueStoreClient(std::shared_ptr<Channel> channel)
      : stub_(KeyValueStore::NewStub(channel)) {}

  std::string Get(const std::string& key) {
    // Context for the client. It could be used to convey extra information to
    // the server and/or tweak certain RPC behaviors.
    ClientContext context;
    Request request;
    request.set_key(key);
    Response response;

    Status status = stub_->Get(&context, request, &response);
    if (status.ok()) {
      //std::cout << key << " : " << response.value() << "\n";
      return response.value();
    } else {
      std::cout << status.error_code() << ": " << status.error_message()
                << std::endl;
      std::cout << "RPC failed" << std::endl;
      return std::string();
    }
  }

  bool Set(const std::string& key, const std::string& value) {
    // Context for the client. It could be used to convey extra information to
    // the server and/or tweak certain RPC behaviors.
    ClientContext context;
    KVPair request;
    request.set_key(key);
    request.set_value(value);
    Empty response;

    Status status = stub_->Set(&context, request, &response);
    if (status.ok()) {
      //std::cout << key << " inserted successfully.\n";
      return true;
    } else {
      std::cout << status.error_code() << ": " << status.error_message()
                << std::endl;
      std::cout << "RPC failed" << std::endl;
      return false;
    }
  }

  std::vector<std::string> GetPrefix(const std::string& prefixKey) {
    // Context for the client. It could be used to convey extra information to
    // the server and/or tweak certain RPC behaviors.
    ClientContext context;
    Request request;
    request.set_key(prefixKey);
    std::unique_ptr<ClientReader<Response> > reader(
    	stub_->GetPrefix(&context, request));
    Response response;

    std::vector<std::string> values;
    while (reader->Read(&response)) {
      	//std::cout << response.value() <<"\n";
        values.push_back(response.value());
    }
    Status status = reader->Finish();
    if (status.ok()) {
      std::cout << prefixKey << ": Prefix Key. Got the list of all expected values successfully.\n";
    } else {
      std::cout << status.error_code() << ": " << status.error_message()
                << std::endl;
      std::cout << "RPC failed" << std::endl;
    }

    return values;
  }
 

 private:
  std::unique_ptr<KeyValueStore::Stub> stub_;
};
