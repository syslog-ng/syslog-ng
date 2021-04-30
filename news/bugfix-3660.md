templates: fixed error reporting when invalid templates were specified

The `amqp()`, `file()` destination, `sql()`, `stomp()`, `pdbtool`, and
`graphite()` plugins had template options that did not report errors at startup
when invalid values were specified.
