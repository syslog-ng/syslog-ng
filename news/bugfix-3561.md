`stats-level()`: fix processing the changes in the stats-level() global
option: changes in stats-level() were not reflected in syslog
facility/severity related and message tag related counters after first
configuration reload. These counters continued to operate according to the
value of stats-level() at the first reload.
