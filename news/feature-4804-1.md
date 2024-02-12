`template()`: Added a new template function: `$(tags-head)`

This template function accepts multiple tag names, and returns the
first one that is set.

Example config:
```
# resolves to "bar" if "bar" tag is set, but "foo" is not
template("$(tags-head foo bar baz)")
```
