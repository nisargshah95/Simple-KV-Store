#include "client.h"

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
