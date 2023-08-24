`metrics-probe`: Added dynamic labelling support via name-value pairs

You can use all value-pairs options, like `key()`, `rekey()`, `pair()` or `scope()`, etc...

Example:
```
metrics-probe(
  key("foo")
  labels(
    "static-label" => "bar"
    key(".my_prefix.*" rekey(shift-levels(1)))
  )
);
```
```
syslogng_foo{static_label="bar",my_prefix_baz="almafa",my_prefix_foo="bar",my_prefix_nested_axo="flow"} 4
```
