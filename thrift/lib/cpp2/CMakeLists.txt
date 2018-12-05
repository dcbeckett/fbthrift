# Set the cpp2 directory
set(LIB_CPP2_HOME ${CMAKE_CURRENT_SOURCE_DIR})
set(THRIFT_TEMPLATES_DIR ${THRIFT_HOME}/thrift/compiler/generate/templates)


set(final_exported_targets
    thriftfrozen2
    thriftprotocol
    thriftcpp2
)
#######
# CMAKE variables only have local/subdirectory scope
# So even though this is defined in ../thrift/CMakeLists.txt as a variable
# it's not accessible here and silently defaults to ""
#
# Using global properties gets around this; probably better to restructure
# the directories/files to avoid this type of hack
#
get_property(RpcMetadata-cpp2-SOURCES GLOBAL PROPERTY RpcMetadata-cpp2-SOURCES)
get_property(frozen-cpp2-SOURCES GLOBAL PROPERTY frozen-cpp2-SOURCES)

bypass_source_check("${frozen-cpp2-SOURCES}")
add_library(
  thriftfrozen2

  frozen/Frozen.cpp
  frozen/FrozenUtil.cpp
  frozen/schema/MemorySchema.cpp
  ${frozen-cpp2-SOURCES}
)
add_dependencies(thriftfrozen2 frozen-cpp2-target)
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

bypass_source_check("${RpcMetadata-cpp2-SOURCES}")

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
  ${RpcMetadata-cpp2-SOURCES}
)

set(thriftcpp2_libraries
    thrift
    thriftfrozen2
    thriftprotocol
)


if(rsocket_FOUND)
    thrift_object2(
        FILE Config
        FILE_DIRECTORY ${LIB_CPP2_HOME}/transport/rsocket
        OUTPUT_PATH ${LIB_CPP2_HOME}/transport/rsocket
        LANGUAGE cpp2
        GENERATED_INCLUDE_PREFIX "thrift/lib/cpp2/transport/rsocket"
    )

    add_library(
		thriftcpp2_transport_rsocket
		OBJECT
			async/RSocketClientChannel.cpp
			transport/rsocket/server/RSResponder.cpp
			transport/rsocket/server/ManagedRSocketConnection.cpp
			transport/rsocket/server/RSThriftRequests.cpp
			transport/rsocket/server/RSRoutingHandler.cpp
    )
    add_dependencies(thriftcpp2_transport_rsocket Config-cpp2-obj)
	list(
		APPEND thriftcpp2_sources
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
 	list(APPEND thriftcpp2_sources "$<TARGET_OBJECTS:thriftcpp2_transport_h2>")
	list(APPEND thriftcpp2_libraries proxygen::proxygen)
endif()


add_library(
  thriftcpp2
  ${thriftcpp2_sources}
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
  TARGETS ${final_exported_targets}
  EXPORT fbthrift-exports
  DESTINATION ${LIB_INSTALL_DIR}
)
