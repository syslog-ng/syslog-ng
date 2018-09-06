add_library(examples SHARED example-plugins.c)


add_subdirectory(sources/msg-generator)
target_link_libraries(examples PRIVATE msg-generator)


target_link_libraries(examples PRIVATE syslog-ng)

install(TARGETS examples LIBRARY DESTINATION lib/syslog-ng COMPONENT examples)
