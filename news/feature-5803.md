`http`: `response-body-action()` classifies a 2xx response by its body

For APIs that report a failure with a 2xx status code and the error in the response body, for
example `200 {"Status": "Fail"}`: `"regexp" => success|retry|drop|disconnect` pairs, matched in
order against the response body when the status code means success.

The patterns are PCRE2 regular expressions compiled with default options: use the inline modifier
`(?i)` for case-insensitive matching, for example `"(?i)status.*fail" => retry`. The first 64 KiB
of the body are matched (only captured when the option is configured); a longer body is truncated
and a debug message says so.
