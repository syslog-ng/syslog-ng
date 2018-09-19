add_library(examples SHARED example-plugins.c)


add_subdirectory(sources/msg-generator)
add_subdirectory(sources/threaded-random-generator)
add_subdirectory(sources/threaded-diskq-source)

target_link_libraries(examples PRIVATE msg-generator)
target_link_libraries(examples PRIVATE threaded-random-generator)
target_link_libraries(examples PRIVATE threaded-diskq-source)


target_link_libraries(examples PRIVATE syslog-ng)

install(TARGETS examples LIBRARY DESTINATION lib/syslog-ng COMPONENT examples)
