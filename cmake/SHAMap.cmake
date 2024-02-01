
add_library (SHAMap)
target_include_directories (SHAMap PUBLIC src)
target_compile_features (SHAMap PUBLIC cxx_std_20)

target_link_libraries (SHAMap
  PUBLIC Boost::boost
  PUBLIC Boost::coroutine
  PUBLIC Boost::program_options
  PUBLIC Boost::system
  PUBLIC Boost::log
  PUBLIC Boost::log_setup
  PUBLIC Boost::stacktrace_backtrace
  PUBLIC cassandra-cpp-driver::cassandra-cpp-driver
  PUBLIC fmt::fmt
  PUBLIC OpenSSL::Crypto
  PUBLIC OpenSSL::SSL
  PUBLIC xrpl::libxrpl
  PUBLIC dl
  PUBLIC libbacktrace::libbacktrace

  INTERFACE Threads::Threads
)

target_sources (SHAMap PRIVATE
  src/shamap/impl/SHAMap.cpp
  src/shamap/impl/SHAMapDelta.cpp
  src/shamap/impl/SHAMapInnerNode.cpp
  src/shamap/impl/SHAMapLeafNode.cpp
  src/shamap/impl/SHAMapNodeID.cpp
  src/shamap/impl/SHAMapSync.cpp
  src/shamap/impl/SHAMapTreeNode.cpp
)
