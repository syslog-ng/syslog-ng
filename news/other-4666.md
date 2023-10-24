templates: The `template-escape()` option now only escapes the top-level template function.

Before syslog-ng 4.5.0 if you had embedded template functions, the `template-escape(yes)` setting
escaped the output of each template function, so the parent template function received an
already escaped string. This was never the intention of the `template-escape()` option.

Although this is a breaking change, we do not except anyone having a config that is affected.
If you have such a config, make sure to follow-up this change. If you need help with it, feel
free to open an issue or discussion on GitHub, or contact us on the Axoflow Discord server.
