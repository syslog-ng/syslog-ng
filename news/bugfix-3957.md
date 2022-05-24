`grouping-by()`: fix `grouping-by()` use through parser references.
Originally if a grouping-by() was part of a named parser statement and was
referenced from multiple log statements, only the first grouping-by()
instance behaved properly, 2nd and subsequent references were ignoring all
configuration options and have reverted using defaults instead.

`db-parser()`: similarly to `grouping-by()`, `db-parser()` also had issues
propagating some of its options to 2nd and subsequent references of a parser
statement. This includes `drop-unmatched()`, `program-template()` and
`template()` options.
