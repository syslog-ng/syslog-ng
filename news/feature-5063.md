`syslog-parser`: Added the `no-piggyback-errors` and the `piggyback-errors` flags to control whether the message retains the original message or not. By default the old behaviour/`no-piggyback-errors` flag is active, modifing the original message on error.

- `no-piggyback-errors`: On failure the original message, will be left as it was before parsing, the value of `$MSGFORMAT` will be set to `syslog-error`, and a tag will be placed on the message roccesponding to the parser's failure.
- `piggyback-errors`: On failure the old behaviour is used (clearing the entire message then syslog-ng will generate a new message in place of the old one describing the parser's error).

The following new tags can be added by the `syslog-parser` to the message when the parsing failed:
  - `syslog.rfc5424_missing_hostname`
  - `syslog.rfc5424_missing_app_name`
  - `syslog.rfc5424_missing_procid`
  - `syslog.rfc5424_missing_msgid`
  - `syslog.rfc5424_missing_sdata`
  - `syslog.rfc5424_invalid_sdata`
  - `syslog.rfc5424_missing_message`