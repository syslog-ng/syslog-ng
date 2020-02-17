`config version`: make the config version check of the configuration more
liberal by accepting version numbers that had no changes relative to the
current version.  This means that if you are running 3.25 and the last
semantic change in the configuration was 3.22, then anything between 3.22
and 3.25 (inclusive) is accepted by syslog-ng without a warning at startup.
