`$(values)` and `$(names)`: these new template functions can be used to
query a list of name-value pairs in the current message. The list of name
value pairs queried are specified by a value-pairs expression, just like
with `$(format-json)`.

Examples:

  This expression sets the JSON array `values` to contain the list of SDATA
  values, while the JSON array `names` would contain the associated names, in
  the same order.

  $(format-json values=list($(values .SDATA.*)) names=list($(names .SDATA.*)))

The resulting name-value pairs are always sorted by their key, regardless of
the argument order.
