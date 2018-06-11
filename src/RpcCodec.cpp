#include "RpcCodec.h"

#include "util/util.h"

namespace mcast {

Status DefaultRpcCodec::ReadRequestMessage(TcpConnectionBase* conn,
                                           rpc::RpcRequest* request) {
  uint32_t head = 0;
  Status status;
  if ((status = conn->Read(&head, 3))) {
    uint32_t msg_type = head & 0xFF;
    if (msg_type != kRequest)
      return Status(kInvailArgument, "Message type error");

    uint32_t pack_len = (head >> 8) & 0xFFFF;
    if (pack_len == 0)
      return Status(kInvailArgument, " Message length error");

    buffer_.resize(pack_len);
    if ((status = conn->Read(&buffer_[0], buffer_.size()))) {
      if (request->ParseFromArray(buffer_.data(),
                                  static_cast<int>(buffer_.size()))) {
        return Status::OK();
      } else {
        return Status(kFailed, "ParseFromArray failed");
      }
    }
  }

  return status;
}

Status DefaultRpcCodec::ReadResponeMessage(TcpConnectionBase* conn,
                                           rpc::RpcResponse* response) {
  uint32_t head = 0;
  Status status;
  if ((status = conn->Read(&head, 3))) {
    uint32_t msg_type = head & 0xFF;
    if (msg_type != KResponse)
      return Status(kInvailArgument, "Message type error");

    uint32_t pack_len = (head >> 8) & 0xFFFF;
    if (pack_len == 0)
      return Status(kInvailArgument, " Message length error");

    buffer_.resize(pack_len);
    if ((status = conn->Read(&buffer_[0], buffer_.size()))) {
      if (response->ParseFromArray(buffer_.data(),
                                   static_cast<int>(buffer_.size()))) {
        return Status::OK();
      } else {
        return Status(kFailed, "ParseFromArray failed");
      }
    }
  }

  return status;
}

Status DefaultRpcCodec::WriteMessage(TcpConnectionBase* conn, MessageType type,
                                     const google::protobuf::Message& msg) {
  buffer_.resize(3);
  buffer_[0] = static_cast<char>(type);
  if (msg.AppendToString(&buffer_)) {
    size_t pack_len = buffer_.size() - 3;
    CHECK_LT(pack_len, 0xFFFF);
    buffer_[1] = static_cast<char>(pack_len & 0xFF);
    buffer_[2] = static_cast<char>((pack_len >> 16) & 0xFF);

    if (auto s = conn->Write(buffer_.data(), buffer_.size())) {
      return Status::OK();
    } else {
      return s;
    }
  }
  return Status(kFailed, "AppendToString failed");
}

Status DefaultRpcCodec::WriteRequestMessage(TcpConnectionBase* conn,
                                            const rpc::RpcRequest& request) {
  return WriteMessage(conn, kRequest, request);
}

Status DefaultRpcCodec::WriteResponeMessage(TcpConnectionBase* conn,
                                            const rpc::RpcResponse& response) {
  return WriteMessage(conn, KResponse, response);
}

}  // namespace mcast
