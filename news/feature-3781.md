`throttle()`: added a new `filter` that allows rate limiting messages based on arbitrary keys in each message.

Note, that like every other `filter`, messages unmatched (outside of the rate limit) by the `throttle()` filter are
dropped by default. Also, as every `filter` can be used in `channel`s or `if` conditions, the messages unmatched
can be caught and handled, like sent to a different `destination`, etc...

Options:
  * `value()`:
    * optional
    * The macro, whose resolution will be used to create unique throttle token buckets.
    * If not set, one bucket is used for all messages.
  * `rate()`:
    * required
    * The number of messages for each unique macro resolution, that will be let through (matched) by the filter each second.

Example config:
```
filter f_throttle {
  throttle(
    value("HOST")
    rate(5000)
  );
};
```
