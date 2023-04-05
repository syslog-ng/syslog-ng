`python()`: new LogMessage methods for querying as string and with default values

- `get(key[, default])`
  Return the value for `key` if `key` exists, else `default`. If `default` is
  not given, it defaults to `None`, so that this method never raises a
  `KeyError`.


- `get_as_str(key, default=None, encoding='utf-8', errors='strict', repr='internal')`:
  Return the string value for `key` if `key` exists, else `default`.
  If `default` is not given, it defaults to `None`, so that this method never
  raises a `KeyError`.

  The string value is decoded using the codec registered for `encoding`.
  `errors` may be given to set the desired error handling scheme.

  Note that currently `repr='internal'` is the only available representation.
  We may implement another more Pythonic representation in the future, so please
  specify the `repr` argument explicitly if you want to avoid future
  representation changes in your code.
