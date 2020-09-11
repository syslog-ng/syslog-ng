`map`: pass `$_` to `if` correctly.

Prior this patchset, `if` did not receive `$_` correctly.

After this change, these configurations will work:

```
log {
  source { example-msg-generator(num(1) values(INPUT => "0,1,2,3")); };
  destination {
     file("/dev/stdout"
           template("$(map $(if ('$(% $_ 2)' eq '0') 'even' 'odd') $INPUT)'\n)")
     );
  };
};
```
