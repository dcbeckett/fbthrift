# Set the lib directory
set(LIB_THRIFT_HOME ${CMAKE_CURRENT_SOURCE_DIR})

add_thrift_library(
    RpcMetadata-cpp2
    ${LIB_THRIFT_HOME}/reflection.thrift
    LANGUAGE cpp2
    OPTIONS templates
    OUTPUT_PATH ${LIB_THRIFT_HOME}
    GENERATED_INCLUDE_PREFIX thrift/lib/thrift
    INSTALL
)

add_thrift_library(
    reflection-cpp2
    ${LIB_THRIFT_HOME}/reflection.thrift
    LANGUAGE cpp2
    OPTIONS templates
    OUTPUT_PATH ${LIB_THRIFT_HOME}
    GENERATED_INCLUDE_PREFIX thrift/lib/thrift
    INSTALL
)

add_thrift_library(
    frozen-cpp2
    ${LIB_THRIFT_HOME}/frozen.thrift
    LANGUAGE cpp2
    OUTPUT_PATH ${LIB_THRIFT_HOME}
    GENERATED_INCLUDE_PREFIX thrift/lib/thrift
    INSTALL
)

get_property(RpcMetadata-cpp2-SOURCES TARGET RpcMetadata-cpp2 PROPERTY SOURCES)
get_property(frozen-cpp2-SOURCES TARGET frozen-cpp2 PROPERTY SOURCES)
get_property(reflection-cpp2-SOURCES TARGET reflection-cpp2 PROPERTY SOURCES)
set_property(GLOBAL PROPERTY RpcMetadata-cpp2-SOURCES ${RpcMetadata-cpp2-SOURCES})
set_property(GLOBAL PROPERTY frozen-cpp2-SOURCES ${frozen-cpp2-SOURCES})
set_property(GLOBAL PROPERTY reflection-cpp2-SOURCES ${reflection-cpp2-SOURCES})
