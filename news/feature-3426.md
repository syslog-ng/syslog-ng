`filter`: new template function

The new introduced `filter` template function will allow filtering lists based on a filter expression.

For example this snippet removes odd numbers
```
log {
  source { example-msg-generator(num(1) values(INPUT => "0,1,2,3")); };
  destination {
     file("/dev/stdout"
           template("$(filter ('$(% $_ 2)' eq '0') $INPUT)\n)")
     );
  };
};
```
