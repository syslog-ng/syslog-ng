4.8.2
=====

This is a bug fix release.

## Bugfixes

  * CVE-2024-47619 fixed: When using a wildcard syntax in the configuration file to specify the tls
    certificate name, syslog-ng would match the wildcard  too loosely, accepting more than the intended
    certificate name. This could be exploited by knowing the original certificate name(s) and guessing
    the wildcard string used to match the correct certificate(s) and then creating fake certificates also
    satisfying the guessed wildcard sting using g_pattern_match_simple(). Since this exploit needs inside
    information, and does not lead to data loss or privileged access, it was deemed low impact.

  * `s3`: Bugfixes and general stability improvements for the `s3` destination driver

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
    ([#5257](https://github.com/syslog-ng/syslog-ng/pull/5257))

  * We forgot to update all the scl files we have after a bug fix in 4.8.1. Also adds a more detailed HTTP error
    response logging of compressed error response data. Fixes elasticsearch-http() and other destinations.
    ([#5232](https://github.com/syslog-ng/syslog-ng/pull/5232))

  * cfg: Fixed syslog-ng crashing on startup when using certain scl drivers without some options defined.
    ([#5163](https://github.com/syslog-ng/syslog-ng/pull/5163))


## Credits

syslog-ng is developed as a community project, and as such it relies
on volunteers, to do the work necessarily to produce syslog-ng.

Reporting bugs, testing changes, writing code or simply providing
feedback are all important contributions, so please if you are a user
of syslog-ng, contribute.

We would like to thank the following people for their contribution:

Bálint Horváth, David Mcanulty, Franco Fichtner, Hofi,
Kovács Gergő Ferenc, László Várady, Romain Tartière, Tamas Pal,
sulpher
