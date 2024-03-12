add_library(SHAMap)
target_include_directories(SHAMap PUBLIC src)
target_compile_features(SHAMap PUBLIC cxx_std_20)

target_link_libraries(
  SHAMap
  PUBLIC Boost::boost
  PUBLIC Boost::system
  PUBLIC OpenSSL::Crypto
  PUBLIC OpenSSL::SSL
  PUBLIC xrpl::libxrpl
  INTERFACE Threads::Threads
)

target_sources(
  SHAMap
  PRIVATE src/shamap/impl/SHAMap.cpp
          # src/shamap/impl/SHAMapDelta.cpp
          src/shamap/impl/SHAMapInnerNode.cpp
          src/shamap/impl/SHAMapLeafNode.cpp
          src/shamap/impl/SHAMapNodeID.cpp
          # src/shamap/impl/SHAMapSync.cpp
          src/shamap/impl/SHAMapTreeNode.cpp
)
