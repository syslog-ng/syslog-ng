# Threaded random generator source

This example source plugin generates `bytes()` random bytes every `freq()` seconds.
The output is represented as a hexadecimal string in a generated log message.

`type()` can be `random` or `urandom`, the default is `random`.

The plugin is implemented on the top of `LogThreadedSourceDriver`. It provides a separate thread, so blocking function
calls are allowed.

### Usage

```
source s_generator {
  example-random-generator(
    bytes(32)
    freq(0.5)
    type(random)
  );
};

log {
  source(s_generator);

  destination {
    file("/dev/stdout");
  };
};
```
