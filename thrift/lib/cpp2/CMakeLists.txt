# Set the cpp2 directory
set(LIB_CPP2_HOME ${CMAKE_CURRENT_SOURCE_DIR})
set(THRIFT_TEMPLATES_DIR ${THRIFT_HOME}/thrift/compiler/generate/templates)


add_library(
  thriftfrozen2

  frozen/Frozen.cpp
  frozen/FrozenUtil.cpp
  frozen/schema/MemorySchema.cpp
  $<TARGET_OBJECTS:frozen-cpp2>
)

add_dependencies(thriftfrozen2 frozen-cpp2)
target_link_libraries(
  thriftfrozen2
  PUBLIC
    Folly::folly
    ${GLOG_LIBRARIES}
    ${LIBGFLAGS_LIBRARY}
)

add_library(
  thriftprotocol

  protocol/BinaryProtocol.cpp
  protocol/CompactProtocol.cpp
  protocol/CompactV1Protocol.cpp
  protocol/DebugProtocol.cpp
  protocol/JSONProtocolCommon.cpp
  protocol/JSONProtocol.cpp
  protocol/Serializer.cpp
  protocol/SimpleJSONProtocol.cpp
  protocol/VirtualProtocol.cpp
)
target_link_libraries(
  thriftprotocol
  PUBLIC
    thrift
    Folly::folly
    wangle::wangle
)

set(thriftcpp2_sources
  FrozenTApplicationException.cpp
  GeneratedCodeHelper.cpp
  async/AsyncClient.cpp
  async/AsyncProcessor.cpp
  async/Cpp2Channel.cpp
  async/DuplexChannel.cpp
  async/FramingHandler.cpp
  async/HeaderChannel.cpp
  async/HeaderChannelTrait.cpp
  async/HeaderClientChannel.cpp
  async/HeaderServerChannel.cpp
  async/PcapLoggingHandler.cpp
  async/RequestChannel.cpp
  async/ResponseChannel.cpp
  gen/field_ref.cpp
  server/BaseThriftServer.cpp
  server/Cpp2Connection.cpp
  server/Cpp2Worker.cpp
  server/ThriftServer.cpp
  server/peeking/TLSHelper.cpp
  transport/core/ThriftProcessor.cpp
  transport/core/ThriftClient.cpp
  transport/core/ThriftClientCallback.cpp
  util/ScopedServerInterfaceThread.cpp
  util/ScopedServerThread.cpp
)

set(thriftcpp2_objects $<TARGET_OBJECTS:RpcMetadata-cpp2>)

set(thriftcpp2_libraries
    thrift
    thriftfrozen2
    thriftprotocol
)


if(rsocket_FOUND)
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/transport/rsocket)
    add_thrift_library(
      Config-cpp2-obj OBJECT
      transport/rsocket/Config.thrift
      LANGUAGE cpp2
      OUTPUT_PATH ${CMAKE_CURRENT_BINARY_DIR}/transport/rsocket/
      GENERATED_INCLUDE_PREFIX "thrift/lib/cpp2/transport/rsocket"
    )

    # XXX: This library is not complete, it is the minimal amount of
    # TUs required to get perf/cpp2/client to compile.
    #
    # Known problems: RocketServerFrameContext.cpp requires at least Boost 1.67
    # for a variant overload.
    add_library(
      thriftcpp2_transport_rsocket
      OBJECT
        async/RocketClientChannel.cpp
        async/RSocketClientChannel.cpp
        transport/rsocket/server/RSResponder.cpp
        transport/rsocket/server/ManagedRSocketConnection.cpp
        transport/rsocket/server/RSThriftRequests.cpp
        transport/rsocket/server/RSRoutingHandler.cpp
        transport/rocket/Types.cpp
        transport/rocket/client/RequestContext.cpp
        transport/rocket/client/RequestContextQueue.cpp
        transport/rocket/client/RocketClient.cpp
        transport/rocket/client/RocketClientFlowable.cpp
        transport/rocket/framing/ErrorCode.cpp
        transport/rocket/framing/Frames.cpp
        transport/rocket/framing/Serializer.cpp
        transport/rocket/framing/Util.cpp
        transport/rocket/server/RocketServerConnection.cpp
    )
    add_dependencies(thriftcpp2_transport_rsocket Config-cpp2-obj)

    list(
      APPEND thriftcpp2_objects
        $<TARGET_OBJECTS:Config-cpp2-obj>
        $<TARGET_OBJECTS:thriftcpp2_transport_rsocket>
    )
    list(APPEND thriftcpp2_libraries rsocket::ReactiveSocket)
endif()

if (proxygen_FOUND)
    add_library(
        thriftcpp2_transport_h2
        OBJECT
          transport/http2/common/HTTP2RoutingHandler.cpp
          async/HTTPClientChannel.cpp
          server/proxygen/ProxygenThriftServer.cpp
          transport/http2/server/ThriftRequestHandler.cpp
          transport/http2/common/SingleRpcChannel.cpp
          transport/http2/common/HTTP2RoutingHandler.cpp
          transport/http2/client/ThriftTransactionHandler.cpp
          transport/http2/client/H2ClientConnection.cpp
          transport/http2/common/H2Channel.cpp
    )
    add_dependencies(thriftcpp2_transport_h2 RpcMetadata-cpp2)
    list(APPEND thriftcpp2_objects $<TARGET_OBJECTS:thriftcpp2_transport_h2>)
    list(APPEND thriftcpp2_libraries proxygen::proxygen)
endif()


add_library(
  thriftcpp2
  ${thriftcpp2_sources}
  ${thriftcpp2_objects}
)
target_link_libraries(
  thriftcpp2
  PUBLIC
  ${thriftcpp2_libraries}
)

# Install includes
set(THRIFT2_HEADER_DIRS
  async
  frozen
  gen-cpp2
  protocol
  server
  transport
  util
)
foreach(dir ${THRIFT2_HEADER_DIRS})
  install(DIRECTORY ${dir} DESTINATION ${INCLUDE_INSTALL_DIR}/thrift/lib/cpp2
          FILES_MATCHING PATTERN "*.h")
  install(DIRECTORY ${dir} DESTINATION ${INCLUDE_INSTALL_DIR}/thrift/lib/cpp2
          FILES_MATCHING PATTERN "*.tcc")
endforeach()

install(
  TARGETS
    thriftfrozen2
    thriftprotocol
    thriftcpp2
  EXPORT fbthrift-exports
  DESTINATION ${LIB_INSTALL_DIR}
)
