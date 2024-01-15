typing: Introduced a new LogMessage value type called `kvlist`.

This new type is similar to the `list` type, with the addition that
the strings in the `kvlist` are defining keys and values in an alternating
manner.

Creating a `kvlist` can be done with the new `$(kvlist-from-keys-and-values)`
template function, for example:
```
$(kvlist-from-keys-and-values ${list_of_keys} ${list_of_values})
```
