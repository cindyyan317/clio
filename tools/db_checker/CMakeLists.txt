set(go_files
  main.go
)

set(cgo_deps
  SHAMap
)

add_cgo_executable(db_checker go_files cgo_deps ${CMAKE_CURRENT_BINARY_DIR})
