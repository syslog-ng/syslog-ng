`format-json`: Added the `--order` option to `$(format-json)` and `$(format-flat-json)` to control the order of
the emitted keys. It accepts `descending` (the unchanged default), `ascending`, and `as-written`, the latter
keeping the keys in their first-seen order instead of sorting them.
