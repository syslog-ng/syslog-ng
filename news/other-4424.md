`grouping-by()`: Remove setting of the `${.classifier.context_id}`
name-value pair in all messages consumed into a correlation context.  This
functionality is inherited from db-parser() and has never been documented
for `grouping-by()`, has of limited use, and any uses can be replaced by the
use of the built-in macro named `$CONTEXT_ID`.  Modifying all consumed
messages this way has significant performance consequences for
`grouping-by()` and removing it outweighs the small incompatibility this
change introduces. The similar functionality in `db-parser()` correlation is
not removed with this change.
