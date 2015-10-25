date parser
===========

The date parser can extract dates from non-syslog messages. It
operates by default on `$MSG` but any template can be provided. The
date will be stored as the sender date and the remaining of the
message (if any) is discarded. Therefore, the result can be accessed
using any of the `S_` macros (`${S_DATE}`, `${S_ISODATE}`,
`${S_MONTH}`, …).

Example config:

```
source s_apache {
  channel {
    source {
      file("/var/log/apache/access.log" flags(no-parse));
     };
     parser(p_apache_parser);
    };
  };
};

parser p_apache_parser {
  csv-parser(columns("APACHE.CLIENT_IP", "APACHE.IDENT_NAME", "APACHE.USER_NAME",
                     "APACHE.TIMESTAMP", "APACHE.REQUEST_URL", "APACHE.REQUEST_STATUS",
                     "APACHE.CONTENT_LENGTH", "APACHE.REFERER", "APACHE.USER_AGENT",
                     "APACHE.PROCESS_TIME", "APACHE.SERVER_NAME")
             flags(escape-double-char,strip-whitespace)
             delimiters(" ")
             quote-pairs('""[]'));

  date-parser(
       format("%d/%b/%Y:%H:%M:%S %Z")
       template("${APACHE.TIMESTAMP}"));
};

destination d_file {
  file("/var/log/messages");
};

log {
  source(s_apache);
  destination(d_file);
};
```
