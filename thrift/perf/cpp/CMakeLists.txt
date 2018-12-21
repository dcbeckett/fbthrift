add_library(
  async-load-handler
  AsyncLoadHandler2.cpp
)

add_library(
  utils
  Utils.cpp
)

target_link_libraries(
  async-load-handler
  load-cpp2
  thriftcpp2
  thriftloadgen
)

add_executable(
  loadgen
  AsyncClientWorker2.cpp
  ClientLoadConfig.cpp
  ClientWorker2.cpp
  loadgen.cpp
)
target_link_libraries(
  loadgen
  async-load-handler
  utils
)

add_executable(
  ThriftServer
  Cpp2Server.cpp
)
target_link_libraries(
  ThriftServer
  async-load-handler
  utils
)
