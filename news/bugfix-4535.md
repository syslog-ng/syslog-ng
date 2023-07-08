`int32()` and `int64()` type casts: accept hex numbers as proper
number representations just as the @NUMBER@ parser within db-parser().
Supporting octal numbers were considered and then rejected as the canonical
octal representation for numbers in C would be ambigious: a zero padded
decimal number could be erroneously considered octal. I find that log
messages contain zero padded decimals more often than octals.
