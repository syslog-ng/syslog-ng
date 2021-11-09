set(AFSOCKET_SOURCES
    afsocket.c
    afsocket.h
    afsocket-source.c
    afsocket-source.h
    afsocket-dest.c
    afsocket-dest.h
    socket-options.c
    socket-options.h
    transport-mapper.c
    transport-mapper.h
    afinet.c
    afinet.h
    afinet-source.c
    afinet-source.h
    afinet-dest-failover.c
    afinet-dest-failover.h
    afinet-dest.c
    afinet-dest.h
    socket-options-inet.c
    socket-options-inet.h
    transport-mapper-inet.c
    transport-mapper-inet.h
    afunix-source.c
    afunix-source.h
    afunix-dest.c
    afunix-dest.h
    transport-mapper-unix.c
    transport-mapper-unix.h
    socket-options-unix.c
    socket-options-unix.h
    transport-unix-socket.c
    transport-unix-socket.h
    compat-unix-creds.c
    compat-unix-creds.h
    afsocket-grammar.y
    afsocket-parser.c
    afsocket-parser.h
    afsocket-plugin.c
    systemd-syslog-source.h
    systemd-syslog-source.c
    afsocket-systemd-override.h
)

find_package(ZLIB REQUIRED)
find_package(systemd)

add_module(
  TARGET afsocket
  GRAMMAR afsocket-grammar
  INCLUDES ${ZLIB_INCLUDE_DIRS}
           ${WRAP_INCLUDE_DIRS}
           ${Libsystemd_INCLUDE_DIRS}
  DEPENDS ${WRAP_LIBRARIES}
          ${ZLIB_LIBRARIES}
          OpenSSL::SSL
          OpenSSL::Crypto
          libnet
          ${Libsystemd_LIBRARIES}
  SOURCES ${AFSOCKET_SOURCES}
)

add_test_subdirectory(tests)
