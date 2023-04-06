`$(format-json)`: fix RFC8259 number violation

`$(format-json)` produced invalid JSON output when it contained numeric values with leading zeros or + signs.
This has been fixed.
