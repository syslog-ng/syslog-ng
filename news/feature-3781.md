`throttle()`: added a new `filter` that allows rate limiting messages based on arbitrary keys in each message.

Note: messages over the rate limit are dropped (just like in any other filter).

```
filter f_throttle {
  throttle(
    template("$HOST")
    rate(5000)
  );
};
```
