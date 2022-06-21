`add-contextual-data()`: add support for type propagation, e.g. set the
type of name-value pairs as they are created/updated to the value returned
by the template expression that we use to set the value.

The 3rd column in the CSV file (e.g. the template expression) now supports
specifying a type-hint, in the format of "type-hint(template-expr)".

Example line in the CSV database:

selector-value,name-value-pair-to-be-created,list(foo,bar,baz)
