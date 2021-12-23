filters: fix `not` operator in filter expressions (regression in v3.35.1)

Reusing a filter that contains the `not` operator more than once, or
referencing a complex expression containing `not` might have caused invalid results
in the previous syslog-ng version (v3.35.1).  This has been fixed.
