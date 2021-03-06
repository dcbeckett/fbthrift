add_library(
  thrift-core

  Thrift.cpp
)
target_include_directories(
  thrift-core
  INTERFACE
    $<INSTALL_INTERFACE:${INCLUDE_INSTALL_DIR}>
)
target_link_libraries(
  thrift-core
  PUBLIC
    Folly::folly
)

add_library(
  concurrency

  concurrency/Mutex.cpp
  concurrency/Monitor.cpp
  concurrency/PosixThreadFactory.cpp
  concurrency/ThreadManager.cpp
  concurrency/TimerManager.cpp
  concurrency/Util.cpp
)
target_link_libraries(
  concurrency
  PUBLIC
    Folly::folly
    ${LIBGFLAGS_LIBRARY}
    ${GLOG_LIBRARIES}
)

add_library(
  protocol

  protocol/TDebugProtocol.cpp
  protocol/TJSONProtocol.cpp
  protocol/TBase64Utils.cpp
  protocol/TProtocolException.cpp
  protocol/TSimpleJSONProtocol.cpp
  protocol/THeaderProtocol.cpp
  $<TARGET_OBJECTS:reflection-cpp2>
)

target_link_libraries(
  protocol
  PUBLIC
    Folly::folly
    ${GLOG_LIBRARIES}
)

add_library(
  transport

  transport/TTransportException.cpp
  transport/TFDTransport.cpp
  transport/THeaderTransport.cpp
  transport/THttpTransport.cpp
  transport/THttpClient.cpp
  transport/THttpServer.cpp
  transport/TSocket.cpp
  transport/TServerSocket.cpp
  transport/TBufferTransports.cpp
  transport/THeader.cpp
  transport/TZlibTransport.cpp
  util/FdUtils.cpp
  util/PausableTimer.cpp
  util/THttpParser.cpp
  util/VarintUtils.cpp
)
target_link_libraries(
  transport
  PUBLIC
    concurrency
    thrift-core
    Folly::folly
    ${OPENSSL_LIBRARIES}
    ${ZLIB_LIBRARIES}
    ${ZSTD_LIBRARIES}
)

add_library(
  async

  ContextStack.cpp
  EventHandlerBase.cpp
  async/TAsyncSocketFactory.cpp
  async/TAsyncSSLSocketFactory.cpp
  async/TBinaryAsyncChannel.cpp
  async/TFramedAsyncChannel.cpp
  async/THeaderAsyncChannel.cpp
  async/THttpAsyncChannel.cpp
  async/TUnframedAsyncChannel.cpp
  async/TZlibAsyncChannel.cpp
  server/TServerObserver.cpp
)
target_link_libraries(
  async
  PUBLIC
    concurrency
    transport
    ${Boost_LIBRARIES}
    Folly::folly
    ${GLOG_LIBRARIES}
    ${OPENSSL_LIBRARIES}
)

add_library(
  thrift

  VirtualProfiling.cpp
)
target_link_libraries(
  thrift
  PUBLIC
    async
    concurrency
    protocol
    transport
    Folly::folly
    ${GLOG_LIBRARIES}
)

set(THRIFT1_HEADER_DIRS
  async
  concurrency
  protocol
  server
  transport
)
foreach(dir ${THRIFT1_HEADER_DIRS})
  install(DIRECTORY ${dir} DESTINATION ${INCLUDE_INSTALL_DIR}/thrift/lib/cpp
          FILES_MATCHING PATTERN "*.h")
  install(DIRECTORY ${dir} DESTINATION ${INCLUDE_INSTALL_DIR}/thrift/lib/cpp
          FILES_MATCHING PATTERN "*.tcc")
endforeach()

install(
  TARGETS
    thrift-core
    concurrency
    protocol
    transport
    async
    thrift
  EXPORT fbthrift-exports
  DESTINATION ${LIB_INSTALL_DIR}
)

# Loadgen support (intentionally not installed)
add_library(
  thriftloadgen
  test/loadgen/Controller.cpp
  test/loadgen/LatencyMonitor.cpp
  test/loadgen/LatencyScoreBoard.cpp
  test/loadgen/LoadConfig.cpp
  test/loadgen/QpsMonitor.cpp
  test/loadgen/QpsScoreBoard.cpp
  test/loadgen/RNG.cpp
  test/loadgen/TerminalMonitor.cpp
  test/loadgen/WeightedLoadConfig.cpp
  test/loadgen/loadgen.cpp
)
target_link_libraries(
  thriftloadgen PUBLIC
  concurrency
  protocol
)
