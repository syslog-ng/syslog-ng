set(SECRET_STORAGE_SOURCES
  secret-storage.c
  nondumpable-allocator.c)
set(SECRET_STORAGE_HEADERS
  secret-storage.h
  nondumpable-allocator.h)

add_library(secret-storage SHARED ${SECRET_STORAGE_SOURCES})

target_include_directories(secret-storage
  PRIVATE
    ${GLIB_INCLUDE_DIRS}
    # workaround, "compat" should be a separate static library
    ${PROJECT_SOURCE_DIR}/lib
)

target_link_libraries(secret-storage PRIVATE ${GLIB_LIBRARIES})
set_target_properties(secret-storage PROPERTIES C_VISIBILITY_PRESET hidden)

install(TARGETS secret-storage LIBRARY DESTINATION lib)
install(FILES ${SECRET_STORAGE_HEADERS} DESTINATION include/syslog-ng)

add_test_subdirectory(tests)
