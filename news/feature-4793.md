`windows-eventlog-xml-parser()`: Added a new parser to parse Windows Eventlog XMLs.

Its parameters are the same as the `xml()` parser.

Example config:
```
parser p_win {
    windows-eventlog-xml-parser(prefix(".winlog."));
};
```
