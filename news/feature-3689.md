`disk-buffer`: Now we optimize the file truncating frequency of disk-buffer.

The new behavior saves IO time, but loses some disk space, which is configurable with a new option.
The new option in the config is settable at 2 places:
 * `truncate-size-ratio()` in the `disk-buffer()` block, which affects the given disk-buffer.
 * `disk-buffer(truncate-size-ratio())` in the global `options` block, which affects every disk-buffer
   which did not set `truncate-size-ratio()` itself.
The default value is 0.01, which operates well with most disk-buffers.

If the possible size reduction of the truncation does not reach `truncate-size-ratio()` x `disk-buf-size()`,
we do not truncate the disk-buffer.

To completely turn off truncating (maximal disk space loss, maximal IO time saved) set `truncate-size-ratio(1)`,
or to mimic the old behavior (minimal disk space loss, minimal IO time saved) set `truncate-size-ratio(0)`.
