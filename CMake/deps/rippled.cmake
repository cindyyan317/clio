target_link_directories(clio PUBLIC ${CONAN_LIB_DIRS_CLIO-XRPL})
target_link_libraries(clio PUBLIC ${CONAN_LIBS_CLIO-XRPL})
target_include_directories(clio PUBLIC ${CONAN_INCLUDE_DIRS_CLIO-XRPL})

target_link_directories(clio PUBLIC ${CONAN_LIB_DIRS_PROTOBUF})
target_link_libraries(clio PUBLIC ${CONAN_LIBS_PROTOBUF})
target_include_directories(clio PUBLIC  ${CONAN_INCLUDE_DIRS_PROTOBUF})

target_link_directories(clio PUBLIC ${CONAN_LIB_DIRS_LIBUV})
target_link_libraries(clio PUBLIC ${CONAN_LIBS_LIBUV})
target_include_directories(clio PUBLIC ${CONAN_INCLUDE_DIRS_LIBUV})

target_link_directories(clio PUBLIC ${CONAN_LIB_DIRS_C-ARES})
target_link_libraries(clio PUBLIC ${CONAN_LIBS_C-ARES})
target_include_directories(clio PUBLIC ${CONAN_INCLUDE_DIRS_C-ARES})

target_link_directories(clio PUBLIC ${CONAN_LIB_DIRS_ZLIB})
target_link_libraries(clio PUBLIC ${CONAN_LIBS_ZLIB})
target_include_directories(clio PUBLIC ${CONAN_INCLUDE_DIRS_ZLIB})

target_link_libraries(clio PUBLIC
  CONAN_PKG::abseil
  )

target_link_libraries(clio PUBLIC
  CONAN_PKG::grpc
  ) 

target_link_directories(clio PUBLIC ${CONAN_LIB_DIRS_RE2})
target_link_libraries(clio PUBLIC ${CONAN_LIBS_RE2})
target_include_directories(clio PUBLIC ${CONAN_INCLUDE_DIRS_RE2})

find_package(OpenSSL 1.1.1 REQUIRED)
set_target_properties(OpenSSL::SSL PROPERTIES
  INTERFACE_COMPILE_DEFINITIONS OPENSSL_NO_SSL2
)

target_link_libraries(clio PUBLIC
  OpenSSL::Crypto
  OpenSSL::SSL
)
