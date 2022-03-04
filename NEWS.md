3.36.1
======

## Highlights

 * `system()` source: added basic support for reading macOS system logs

   The current implementation processes the output of the original macOS syslogd:
   `/var/log/system.log`.
   ([#3710](https://github.com/syslog-ng/syslog-ng/pull/3710))
 * `$(values)` and `$(names)`: these new template functions can be used to
   query a list of name-value pairs in the current message. The list of name
   value pairs queried are specified by a value-pairs expression, just like
   with `$(format-json)`.

   Examples:

     This expression sets the JSON array `values` to contain the list of SDATA
     values, while the JSON array `names` would contain the associated names, in
     the same order.

     `$(format-json values=list($(values .SDATA.*)) names=list($(names .SDATA.*)))`

   The resulting name-value pairs are always sorted by their key, regardless of
   the argument order.
   ([#3911](https://github.com/syslog-ng/syslog-ng/pull/3911))
 * `rename()`: added a new rewrite rule, called `rename()`

   Example usage:
   ```
   rewrite {
     rename( "renamed-from" "renamed-to" );
   };
   ```
   ([#3841](https://github.com/syslog-ng/syslog-ng/pull/3841))

## Features

 * `network()` drivers: added TLS keylog support

   syslog-ng dumps TLS secrets for a given source/destination, which can be used for
   debugging purposes to decrypt data with, for example, Wireshark.

   **This should be used for debugging purposes only!**

   Example usage:
   ```
   source tls_source{
     network(
         port(1234)
         transport("tls"),
         tls(
           key-file("/path/to/server_key.pem"),
           cert-file("/path/to/server_cert.pem"),
           ca-dir("/path/to/ca/")
           keylog-file("/path/to/keylog_file")
         )
     );
   };
   ```
   ([#3792](https://github.com/syslog-ng/syslog-ng/pull/3792))
 * `tls()` block: added option for restricting TLS 1.3 ciphers

   The `network()`, `syslog()`, and the `http()` modules now support specifying TLS 1.3 cipher suites,

   Example usage:
   ```
   network(
     transport("tls")
     tls(
       pkcs12-file("test.p12")
       cipher-suite(
         tls12-and-older("ECDHE-ECDSA-AES128-GCM-SHA256:ECDHE-RSA-AES128-GCM-SHA256"),
         tls13("TLS_CHACHA20_POLY1305_SHA256:TLS_AES_256_GCM_SHA384")
       )
     )
   );
   ```

   `tls12-and-older()` can be used to specify TLS v1.2-and-older ciphers,
   `tls13()` can be used for TLS v1.3 ciphers only.

   Note: The old `cipher-suite("list:of:ciphers")` option restricts only the TLS v1.2-and-older cipher suite
   for backward compatibility.
   ([#3907](https://github.com/syslog-ng/syslog-ng/pull/3907))
 * `file()` destination: added a new option: `symlink-as()`

   This feature allows one to maintain a persistent symlink to a log file when a
   template is used (for example: `/var/log/cron -> /var/log/cron.${YEAR}${MONTH}`).

   Example usage:

   ```
   destination d_file_cron {
     file("/var/log/cron.${YEAR}${MONTH}" symlink-as("/var/log/cron"));
   };
   ```

   From a functional perspective, the `symlink-as` file inherits both
   `create-dirs` and file ownership from its file destination (permissions are not
   applicable to symlinks, at least on linux).

   The symlink is adjusted at the time a new destination file is opened (in the
   example above, if `${YEAR}` or `${MONTH}` changes).

   Although not specific to time macros, that's where the usefulness is. If the
   template contains something like `${PROGRAM}` or `${HOST}`, the configuration wouldn't
   necessarily be invalid, but you'd get an ever-changing symlink of dubious
   usefulness.
   ([#3855](https://github.com/syslog-ng/syslog-ng/pull/3855))
 * `flags(no-rfc3164-fallback)`: added a new flag to sources that parse
   incoming syslog data and operate in RFC5424 mode (e.g. `syslog-protocol` is
   also set). With the new flag the automatic fallback to RFC3164 format
   is disabled. In this case if the parsing in RFC5424 fails, the
   syslog parser would result in an error message. In the case of
   `syslog-parser(drop-invalid(yes))`, the message would be dropped.
   ([#3891](https://github.com/syslog-ng/syslog-ng/pull/3891))
 * `syslog-format`: accept ISO timestamps that incorrectly use a space instead of
   a 'T' to delimit the date from the time portion.  For example, a
   `"2021-01-01T12:12:12"` timestamp is well formed according to RFC5424 (which
   uses a subset of ISO8601, see https://datatracker.ietf.org/doc/html/rfc5424#section-6.2.3).
   Some systems simply use a space instead of a 'T'.  The same format is
   accepted for both RFC3164 (e.g.  `udp()`, `tcp()` and `network()` sources) and
   RFC5424 (e.g.  `syslog()` source).
   ([#3893](https://github.com/syslog-ng/syslog-ng/pull/3893))
 * `transport(text-with-nuls)`: added a new transport mechanism for
   the `network()` driver that allows `NUL` characters within the message.

   Note: syslog-ng does not support embedded `NUL` characters everywhere, so it is
   recommended that you also use `flags(no-multi-line)` that causes `NUL`
   characters to be replaced by space.
   ([#3913](https://github.com/syslog-ng/syslog-ng/pull/3913))


## Bugfixes

 * `filter`: fixed the `not` operator in `filter` expressions (regression in v3.35.1)

   Reusing a filter that contains the `not` operator more than once, or
   referencing a complex expression containing `not` might have caused invalid results
   in the previous syslog-ng version (v3.35.1).  This has been fixed.
   ([#3863](https://github.com/syslog-ng/syslog-ng/pull/3863))
 * `throttle()` filter: support negation
   ([#3863](https://github.com/syslog-ng/syslog-ng/pull/3863))
 * `disk-buffer()`: fixed a crash which could happen in very rare cases, while a corrupted `disk-buffer` was getting replaced
   ([#3845](https://github.com/syslog-ng/syslog-ng/pull/3845))
 * `disk-buffer()`: fixed a memory leak issue and inconsistent buffer handling in rare cases
   ([#3887](https://github.com/syslog-ng/syslog-ng/pull/3887))
 * `disk-buffer()`: fixed underflowing `queued` stats counter
   ([#3887](https://github.com/syslog-ng/syslog-ng/pull/3887))
 * `disk-buffer()`: fixed `queued` stats were not adjusted when a disk-buffer became corrupt
   ([#3851](https://github.com/syslog-ng/syslog-ng/pull/3851))
 * `disk-buffer()`: fixed a disk-buffer corruption issue

   A completely filled and then emptied disk-buffer may have been recognised as corrupt.
   ([#3874](https://github.com/syslog-ng/syslog-ng/pull/3874))
 * `amqp()`: fixed a minor error reporting problem.
   ([#3869](https://github.com/syslog-ng/syslog-ng/pull/3869))
 * `amqp()`: syslog-ng now drops messages that are too large to send
   ([#3869](https://github.com/syslog-ng/syslog-ng/pull/3869))
 * `amqp()`: fixed a crash, which happened with `librabbitmq` v0.9.0 or v0.10.0, while using the `tls()` block.
   ([#3929](https://github.com/syslog-ng/syslog-ng/pull/3929))
 * `file()` source: fixed invalid buffer handling when `encoding()` is used

   A bug has been fixed that - under rare circumstances - could cause message
   duplication or partial message loss when non-fixed length or less known
   fixed-length encodings are used.
   ([#3892](https://github.com/syslog-ng/syslog-ng/pull/3892))
 * `syslog-ng`: fixed a SIGSEGV triggered by an incorrectly formatted "CONFIG"
   command, received on the syslog-ng control socket.  The only known
   implementation of the control protocol is syslog-ng-ctl itself, which always
   sends a correct command, but anyone with access to the UNIX domain socket
   `syslog-ng.ctl` (root only by default) can trigger a crash.
   ([#3900](https://github.com/syslog-ng/syslog-ng/pull/3900))
 * `credit-card-mask()`: fixed visa, mastercard and jcb card regex pattern
   ([#3853](https://github.com/syslog-ng/syslog-ng/pull/3853))
 * `cisco-parser()`: allow a leading dot in the timestamp (not synced clocks)
   ([#3843](https://github.com/syslog-ng/syslog-ng/pull/3843))

## Notes to developers

 * plugins: we have made it easier to implement filter plugins

   An example can be found under `modules/rate-limit-filter`.
   ([#3866](https://github.com/syslog-ng/syslog-ng/pull/3866))
 * dev-utils: various fixes for the plugin skeleton generator script
   ([#3866](https://github.com/syslog-ng/syslog-ng/pull/3866))

## Other changes

 * The [syslog-ng Docker image](https://hub.docker.com/r/balabit/syslog-ng/)
   is now automatically tagged and pushed to Docker Hub after each release
   ([#3870](https://github.com/syslog-ng/syslog-ng/pull/3870))
 * `throttle()` filter: renamed to `rate-limit()`
   ([#3866](https://github.com/syslog-ng/syslog-ng/pull/3866))
 * `python`: support Python 3.10
   ([#3865](https://github.com/syslog-ng/syslog-ng/pull/3865))
 * `java`: upgraded from old log4j v1.x line to log4j v2.17.2
   ([#3861](https://github.com/syslog-ng/syslog-ng/pull/3861))
   ([#3927](https://github.com/syslog-ng/syslog-ng/pull/3927))

## Credits

syslog-ng is developed as a community project, and as such it relies
on volunteers, to do the work necessarily to produce syslog-ng.

Reporting bugs, testing changes, writing code or simply providing
feedback are all important contributions, so please if you are a user
of syslog-ng, contribute.

We would like to thank the following people for their contribution:

Andras Mitzki, Andrea Biardi, Attila Szakacs, Balazs Scheidler,
Balázs Barkó, Benedek Cserhati, Gabor Nagy, Janos SZIGETVARI,
Laszlo Budai, Laszlo Szemere, László Várady, Mikel Olasagasti Uranga,
Norbert Takacs, Parrag Szilárd, Peter Kokai, Szilárd Parrag,
Zoltan Pallagi, Stanislav Osipov, Yash Mathne
