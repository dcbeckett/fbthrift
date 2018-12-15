add_subdirectory(if)

# Perf cpp2 tests depend on rsocket and proxygen
if(rsocket_FOUND AND proxygen_FOUND)
    add_library(
        perf-common
        util/Util.cpp
    )

    target_link_libraries(
        perf-common
        thriftcpp2
        Api-cpp2
        StreamApi-cpp2
    )

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
