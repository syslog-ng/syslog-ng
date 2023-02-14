`date-parser()`: add `value()` parameter to instruct `date-parser()` to store
the resulting timestamp in a name-value pair, instead of changing the
timestamp value of the LogMessage.

`datetime` type representation: typed values in syslog-ng are represented as
strings when stored as a part of a log message.  syslog-ng simply remembers
the type it was stored as.  Whenever the value is used as a specific type in
a type-aware context where we need the value of the specific type, an
automatic string parsing takes place.  This parsing happens for instance
whenever syslog-ng stores a datetime value in MongoDB or when
`$(format-date)` template function takes a name-value pair as parameter.
The datetime() type has stored its value as the number of milliseconds since
the epoch (1970-01-01 00:00:00 GMT).  This has now been enhanced by making
it possible to store timestamps up to nanosecond resolutions along with an
optional timezone offset.

`$(format-date)`: when applied to name-value pairs with the `datetime` type,
use the timezone offset if one is available.
