`http`: relax the worker-partition-key() requirement for message-independent URL templates

The partition-key guard previously fired for any URL containing a `$` sign, even when
the template was effectively constant (e.g. `$(url-encode literal)` after SCL backtick
substitution). The check now uses a post-compile predicate that walks the template AST
and only requires `worker-partition-key()` if the URL is genuinely message-dependent.
