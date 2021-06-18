`logpath`: Fixed a message write protection bug, where message modifications (rewrite rules, parsers, etc.)
leaked through preceding path elements. This may have resulted not only in unwanted/undefined message modification,
but in certain cases crash as well.
