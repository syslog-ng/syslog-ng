`set-severity()`: new rewrite rule for changing message severity

`set-severity()` receives a template and sets message severity by evaluating the
template.

Numerical and textual severity levels are both supported.

Examples:

```
rewrite {
  set-severity("info");
  set-severity("6");
  set-severity("${.json.severity}");
};
```
