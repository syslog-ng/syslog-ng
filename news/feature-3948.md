`set-matches()` and `unset-matches()`: these new rewrite operations allow
the setting of match variables ($1, $2, ...) in a single operation, based
on a syslog-ng list expression.

Example:
  # set $1, $2 and $3 respectively
  set-matches("foo,bar,baz")

  # likewise, but using a list function
  set-matches("$(explode ':' 'foo:bar:baz')");
