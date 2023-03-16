Consider messages consumed into correlation states "matching": syslog-ng's
correlation functionality (e.g.  grouping-by() or db-parser() with such
rules) drop individual messages as they are consumed into a correlation
contexts and you are using `inject-mode(aggregate-only)`.  This is usually
happens because you are only interested in the combined message and not in
those that make up the combination. However, if you are using correlation
with conditional processing (e.g. if/elif/else or flags(final)), such
messages were erroneously considered as unmatching, causing syslog-ng to
take the alternative branch.

Example:

With a configuration similar to this, individual messages are consumed into
a correlation state and dropped by `grouping-by()`:

```
log {
    source(...);

    if {
        grouping-by(... inject-mode(aggregate-only));
    } else {
        # alternative branch
    };
};
```

The bug was that these individual messages also traverse the `else` branch,
even though they were successfully processed with the inclusion into the
correlation context. This is not correct. The bugfix changes this behaviour.
