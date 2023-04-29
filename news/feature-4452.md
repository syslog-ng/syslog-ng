`$(format-json)` and template functions which support value-pairs
expressions: new key transformations upper() and lower() have been added to
translate the caps of keys while formatting the output template. For
example:

    template("$(format-json test.* --upper)\n")

Would convert all keys to uppercase. Only supports US ASCII.
