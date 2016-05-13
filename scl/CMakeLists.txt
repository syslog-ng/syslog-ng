set(SCL_DIRS
    apache
    cim
    elasticsearch
    graphite
    hdfs
    kafka
    loggly
    logmatic
    mbox
    nodejs
    pacct
    rewrite
    solaris
    syslogconf
    system
)

install(DIRECTORY ${SCL_DIRS} DESTINATION share/syslog-ng/include/scl)
install(FILES scl.conf DESTINATION etc)

if (NOT EXISTS ${CMAKE_INSTALL_PREFIX}/etc/syslog-ng.conf)
    install(FILES syslog-ng.conf DESTINATION etc)
endif()
