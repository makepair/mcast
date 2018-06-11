#set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include_directories(.)
include_directories(/util)

add_library(mcast 
  util/Logging.cpp
  util/Timer.cpp
  util/socketops.cpp
  util/Thread.cpp
  libcontext.cpp
  ServiceEvent.cpp
  ServiceContext.cpp 
  Service.cpp
  TimerService.cpp  
  IOService.cpp
  TcpConnectionBase.cpp
  TcpConnection.cpp  
  TcpConnector.cpp 
	RpcCodec.cpp
  RpcServer.cpp
  RpcChannel.cpp
  WakeupService.cpp 
  System.cpp 
  Acceptor.cpp 
  TcpServer.cpp 
  rpc.pb.cc
  echo_service.pb.cc
)

add_executable(tests 
  util/LoggingTest.cpp 
  util/TimerTest.cpp
  util/ThreadTest.cpp  
  util/ThreadSafeQueueTest.cpp
  util/ObjectCacheTest.cpp
  util/StatusTest.cpp  
  util/MPSCQueueTest.cpp
  util/test_main.cpp
  SystemTest.cpp 
  TimerServiceTest.cpp 
  TcpConnectionTest.cpp 
  AcceptorTest.cpp
  TcpServerTest.cpp
  RpcServerTest.cpp
)

target_link_libraries (tests mcast protobuf gtest)

add_executable(pingpong_srv examples/pingpong/pingpong_server.cpp) 
target_link_libraries (pingpong_srv mcast protobuf)

add_executable(pingpong_cli examples/pingpong/pingpong_client.cpp) 
target_link_libraries (pingpong_cli mcast protobuf)

add_executable(tcp_echo_srv examples/tcp_echo/echo_srv.cpp) 
target_link_libraries (tcp_echo_srv mcast protobuf)

add_executable(rpc_srv examples/rpc/rpc_server.cpp) 
target_link_libraries (rpc_srv mcast protobuf)

add_executable(rpc_cli examples/rpc/rpc_client.cpp) 
target_link_libraries (rpc_cli mcast protobuf)

file(GLOB HEADERS "*.h")
install(FILES ${HEADERS} DESTINATION include/mcast)

file(GLOB HEADERS "util/*.h")
install(FILES ${HEADERS} DESTINATION include/mcast/util)

install(TARGETS mcast DESTINATION lib)


