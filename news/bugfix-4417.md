`$(format-json)`: fix escaping control characters

`$(format-json)` produced invalid JSON output when a string value contained control characters.
