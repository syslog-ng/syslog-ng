set(SYSLOG_NG_CTL_SOURCES
    syslog-ng-ctl.c
    control-client.h
    control-client.c
)

add_executable(syslog-ng-ctl ${SYSLOG_NG_CTL_SOURCES})
target_link_libraries(syslog-ng-ctl PRIVATE
    syslog-ng
    secret-storage
    ${GLIB_LIBRARIES}
    ${RESOLV_LIBS})
target_include_directories(syslog-ng-ctl PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
install(TARGETS syslog-ng-ctl RUNTIME DESTINATION sbin)
