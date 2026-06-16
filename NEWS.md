4.12.0
======

syslog-ng provides [RPM](https://github.com/syslog-ng/syslog-ng#rhel) and [DEB](https://github.com/syslog-ng/syslog-ng#debianubuntu) package repositories for Ubuntu, Debian, and RHEL, for both amd64 and arm64 architectures.

We also provide ready-to-test binaries in [Docker containers](https://hub.docker.com/r/balabit/syslog-ng/tags) based on the current stable Debian image.

For more details, visit our [Documentation Center](https://syslog-ng.github.io/)

## Highlights

* `parallelize()`: Added `batch-size()` option

  `batch-size()` defines how many consecutive messages each input thread assigns to a single `parallelize()` worker.\
  This preserves ordering for those messages on the output side and can also improve the performance of `parallelize()`.
  ([#5654](https://github.com/syslog-ng/syslog-ng/pull/5654))

* `docker`: Added an Alma Linux docker image with the RPM based installation of syslog-ng. See [installation from Docker image](https://github.com/syslog-ng/syslog-ng#installation-from-docker-image) for details.
  ([#5705](https://github.com/syslog-ng/syslog-ng/pull/5705))

* `docker`: Docker images are now rebuilt weekly to include the latest security patches from the base image.
  ([#5665](https://github.com/syslog-ng/syslog-ng/pull/5665))

* `packaging`: Added Ubuntu Resolute based install packages. See [supported distributions](https://github.com/syslog-ng/syslog-ng#supported-distributions) for details.
  ([#5695](https://github.com/syslog-ng/syslog-ng/pull/5695))

## Features

* `afuser`: add escaping() option to usertty() output

  The usertty() destination now supports an escaping() option, using the same
  template escaping behavior as templates.
  ([#5713](https://github.com/syslog-ng/syslog-ng/pull/5713))

* `scl`: make syslogconf awk converter installation optional

  Add build options for both CMake and autotools to optionally omit
  installation of `scl/syslogconf/convert-syslogconf.awk`.

  The converter remains installed by default, preserving existing
  behavior for current users.
  ([#5702](https://github.com/syslog-ng/syslog-ng/pull/5702))

* `secure-logging`: add configure switch and disable by default

  The secure logging (slog) module and its command line tools
  (slogkey, slogencrypt, slogverify) are now build-conditional and
  disabled by default. Enable them with the `--enable-slog` /
  `--disable-slog` autotools switches, or with `-DENABLE_SLOG=ON` /
  `-DENABLE_SLOG=OFF` when building with CMake. The official DEB
  and RPM packages no longer ship slog; it can be re-enabled by
  building with the `sng-slog` Debian build profile or with
  `--with slog` on RPM.
  ([#5709](https://github.com/syslog-ng/syslog-ng/pull/5709))

* `journald-source`: add `read_old_on_error()` option to control where to continue after a position restore attempt failure
  ([#5648](https://github.com/syslog-ng/syslog-ng/pull/5648))

* `timeutils`: accept "UTC" in `date-parser()`'s `%z` / `%Z` format strings.
  ([#5637](https://github.com/syslog-ng/syslog-ng/pull/5637))

## Bugfixes

* `CVE-2026-39879`: fixed a possible SQL injection in syslog-ng SQL destionation driver

  Due to a missing sanitization call in afsql_dd_run_query, an SQL injection from an untrusted source might be possible. This is not part of the default configuration, the SQL driver has to be manually configured.
  ([#5696](https://github.com/syslog-ng/syslog-ng/pull/5696))

* `afsql`: fix segfault after database error
  ([#5696](https://github.com/syslog-ng/syslog-ng/pull/5696))

* `java/hdfs`: fix unreleased lock in `send()` when file open fails

  If `getHdfsFile()` returned `null`, the lock acquired at the start of
  `send()` was never released, causing a permanent deadlock on all
  subsequent calls.
  ([#5707](https://github.com/syslog-ng/syslog-ng/pull/5707))

* `stats-aggregator`: Fix use-after-free when an orphaned aggregator's timer fires after the aggregator is freed
  ([#5712](https://github.com/syslog-ng/syslog-ng/pull/5712))

* `stats-exporter()`: fixed the content-length value in the response header
  ([#5662](https://github.com/syslog-ng/syslog-ng/pull/5662))

* `filter-blank`: Fix race condition when evaluating from multiple threads

  The per-evaluation result was stored in a shared struct field, so
  concurrent worker threads could read each other's intermediate state,
  causing `blank()`/`not blank()` to return incorrect results.
  ([#5700](https://github.com/syslog-ng/syslog-ng/pull/5700))

* `correlation`: fix radix parser end-of-input handling

  Fixes two related radix matcher edge cases at end of input.

  Parser scans now stop before '\0' to avoid reading past end-of-input and to keep captured lengths correct.
  Parser-node traversal now continues with empty remaining input, so OPTIONALSET children can still match.
  ([#5690](https://github.com/syslog-ng/syslog-ng/pull/5690))

* `ivykis`: frequent SIGABRT on FreeBSD

  Fixed a [FreeBSD-specific issue](https://github.com/syslog-ng/syslog-ng/issues/4049) in our ivykys internal fork. ([#5690](https://github.com/syslog-ng/syslog-ng/pull/5690))

  NOTE: the fix has not yet been merged into the upstream ivykis repository, so it is currently available only in builds using the syslog-ng internal ivykis fork (`--with-ivykis=internal` or `-DIVYKIS_SOURCE=internal`). This includes our official DEB and RPM packages, as well as Docker images.
  If you are building syslog-ng from source with an external ivykis library, you will need to apply the patch manually until it is merged upstream.

* `pdbtool`: use fixed ISO-8601 timestamp format in patternize progress messages

  Patternize progress lines now use `YYYY-MM-DDTHH:MM:SS.UUUUUU` formatting instead of
  `ctime()` output, making them consistent with other syslog-ng message timestamps.
  This also avoids relying on `ctime()` in this path, reducing possible multithreading issues.
  ([#5697](https://github.com/syslog-ng/syslog-ng/pull/5697))

* `cfg`, `tls`: respect `perm()` when writing security-sensitive files

  The `--preprocess-into` config dump and the `tls(keylog-file())` output
  are now created via a new `file_perm_options_fopen()` helper that
  honours the global `perm()`/`owner()`/`group()` options with a `0600`
  floor. Previously both files inherited the process umask (typically
  `0644`); depending on the enclosing directory's permissions, this
  could leave config secrets and TLS session keys readable to other
  local users on the host.

  Note for admins: the helper opens these two files with `O_NOFOLLOW`,
  so if the target path is a symlink at the final component the open
  will now fail with `ELOOP` instead of writing through the link.
  Replace any such symlinks with the real destination path.
  ([#5701](https://github.com/syslog-ng/syslog-ng/pull/5701))

* `secure-logging`: new implementation of the pseudo-random function

  The previous implementation allowed an attacker to distinguish between
  the pseudo-random function (PRF) and a real random function by supplying
  specially crafted inputs to it. This leads to a predictable way of how
  the PRF is generating output which should not by allowed by a good PRF.
  The new implementation provides a variable input length and
  constant output length PRF based on AES CMAC for key derivation using
  the current key Ki, i.e. a key expansion of Ki using multiple iterations
  is performed.
  ([#5614](https://github.com/syslog-ng/syslog-ng/pull/5614))

* `http`: fixed a crash when syslog-ng built with http compression disabled
  ([#5648](https://github.com/syslog-ng/syslog-ng/pull/5648))

* `cfg-parser`: let the user adjust the parser stack size
  ([#5639](https://github.com/syslog-ng/syslog-ng/pull/5639))

* `tls-verifier`: fix leak in tls_verify_certificate_name
  ([#5635](https://github.com/syslog-ng/syslog-ng/pull/5635))

* `tls-verifier`: fix leak in tls_wildcard_match
  ([#5630](https://github.com/syslog-ng/syslog-ng/pull/5630))

* `afsql`: fix missing break in LM_VT_BOOLEAN case causing fallthrough to LM_VT_NULL
  ([#5626](https://github.com/syslog-ng/syslog-ng/pull/5626))

* `tls`: add NULL check after SSL_new() in tls_context_setup_session
  ([#5621](https://github.com/syslog-ng/syslog-ng/pull/5621))

## Notes to developers

* debun: fix possible hung issues related to syslog-ng-ctl
  ([#5680](https://github.com/syslog-ng/syslog-ng/pull/5680))
* tests: all remained old style functional tests are converted to light functional tests, the old style functional test folder is removed completely
  ([#5673](https://github.com/syslog-ng/syslog-ng/pull/5673))
* grpc: the minimum required C++ standard is now C++20 on some platforms, so the configuration flows trying to detect and use C++20 support. If C++20 is not available, the build will fall back to C++17 as before, but the build can fail depending on the platform and compiler versions.
  ([#5711](https://github.com/syslog-ng/syslog-ng/pull/5711))
* criterion: fixed criterion tests on macOS, do not use ParameterizedTests on that platform due to a known issue with the test framework.
  ([#5689](https://github.com/syslog-ng/syslog-ng/pull/5689))

## Other changes

* `stats-exporter()`: applied changes arte:
  - any internal request or response processing errors which cannot be responded with a valid HTTP response, will now log the error and close the connection
  - the SCL module single-instance() option is synced correctly with the stats-exporter module's single-instance() option, so the default value must be `yes` in every case
  - the response content-type is set according to the requested stat-format()
  - added an internal chunked response solution, so large responses should not cause stalls anymore
  - set the default scrape-freq-limit() to 15

  ([#5662](https://github.com/syslog-ng/syslog-ng/pull/5662))

## Credits

syslog-ng is developed as a community project, and as such it relies
on volunteers, to do the work necessarily to produce syslog-ng.

Reporting bugs, testing changes, writing code or simply providing
feedback are all important contributions, so please if you are a user
of syslog-ng, contribute.

We would like to thank the following people for their contribution:

Airbus Commercial Aircraft, Hofi, Bálint Horváth, Kevin Mainardis,
OvO, Tamas Pal, Romain Tartière, Alex Tristor, László Várady,
Alexander Yurkov, Akos Zalavary
