SUBDIRS = affile afsocket syslogformat basicfuncs basic-proto confgen convertfuncs csvparser dbparser dummy afprog afsql afstreams afuser pacctformat json

# windows plugins
SUBDIRS += agent-config eventlog windows-resource

# mongodb
SUBDIRS += afmongodb

# smtp
SUBDIRS += afsmtp

# pe modules
SUBDIRS += license confighash diskq logstore rltp-proto snmp

if !IS_WINDOWS
SUBDIRS += afsqlsource
endif
