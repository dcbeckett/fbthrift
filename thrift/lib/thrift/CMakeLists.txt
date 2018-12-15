# Set the lib directory
set(LIB_THRIFT_HOME ${CMAKE_CURRENT_SOURCE_DIR})

# All of these generated thrift libraries will be included in either
# thriftcpp2, thriftfrozen2, or thriftprotocol (which are all exported
# libraries), so we compile them as object libraries.
add_thrift_library(
    RpcMetadata-cpp2 OBJECT
    RpcMetadata.thrift
    LANGUAGE cpp2
    OPTIONS templates
    GENERATED_INCLUDE_PREFIX thrift/lib/thrift
    INSTALL
)

add_thrift_library(
    reflection-cpp2 OBJECT
    reflection.thrift
    LANGUAGE cpp2
    OPTIONS templates
    GENERATED_INCLUDE_PREFIX thrift/lib/thrift
    INSTALL
)

add_thrift_library(
    frozen-cpp2 OBJECT
    frozen.thrift
    LANGUAGE cpp2
    GENERATED_INCLUDE_PREFIX thrift/lib/thrift
    INSTALL
)
