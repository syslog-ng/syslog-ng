set(SCL_DIRS
    apache
    cee
    cim
    checkpoint
    cisco
    collectd
    default-network-drivers
    discord
    elasticsearch
    ewmm
    fortigate
    graphite
    graylog2
    hdfs
    iptables
    junos
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
    paloalto
    rewrite
    slack
    snmptrap
    solaris
    sudo
    sumologic
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
