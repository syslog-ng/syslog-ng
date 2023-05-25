`lib/logmsg`: Public field `LogMessage::protected` has been renamed to `LogMessage::write_protected`.

Usage of this field is discouraged directly, use the following functions:
  * `log_msg_is_write_protected()`
  * `log_msg_write_protect()`
