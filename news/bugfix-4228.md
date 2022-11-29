`csv-parser()`: fixed the processing of the dialect() parameter, which was
not taken into consideration.

`apache-accesslog-parser()`: Apache may use backslash-style escapes in the
`request` field, so support it by setting the csv-parser() dialect to
`escape-backslash-with-sequences`.  Also added validation that the
`rawrequest` field contains a valid HTTP request and only extract `verb`,
`request` and `httpversion` if this is the case.
