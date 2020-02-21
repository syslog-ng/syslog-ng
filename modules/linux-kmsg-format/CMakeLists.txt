set(LINUX_KMSG_FORMAT_SOURCES
    linux-kmsg-format.c
    linux-kmsg-format.h
    linux-kmsg-format-plugin.c
)

add_module(
  TARGET linux-kmsg-format
  SOURCES ${LINUX_KMSG_FORMAT_SOURCES}
)

add_test_subdirectory(tests)
