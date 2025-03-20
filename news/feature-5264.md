`check-program`: Introduced as a flag for global or source options.

By default, this flag is set to false. Enabling the check-program flag triggers `program` name validation for `RFC3164` messages. Valid `program` names must adhere to the following criteria:

Contain only these characters: `[a-zA-Z0-9-_/().]`
Include at least one alphabetical character.
If a `program` name fails validation, it will be considered part of the log message.


Example:

```
source { network(flags(check-hostname, check-program)); };
```
