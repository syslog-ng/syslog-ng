`json-parser()`: Fixed parsing non-string arrays.

syslog-ng now no longer parses non-string arrays to list of strings, losing the original type
information of the array's elements.
