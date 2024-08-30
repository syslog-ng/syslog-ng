`format-json`: spaces around `=` in `$(format-json)` template function could cause a
[crash](https://github.com/syslog-ng/syslog-ng/issues/5065).
The fix of the issue also introduced an enhancement, from now on spaces are allowed
around the `=` operator, so the following `$(format-json)` template function calls
are all valid:
```
$(format-json foo =alma)
$(format-json foo= alma)
$(format-json foo = alma)
$(format-json foo=\" alma \")
$(format-json foo= \" alma \")
$(format-json foo1= alma foo2 =korte foo3 = szilva foo4 = \" meggy \" foo5=\"\")
```
Please note the usage of the escaped strings like `\" meggy \"`, and the (escaped and) quoted form
that used for an empty value `\"\"`, the latter is a breaking change as earlier an expression like
`key= ` led to a json key-value pair with an empty value `{"key":""}` that will not work anymore.
