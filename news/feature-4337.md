stats: Added `syslog-stats()` global `stats()` group option.

E.g.:
```
options {
  stats(
    syslog-stats(no);
  );
};

It changes the behavior of counting messages based on different syslog-proto fields,
like `SEVERITY`, `FACILITY`, `HOST`, etc...

Possible values are:
  * `yes` => force enable
  * `no` => force disable
  * `auto` => let `stats(level())` decide (old behavior)
