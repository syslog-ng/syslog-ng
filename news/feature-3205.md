`iterate`: new template function

The iterate template function generates a series from an initial number and a `next` function.

For example you can generate a sequence of nonnegative numbers with

```
source {
  example-msg-generator(
    num(3)
    template("$(iterate $(+ 1 $_) 0)")
  );
};
```
