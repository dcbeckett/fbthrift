add_thrift_library(
  ApiBase-cpp2
  ApiBase.thrift
  LANGUAGE cpp2
  GENERATED_INCLUDE_PREFIX thrift/perf/cpp2/if
)

add_thrift_library(
  Api-cpp2
  Api.thrift
  SERVICES Benchmark
  LANGUAGE cpp2
  OPTIONS process_in_event_base stream
  THRIFT_INCLUDE ${THRIFT_HOME}
  GENERATED_INCLUDE_PREFIX thrift/perf/cpp2/if
)
target_link_libraries(Api-cpp2 PRIVATE ApiBase-cpp2)

add_thrift_library(
  StreamApi-cpp2
  StreamApi.thrift
  SERVICES StreamBenchmark
  LANGUAGE cpp2
  OPTIONS stream
  THRIFT_INCLUDE ${THRIFT_HOME}
  GENERATED_INCLUDE_PREFIX thrift/perf/cpp2/if
)
target_link_libraries(StreamApi-cpp2 PRIVATE ApiBase-cpp2)
