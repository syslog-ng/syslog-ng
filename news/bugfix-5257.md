`s3`: Bugfixes and general stability improvements for the `s3` destination driver

Refactored the python `s3` destination driver to fix a major bug causing data loss if multithreaded upload was 
enabled via the `upload-threads` option.

This pull request also
* Fixes another bug generic to all python drivers, causing syslog-ng to intermittently crash if stopped by `SIGINT`.
* Adds a new suffix option to the `s3` destination driver. The default suffix is `.log`, denoting file extension.
* Removes more than 600 lines of superfluous code.
* Brings major stability improvements to the `s3` driver.

**Important:**
* This change affects the naming of multipart objects, as the sequence index is moved in front of the suffix.
* The `upload-threads` option is changed to act on a per-object basis, changing the maximum thread count 
dependent on `max-pending-uploads * upload-threads`. 
