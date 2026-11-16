`file()`, `wildcard-file()`: Added support for skipping existing content with `read-old-records(no)` when no persisted
position exists, while preserving saved positions and the skipped EOF across restarts.
For `wildcard-file()`, this applies only to files found during source initialization; files discovered later are read
from the beginning unless a persisted position exists.
