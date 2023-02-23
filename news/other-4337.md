stats related options: The stats related options have been groupped to a new `stats()` block.

This affects the following global options:
  * `stats-freq()`
  * `stats-level()`
  * `stats-lifetime()`
  * `stats-max-dynamics()`

These options have been kept for backward compatibility, but they have been deprecated.

Migrating from the old stats options to the new ones looks like this.
```
@version: 4.0

options {
    stats-freq(1);
    stats-level(1);
    stats-lifetime(1000);
    stats-max-dynamics(10000);
};
```
```
@version: 4.1

options {
    stats(
        freq(1)
        level(1)
        lifetime(1000)
        max-dynamics(10000)
    );
};
```

**Breaking change**
For more than a decade `stats()` was a deprecated alias to `stats-freq()`, now it is used as the name
of the new block. If you have been using `stats(xy)`, use `stats(freq(xy))` instead.
