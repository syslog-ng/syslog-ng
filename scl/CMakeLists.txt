set(SCL_DIRS
    apache
    arr
    cee
    cim
    checkpoint
    cisco
    cloudwatchlogshlc
    collectd
    default-network-drivers
    darwinosl
    discord
    elasticsearch
    ewmm
    freebsd-audit
    fortigate
    google
    graphite
    graylog2
    hdfs
    stats-exporter
    iptables
    jellyfin
    junos
    linux-audit
    loadbalancer
    loggly
    logmatic
    logscale
    mariadb
    mbox
    netskope
    nodejs
    openobserve
    opensearch
    osquery
    pacct
    paloalto
    pihole
    pgsql
    python
    qbittorrent
    rewrite
    slack
    snmptrap
    solaris
    splunk
    sudo
    sumologic
    syslogconf
    system
    telegram
    websense
    windowseventlog
    azure
)

option(ENABLE_SCL_SYSLOGCONF_AWK "Install scl/syslogconf/convert-syslogconf.awk" ON)

install(DIRECTORY ${SCL_DIRS} DESTINATION share/syslog-ng/include/scl
        USE_SOURCE_PERMISSIONS
        PATTERN "convert-syslogconf.awk" EXCLUDE)

if (ENABLE_SCL_SYSLOGCONF_AWK)
    install(FILES syslogconf/convert-syslogconf.awk
            DESTINATION share/syslog-ng/include/scl/syslogconf)
endif()

install(FILES scl.conf DESTINATION share/syslog-ng/include)

if (NOT EXISTS ${CMAKE_INSTALL_PREFIX}/etc/syslog-ng.conf)
    install(FILES syslog-ng.conf DESTINATION etc)
endif()
