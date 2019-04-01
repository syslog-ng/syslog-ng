set(SCL_DIRS
    apache
    cim
    tele-voice
    cisco
    collectd
    default-network-drivers
    elasticsearch
    ewmm
    graphite
    graylog2
    hdfs
    iptables
    kafka
    linux-audit
    loadbalancer
    loggly
    logmatic
    mbox
    netskope
    nodejs
    osquery
    pacct
    rewrite
    slack
    snmptrap
    solaris
    sudo
    syslogconf
    system
    telegram
    websense
    windowseventlog
)

install(DIRECTORY ${SCL_DIRS} DESTINATION share/syslog-ng/include/scl)
install(FILES scl.conf DESTINATION etc)

if (NOT EXISTS ${CMAKE_INSTALL_PREFIX}/etc/syslog-ng.conf)
    install(FILES syslog-ng.conf DESTINATION etc)
endif()
