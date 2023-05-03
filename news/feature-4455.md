`python()`, `python-fetcher()` sources: Added a mapping for the `flags()` option.

The state of the `flags()` option is mapped to the `self.flags` variable, which is
a `Dict[str, bool]`, for example:
```python
{
    'parse': True,
    'check-hostname': False,
    'syslog-protocol': True,
    'assume-utf8': False,
    'validate-utf8': False,
    'sanitize-utf8': False,
    'multi-line': True,
    'store-legacy-msghdr': True,
    'store-raw-message': False,
    'expect-hostname': True,
    'guess-timezone': False,
    'header': True,
    'rfc3164-fallback': True,
}
```
