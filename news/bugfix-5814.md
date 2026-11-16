`geoip2`: Fixed incomplete geodata extraction when the first value is a preferred-language name or an empty container.

The entry walker now produces its own status on every successful path and preserves errors from skipped subtrees.
Traversal errors are logged without rejecting the original message.
