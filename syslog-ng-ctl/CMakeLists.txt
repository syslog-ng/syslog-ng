set(SYSLOG_NG_CTL_SOURCES
    syslog-ng-ctl.c
    control-client.h
    commands/attach.h
    commands/attach.c
    commands/commands.h
    commands/commands.c
    commands/credentials.h
    commands/credentials.c
    commands/verbose.h
    commands/verbose.c
    commands/log-level.h
    commands/log-level.c
    commands/ctl-stats.h
    commands/ctl-stats.c
    commands/query.h
    commands/query.c
    commands/license.h
    commands/license.c
    commands/config.h
    commands/config.c
    commands/healthcheck.h
    commands/healthcheck.c
    control-client.c
)

add_executable(syslog-ng-ctl ${SYSLOG_NG_CTL_SOURCES})
target_link_libraries(syslog-ng-ctl PRIVATE
    syslog-ng
    secret-storage
    GLib::GLib
    ${RESOLV_LIBS})
target_include_directories(syslog-ng-ctl PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
install(TARGETS syslog-ng-ctl RUNTIME DESTINATION sbin)
