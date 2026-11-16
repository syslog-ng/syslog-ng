`format-json`: Added the `--order` option to `$(format-json)` and `$(format-flat-json)` to control the order of
the emitted keys. It accepts `descending` (the unchanged default), `ascending`, and `as-written`, the latter
keeping the keys in their first-seen order instead of sorting them. A name that occurs more than once keeps its
first position under `as-written` and takes its last value under every order.
