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

#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <grpcpp/grpcpp.h>

//#include "caching_interceptor.h"

#ifdef BAZEL_BUILD
#include "examples/protos/keyvaluestore.grpc.pb.h"
#else
#include "keyvaluestore.grpc.pb.h"
#endif

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

  // Requests each key in the vector and displays the key and its corresponding
  // value as a pair
  void GetValues(const std::vector<std::string>& keys) {
    // Context for the client. It could be used to convey extra information to
    // the server and/or tweak certain RPC behaviors.
    ClientContext context;
    auto stream = stub_->GetValues(&context);
    for (const auto& key : keys) {
      // Key we are sending to the server.
      Request request;
      request.set_key(key);
      stream->Write(request);

      // Get the value for the sent key
      Response response;
      stream->Read(&response);
      std::cout << key << " : " << response.value() << "\n";
    }
    stream->WritesDone();
    Status status = stream->Finish();
    if (!status.ok()) {
      std::cout << status.error_code() << ": " << status.error_message()
                << std::endl;
      std::cout << "RPC failed";
    }
  }

  void Get(const std::string& key) {
    // Context for the client. It could be used to convey extra information to
    // the server and/or tweak certain RPC behaviors.
    ClientContext context;
    Request request;
    request.set_key(key);
    Response response;

    Status status = stub_->Get(&context, request, &response);
    if (status.ok()) {
      std::cout << key << " : " << response.value() << "\n";
    } else {
      std::cout << status.error_code() << ": " << status.error_message()
                << std::endl;
      std::cout << "RPC failed" << std::endl;
    }
  }

  void Set(const std::string& key, const std::string& value) {
    // Context for the client. It could be used to convey extra information to
    // the server and/or tweak certain RPC behaviors.
    ClientContext context;
    KVPair request;
    request.set_key(key);
    request.set_value(value);
    Empty response;

    Status status = stub_->Set(&context, request, &response);
    if (status.ok()) {
      std::cout << key << " inserted successfully.\n";
    } else {
      std::cout << status.error_code() << ": " << status.error_message()
                << std::endl;
      std::cout << "RPC failed" << std::endl;
    }
  }

  void GetPrefix(const std::string& prefixKey) {
    // Context for the client. It could be used to convey extra information to
    // the server and/or tweak certain RPC behaviors.
    ClientContext context;
    Request request;
    request.set_key(prefixKey);
    std::unique_ptr<ClientReader<Response> > reader(
    	stub_->GetPrefix(&context, request));
    Response response;
    while (reader->Read(&response)) {
  	std::cout << response.value() <<"\n";
    }
    Status status = reader->Finish();
    if (status.ok()) {
      std::cout << prefixKey << ": Prefix Key. Got the list of all expected values successfully.\n";
    } else {
      std::cout << status.error_code() << ": " << status.error_message()
                << std::endl;
      std::cout << "RPC failed" << std::endl;
    }
  }
 

 private:
  std::unique_ptr<KeyValueStore::Stub> stub_;
};

int main(int argc, char** argv) {
  // Instantiate the client. It requires a channel, out of which the actual RPCs
  // are created. This channel models a connection to an endpoint (in this case,
  // localhost at port 50051). We indicate that the channel isn't authenticated
  // (use of InsecureChannelCredentials()).
  // In this example, we are using a cache which has been added in as an
  // interceptor.
  /*
  grpc::ChannelArguments args;
  std::vector<
      std::unique_ptr<grpc::experimental::ClientInterceptorFactoryInterface>>
      interceptor_creators;
  interceptor_creators.push_back(std::unique_ptr<CachingInterceptorFactory>(
      new CachingInterceptorFactory()));
  auto channel = grpc::experimental::CreateCustomChannelWithInterceptors(
      "localhost:50051", grpc::InsecureChannelCredentials(), args,
      std::move(interceptor_creators));
  KeyValueStoreClient client(channel);
  */
  KeyValueStoreClient client(grpc::CreateChannel(
       "localhost:50051", grpc::InsecureChannelCredentials()));
  std::vector<std::string> keys = {"key1", "skkey2", "key3", "key4",
                                   "key5", "key1", "key2", "key4"};
  //client.GetValues(keys);
  client.Set(keys[0], std::string("hello"));
  client.Get(keys[0]);

  client.Set(keys[1], std::string("world"));
  client.Get(keys[1]);

  client.Set(keys[2], std::string("grpc"));
  client.Get(keys[2]);

  client.Set(keys[3], std::string("world33"));
  client.Get(keys[3]);

  client.GetPrefix(std::string("key"));
  return 0;
}
