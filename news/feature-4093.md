`json-parser()`: add key-delimiter() option to extract JSON structure
members into name-value pairs, so that the names are flattened using the
character specified, instead of dot.

Example:
  Input: {"foo":{"key":"value"}}

  Using json-parser() without key-delimiter() this is extracted to:

      foo.key="value"

  Using json-parser(key-delimiter("~")) this is extracted to:

      foo~key="value"

This feature is useful in case the JSON keys contain dots themselves, in
those cases the syslog-ng representation is ambigious.
