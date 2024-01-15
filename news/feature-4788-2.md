`python`: Added support for `typing.Dict[str, bytes]` type `LogMessage` values.

syslog-ng's new `kvlist` internal type shows up as a dict in the python binding.
For example:
```
msg["kvlist_type_nv"] = {'foo': b'foovalue', 'bar': b'barvalue'}
print(msg["kvlist_type_nv"])  # {'foo': b'foovalue', 'bar': b'barvalue'}
```

Please note that setting a dict like this is not possible for the `options()` option
in the configuration file.
