`python()`: fix LogMessage subscript not raising KeyError on non-existent keys

When message fields were queried (`msg["key"]`) and the given key did not exist,
`None` or an empty string was returned (depending on the version of the config).

Neither was correct, now a KeyError occurs in such cases.
