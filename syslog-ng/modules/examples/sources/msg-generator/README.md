# Message generator source

This example source plugin generates a message every `freq()` seconds based on its `template()` option.

Please note that `template()` is a source-side template, only message-independent macros can be used.

### Usage

```
options {
  use-uniqid(yes);
};

source s_generator {
  example-msg-generator(
    template("-- ${ISODATE} ${LOGHOST} ${UNIQID} Generated message. --")
    freq(0.5)
  );
};

log {
  source(s_generator);

  destination {
    file("/dev/stdout");
  };
};
```
