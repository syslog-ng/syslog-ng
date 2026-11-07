`logmsg`, `timeutils`: Fixed pointer truncation on 64-bit Windows (LLP64, 32-bit `long`), which crashed every date
parser and could leak or share strings in string lists.
