`pdbtool test`: fix two type validation bugs:

  1) When `pdbtool test` validates the type information associated with a name-value
     pair, it was using string comparisons, which didn't take type aliases
     into account. This is now fixed, so that "int", "integer" or "int64"
     can all be used to mean the same type.

  2) When type information is missing from a `<test_value/>` tag, don't
     validate it against "string", rather accept any extracted type.

In addition to these fixes, a new alias "integer" was added to mean the same
as "int", simply because syslog-ng was erroneously using this term when
reporting type information in its own messages.
