`control`: reject malformed STATS/QUERY commands instead of aborting

Malformed control socket input (e.g. CRLF line endings or missing STATS
format argument) could abort syslog-ng via g_assert. Strip trailing CR
and return FAIL for invalid commands.
