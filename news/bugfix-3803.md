`unset()`, `groupunset()`: fix unwanted removal of values on different log paths

Due to a copy-on-write bug, `unset()` and `groupunset()` not only removed values
from the appropriate log paths, but from all the others where the same message
went through. This has been fixed.
