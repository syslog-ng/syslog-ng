`multi-line-mode(smart)`: a new multi-line mode was added to recognize
backtraces for various programming languages automatically.  With this
multi-line mode, the inherently multi-line data backtrace format is
recognized even if they span multiple lines in the input and are converted
to a single log message for easier analysis.  Backtraces for the following
programming languages are recognized : Python, Java, JavaScript, PHP, Go,
Ruby and Dart.

The regular expressions to recognize these programming languages are
specified by an external file called
`/usr/share/syslog-ng/smart-multi-line.fsm` (installation path depends on
configure arguments), in a format that is described in that file.
