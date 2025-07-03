`cisco`: Added support for Cisco Nexus NXOS 9.3 syslog format.

The parser now recognises NXOS 9.3 timestamps in `YYYY MMM DD HH:MM:SS` format and handles the different
sequence number prefix (`: ` instead of `seqno: `) used by NXOS 9.3 compared to traditional IOS formats.

Example Cisco configuration:

- NXOS: `(config)# logging server <syslog-ng-server-ip> port 2000`
- IOS: `(config)# logging host <syslog-ng-server-ip> transport udp port 2000`

Example syslog-ng configuration:

```
@include "scl.conf"

source s_cisco {
    network(ip(0.0.0.0) transport("udp") port(2000) flags(no-parse));
};

parser p_cisco {
    cisco-parser();
};

destination d_placeholder {
    # Define your destination here
};

log {
    source(s_cisco);
    parser(p_cisco);
    destination(d_placeholder);
};
```
