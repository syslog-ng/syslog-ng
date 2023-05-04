`config`: Added `internal()` option to `source`s, `destination`s, `parser`s and `rewrite`s.

Its main usage is in SCL blocks. Drivers configured with `internal(yes)` register
their metrics on level 3. This makes developers of SCLs able to create metrics manually
with `metrics-probe()` and "disable" every other metrics, they do not need.
