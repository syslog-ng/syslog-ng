`$(tag)` template function: expose bit-like tags that are set on
messages.

Syntax:
    $(tag <name-of-the-tag> <value-if-set> <value-if-unset>)

Unless the value-if-set/unset arguments are specified $(tag) results in a
boolean type, expanding to "0" or "1" depending on whether the message has
the specified tag set.

If value-if-set/unset are present, $(tag) would return a string, picking the
second argument <value-if-set> if the message has <tag> and picking the
third argument <value-if-unset> if the message does not have <tag>
