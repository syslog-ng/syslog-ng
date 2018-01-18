set(SECRET_STORAGE_SOURCES
  secret-storage.c)
set(SECRET_STORAGE_HEADERS
  secret-storage.h)

add_library(secret-storage SHARED ${SECRET_STORAGE_SOURCES})
target_include_directories(secret-storage PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_include_directories(secret-storage PUBLIC "${GLIB_INCLUDE_DIRS}")

install(TARGETS secret-storage LIBRARY DESTINATION lib)
install(FILES ${SECRET_STORAGE_HEADERS} DESTINATION include/syslog-ng)


add_subdirectory(tests)
