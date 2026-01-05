set(SECRET_STORAGE_SOURCES
  secret-storage.c
  nondumpable-allocator.c
  compat/secure-mem-posix.c
  compat/secure-mem-windows.c)
set(SECRET_STORAGE_HEADERS
  secret-storage.h
  nondumpable-allocator.h
  compat/secure-mem.h
  compat/secure-mem-posix.h
  compat/secure-mem-windows.h)

add_library(secret-storage SHARED ${SECRET_STORAGE_SOURCES})

target_include_directories(secret-storage
  PRIVATE
    # workaround, "compat" should be a separate static library
    ${PROJECT_SOURCE_DIR}/lib
)

target_link_libraries(secret-storage PRIVATE GLib::GLib)
set_target_properties(secret-storage PROPERTIES C_VISIBILITY_PRESET hidden)

install(TARGETS secret-storage LIBRARY DESTINATION lib)
install(FILES ${SECRET_STORAGE_HEADERS} DESTINATION include/syslog-ng)

add_test_subdirectory(tests)
