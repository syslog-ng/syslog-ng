set(SCL_DIRS
    apache
    cim
    cisco
    default-network-drivers
    elasticsearch
    ewmm
    graphite
    graylog2
    hdfs
    iptables
    kafka
    loadbalancer
    loggly
    logmatic
    mbox
    nodejs
    osquery
    pacct
    rewrite
    snmptrap
    solaris
    sudo
    syslogconf
    system
    telegram
    windowseventlog
)

install(DIRECTORY ${SCL_DIRS} DESTINATION share/syslog-ng/include/scl)
install(FILES scl.conf DESTINATION etc)

if (NOT EXISTS ${CMAKE_INSTALL_PREFIX}/etc/syslog-ng.conf)
    install(FILES syslog-ng.conf DESTINATION etc)
endif()
