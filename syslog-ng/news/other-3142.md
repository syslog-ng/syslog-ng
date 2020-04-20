afsnmp: merge two existing SNMP modules (trapd-parser and destination) into one.

Motivation: keep closely related modules together and decrease the number of small packages.

Packaging related changes:

libsnmptrapd-parser.so has been removed, both the snmp destination and the trapd parser are part of afsnmp.so .

 * deb: we already had mod-snmp which shipped snmp-dest. From now, this packages installs the snmptrapd-parser syslog-ng module. The syslog-ng-mod-snmptrapd-parser package has been removed.
 * rpm: snmpdest renamed to afsnmp and the snmptrapd-parser is installed by this package from now

