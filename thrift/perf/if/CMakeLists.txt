add_thrift_library(
  load-cpp2
  load.thrift
  LANGUAGE cpp2
  SERVICES LoadTest
  GENERATED_INCLUDE_PREFIX thrift/perf/if
  OPTIONS process_in_event_base
)
