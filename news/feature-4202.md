`$(format-date)`: add a new template function to format time and date values

    $(format-date [options] format-string [timestamp])

    $(format-date) takes a timestamp in the DATETIME representation and
    formats it according to an strftime() format string.  The DATETIME
    representation in syslog-ng is a UNIX timestamp formatted as a decimal
    number, with an optional fractional part, where the seconds and the
    fraction of seconds are separated by a dot.

    If the timestamp argument is missing, the timestamp of the message is
    used.

    Options:
      --time-zone <TZstring> -- override timezone of the original timestamp
