`filter`: Added numerical severity settings.

The `level` filter option now accepts numerical values similar to `facility`.

Example config:
```
filter f_severity {
  level(4)
};
```
This is equivalent to
```
filter f_severity {
  level("warning")
};
```

For more information, consult the [documentation](https://syslog-ng.github.io/admin-guide/080_Log/030_Filters/005_Filter_functions/004_level_priority.html).
