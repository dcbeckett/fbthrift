set(PERF_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(PERF_IF_DIR "${PERF_SOURCE_DIR}/if")

find_package(rsocket)
find_package(yarpl)

# Perf tests depend on rsocket, yarpl
if(rsocket_FOUND AND yarpl_FOUND)
    set(generated_root "${CMAKE_CURRENT_BINARY_DIR}/generated")

    set(thrift_generated_output_directory "${generated_root}/thrift/perf/cpp2/if")
    file(MAKE_DIRECTORY ${thrift_generated_output_directory})

    thrift_library2(
        FILE ApiBase
        FILE_DIRECTORY ${PERF_IF_DIR}
        OUTPUT_PATH ${thrift_generated_output_directory}
        LANGUAGE cpp2
        GENERATED_INCLUDE_PREFIX "thrift/perf/cpp2/if"
    )
    target_include_directories(ApiBase-cpp2-obj PUBLIC ${generated_root})

    thrift_library2(
        FILE Api
        FILE_DIRECTORY ${PERF_IF_DIR}
        OUTPUT_PATH ${thrift_generated_output_directory}
        SERVICES Benchmark
        LANGUAGE cpp2
        OPTIONS process_in_event_base stream
        THRIFT_INCLUDE ${THRIFT_HOME}
        GENERATED_INCLUDE_PREFIX "thrift/perf/cpp2/if"
    )
    add_dependencies(Api-cpp2-obj ApiBase-cpp2-obj)
    target_link_libraries(Api-cpp2 ApiBase-cpp2)
    target_include_directories(Api-cpp2-obj PUBLIC ${generated_root})

    thrift_library2(
        FILE StreamApi
        FILE_DIRECTORY ${PERF_IF_DIR}
        OUTPUT_PATH ${thrift_generated_output_directory}
        SERVICES StreamBenchmark
        LANGUAGE cpp2
        OPTIONS stream
        THRIFT_INCLUDE ${THRIFT_HOME}
        GENERATED_INCLUDE_PREFIX "thrift/perf/cpp2/if"
    )
    add_dependencies(StreamApi-cpp2-obj ApiBase-cpp2-obj)
    target_link_libraries(StreamApi-cpp2 ApiBase-cpp2)
    target_include_directories(StreamApi-cpp2-obj PUBLIC ${generated_root})


    add_library(
        perf-common
        "${PERF_SOURCE_DIR}/util/Util.cpp"
    )

    target_link_libraries(
        perf-common
        thriftcpp2
        Api-cpp2
        StreamApi-cpp2
    )

    target_include_directories(perf-common PUBLIC ${generated_root})

    add_executable(
        perf-cpp2-server
        server/Server.cpp
    )
    target_link_libraries(perf-cpp2-server perf-common)

    add_executable(
        perf-cpp2-client
        client/Client.cpp
    )
    target_link_libraries(perf-cpp2-client perf-common StreamApi-cpp2)

else()
    message("Skipping building perf due to missing dependencies")
endif()
