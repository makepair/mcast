#ifndef CAST_RPCCODEC_H_
#define CAST_RPCCODEC_H_

#include <stdint.h>
#include <memory>
#include <string>

#include "rpc.pb.h"

#include "TcpConnectionBase.h"
#include "util/Noncopyable.h"
#include "util/Status.h"

namespace mcast {

struct RpcCodec : public Noncopyable {
  enum MessageType { kInvaild, kRequest, KResponse };

  virtual ~RpcCodec() {}

  // virtual Status ReadMessage(TcpConnectionBase* conn,
  //                               MessageType *type,    rpc::RpcRequest* request) = 0;

  virtual Status ReadRequestMessage(TcpConnectionBase* conn,
                                    rpc::RpcRequest* request) = 0;

  virtual Status ReadResponeMessage(TcpConnectionBase* conn,
                                    rpc::RpcResponse* response) = 0;

  virtual Status WriteRequestMessage(TcpConnectionBase* conn,
                                     const rpc::RpcRequest& request) = 0;

  virtual Status WriteResponeMessage(TcpConnectionBase* conn,
                                     const rpc::RpcResponse& response) = 0;
};

typedef std::unique_ptr<RpcCodec> RpcCodecPtr;

struct DefaultRpcCodec : public RpcCodec {
  virtual ~DefaultRpcCodec() {}

  Status ReadRequestMessage(TcpConnectionBase* conn,
                            rpc::RpcRequest* request) override;

  Status ReadResponeMessage(TcpConnectionBase* conn,
                            rpc::RpcResponse* response) override;

  Status WriteRequestMessage(TcpConnectionBase* conn,
                             const rpc::RpcRequest& request) override;

  Status WriteResponeMessage(TcpConnectionBase* conn,
                             const rpc::RpcResponse& response) override;

 private:
  Status WriteMessage(TcpConnectionBase* conn, MessageType type,
                      const google::protobuf::Message& msg);

  std::string buffer_;
};

}  // namespace mcast

#endif  // CAST_RPCCODEC_H_