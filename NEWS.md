4.13.0
======

syslog-ng provides [RPM](https://github.com/syslog-ng/syslog-ng#rhel) and [DEB](https://github.com/syslog-ng/syslog-ng#debianubuntu) package repositories for Ubuntu, Debian, and RHEL, for both amd64 and arm64 architectures.

We also provide ready-to-test binaries in [Docker containers](https://hub.docker.com/u/balabit) based on the current stable Debian, and AlmaLinux 9 images.

For more details, visit our [Documentation Center](https://syslog-ng.github.io/)

## Highlights

  * `format-json`: Added the `--order` option to `$(format-json)` and `$(format-flat-json)` to control the order of
    the emitted keys. It accepts `descending` (the unchanged default), `ascending`, and `as-written`, the latter
    keeping the keys in their first-seen order instead of sorting them. A name that occurs more than once keeps its
    first position under `as-written` and takes its last value under every order.
    ([#5736](https://github.com/syslog-ng/syslog-ng/pull/5736))

  * `cloudwatchlogshlc`: Added CloudWatch Logs HLC destination

    New SCL destination that sends logs to the AWS CloudWatch Logs HTTP Log Collector (HLC)
    endpoint using bearer token authentication. No AWS SDK dependency required.
    ([#5738](https://github.com/syslog-ng/syslog-ng/pull/5738))

  * `cloud-auth`: Azure authentication now supports PEM RSA certificate credentials through `cert()` and `private-key()`,
    with an optional `private-key-passphrase()` for encrypted keys, as an alternative to `app-secret()`.
    Upload the matching public certificate to the Entra app registration; the private key remains on the syslog-ng host.

    The generic `oauth2()` authenticator gained the same capability through `auth-method(private-key-jwt)` together
    with `cert()`, `private-key()` and the optional `private-key-passphrase()` options. The client authenticates with an
    RFC 7523 JWT bearer client assertion signed by the private key instead of `client-secret()`.
    ([#5845](https://github.com/syslog-ng/syslog-ng/pull/5845))

## Features

  * `tls`: `trusted-keys()` accepts SHA-256 fingerprints (`"SHA256:AA:BB:..."`) next to the existing SHA-1 ones.
    ([#5811](https://github.com/syslog-ng/syslog-ng/pull/5811))

  * `logthrdest`: Exponential reconnect backoff with jitter is opt-in via a positive `time-reopen-max()`;
    when unset or zero, reconnects retain the fixed `time-reopen()` delay.
    With backoff enabled, delays range from half the capped exponential delay (rounded down, at least one second)
    to that delay, inclusive, so even the first retry can be shorter than `time-reopen()`.
    The cap is applied before jitter and may be smaller than `time-reopen()`; exponential growth stops after ten doublings.
    ([#5778](https://github.com/syslog-ng/syslog-ng/pull/5778))

  * `http`: `response-body-action()` classifies a 2xx response by its body

    For APIs that report a failure with a 2xx status code and the error in the response body, for
    example `200 {"Status": "Fail"}`: `"regexp" => success|retry|drop|disconnect` pairs, matched in
    order against the response body when the status code means success.

    The patterns are PCRE2 regular expressions compiled with default options: use the inline modifier
    `(?i)` for case-insensitive matching, for example `"(?i)status.*fail" => retry`. The first 64 KiB
    of the body are matched (only captured when the option is configured); a longer body is truncated
    and a debug message says so.
    ([#5803](https://github.com/syslog-ng/syslog-ng/pull/5803))

  * `cfg`, `logmsg`: Added the `store-debug-macros()` global option to control storing the `TRANSPORT` and `MSGFORMAT`
    macros in messages.
    ([#5781](https://github.com/syslog-ng/syslog-ng/pull/5781))

  * `options`: Added the `stack-alloc-limit()` global option. It lets users opt back into a faster,
    stack-based allocation for internal buffers whose size is derived from a configurable option (such as
    `log-msg-size()`), for buffers up to the configured limit. Defaults to `0`, meaning always use the heap;
    only raise this after validating your deployment's actual thread stack size, see the admin guide for the
    trade-offs.
    ([GHSA-v8gx-qpvj-wqr7](https://github.com/syslog-ng/syslog-ng/commit/38f6c8c4f9a5d9946f2dab28bd89346b42f958ca))

  * `network`, `syslog`: `extended-key-usage-verify()` in source `tls()` options

    `extended-key-usage-verify()` can now be used in the `tls()` options of sources too, to require
    the `clientAuth` extended key usage on client certificates (it was destination-only).
    ([#5804](https://github.com/syslog-ng/syslog-ng/pull/5804))

  * mod-python: replace `Logger` class with internal log handler

    Previous `Logger` python class is replaced with internal messages handler, which is installed
    into logging pipeline, allowing python modules to log messages via default logging facilities.
    Also, `TRACE` log level is now explicitly exported and could be used with `log()` methods or `trace()`
    call directly.
    ([#5526](https://github.com/syslog-ng/syslog-ng/pull/5526))

  * `http`: relax the worker-partition-key() requirement for message-independent URL templates

    The partition-key guard previously fired for any URL containing a `$` sign, even when
    the template was effectively constant (e.g. `$(url-encode literal)` after SCL backtick
    substitution). The check now uses a post-compile predicate that walks the template AST
    and only requires `worker-partition-key()` if the URL is genuinely message-dependent.
    ([#5740](https://github.com/syslog-ng/syslog-ng/pull/5740))

  * `logmsg`: Added dynamic name-value registry controls.

    New `dynamic-nv-name-limit()` and `dynamic-nv-name-overflow()` global options limit new dynamic field names.
    The current registry size is exposed through `logmsg_nv_registry_size` and `logmsg_dynamic_nv_registry_size` stats counters.
    ([GHSA-ch7v-cjmh-j4g2](https://github.com/syslog-ng/syslog-ng/commit/f853f8c77d5ada877f7507a669a29cec689b90b8))

  * `disk-buffer`: Reliable disk-queues now quarantine themselves after repeatedly crashing while
    processing the same unacked record. If the same read position survives 3 restarts in a row without
    being acked, the queue file is renamed to `.corrupted` and a fresh one is started, the same way an
    already-corrupted file is handled, instead of replaying the same poison record forever.
    ([#5793](https://github.com/syslog-ng/syslog-ng/pull/5793))

  * `disk-buffer`: added the `remove-if-empty()` option, which removes a queue file and its persist entry when the
    destination that owns it is torn down by `time-reap()` and the queue holds no messages. It defaults to `no`, and it is
    meant for templated destinations whose expansions never come back, such as a path built from a receive-time date macro,
    where every date roll would otherwise leave queue files behind permanently.
    ([#5749](https://github.com/syslog-ng/syslog-ng/pull/5749))

  * `file()`, `wildcard-file()`: Added support for skipping existing content with `read-old-records(no)` when no persisted
    position exists, while preserving saved positions and the skipped EOF across restarts.
    For `wildcard-file()`, this applies only to files found during source initialization; files discovered later are read
    from the beginning unless a persisted position exists.
    ([#5818](https://github.com/syslog-ng/syslog-ng/pull/5818))

  * `tls`: `pkcs12-file()` now supports password-protected PKCS#12 files. Like for `key-file()`, the password is
    added with `syslog-ng-ctl credentials add --id=<pkcs12-file path> --secret=<password>`; the source starts
    once it is available.
    ([#5810](https://github.com/syslog-ng/syslog-ng/pull/5810))

## Security Fixes

  * `logmsg`: Fixed unbounded dynamic name-value pair registration.
    The global registry now limits new dynamic field names from untrusted input.
    ([GHSA-ch7v-cjmh-j4g2](https://github.com/syslog-ng/syslog-ng/commit/f853f8c77d5ada877f7507a669a29cec689b90b8))

    CVE requested, identifier pending.

  * `logproto`: Fixed a potential crash when a source has `encoding()` set and `log-msg-size()` is configured large
    enough to overflow the io-worker thread's stack. The raw, pre-conversion read buffer used to be allocated
    directly on the stack, sized from `log-msg-size()` with no upper bound, regardless of the actual message
    content; it is now allocated on the heap instead, growing on demand and reused for the lifetime of the
    connection.
    ([GHSA-v8gx-qpvj-wqr7](https://github.com/syslog-ng/syslog-ng/commit/38f6c8c4f9a5d9946f2dab28bd89346b42f958ca))

    CVE requested, identifier pending.

  * `tls`: Fixed certificate hostname verification accepting names with an embedded NUL byte.
    A certificate subjectAltName or common name such as `good.example.com\0.evil.example.net`
    was compared as a C string and matched only the part before the NUL, letting a certificate
    issued for a different name pass the configured hostname check.
    ([GHSA-gx8q-j4c3-prc2](https://github.com/syslog-ng/syslog-ng/commit/1e335ad7782b58243fe8ca59b5baa5fc353ee10b))

    CVE requested, identifier pending.

## Bugfixes

  * `snmp`: Fixed a crash while parsing the configuration when `trap_obj()` had a very long type.
    ([#5809](https://github.com/syslog-ng/syslog-ng/pull/5809))


  * `diskq`: Validate disk-queue records before deserializing NVTable data.
    ([#5770](https://github.com/syslog-ng/syslog-ng/pull/5770))

  * `system-source`: Fixed parsing of FreeBSD kernel messages from `system()`.

    Kernel messages with colons in their payload no longer lose their prefix during parsing.
    ([#5782](https://github.com/syslog-ng/syslog-ng/pull/5782))

  * `logmsg`: Fixed a crash when deserializing a corrupted or tampered disk-buffer record whose
    structured-data handle did not correspond to any registered name, a potential heap buffer
    overflow when such a record's `sdata` entry count exceeded its declared allocation, and
    uninitialized NV handles left behind by the post-deserialization handle fixup for entries
    it could not resolve.
    ([#5791](https://github.com/syslog-ng/syslog-ng/pull/5791))

  * `logmsg`: Fixed `$SDATA` rendering a bogus `[none]` element when a value named exactly `.SDATA.` was set, and
    avoided an unneeded payload copy or growth when a shared message is modified.
    ([#5807](https://github.com/syslog-ng/syslog-ng/pull/5807))

  * `transport`: Fixed a PROXY protocol v2 length check that could abort syslog-ng when the
    header length exactly matched the staging buffer size. The request is now rejected cleanly
    instead of tripping the internal assertion.
    ([#5780](https://github.com/syslog-ng/syslog-ng/pull/5780))

  * `logmatcher`: fix named capture groups numbered 128 and above being captured incorrectly

    The PCRE named-group number was decoded through a signed character, so any named capture
    group whose number had its low byte >= 0x80 (128, 255, ...) sign-extended into a negative
    ovector index, producing an incorrect or garbage captured value. The decode is now unsigned.
    ([#5787](https://github.com/syslog-ng/syslog-ng/pull/5787))

  * `format-json`: Fixed nested `$(format-json)` so a key is no longer placed inside an object whose name is only a
    string prefix of it, for example `hostname` is no longer absorbed into a `host.*` object.
    ([#5736](https://github.com/syslog-ng/syslog-ng/pull/5736))

  * `grpc`: Fixed RSS leak on config reload in the BigQuery, ClickHouse, Loki,
    OpenTelemetry, and Pub/Sub destinations.
    ([#5756](https://github.com/syslog-ng/syslog-ng/pull/5756))

  * `tls`: Fix loading password-protected `pkcs12-file()` bundles without a MAC on older OpenSSL versions.

    The credential-store password now works for these bundles with OpenSSL 1.1.1 and 3.0 as well as newer
    OpenSSL 3.x and 4.x releases. Existing MACs are still verified, and the original bundle is not modified.
    ([#5819](https://github.com/syslog-ng/syslog-ng/pull/5819))

  * `crypto`: Fixed intermittent crashes during OpenSSL thread cleanup when syslog-ng shut down or restarted.
    ([#5775](https://github.com/syslog-ng/syslog-ng/pull/5775))

  * `file()`, `wildcard-file()`: Fixed missed appended records when automatic or legacy follow-method selection uses inotify,
    and an abort when automatic following probes a file that does not yet exist.
    ([#5818](https://github.com/syslog-ng/syslog-ng/pull/5818))

  * `syslogformat`: Fixed a heap over-read when parsing digit-only syslog input.

    This prevents an out-of-bounds read in the framing check for malformed
    octet-counted messages.
    ([#5779](https://github.com/syslog-ng/syslog-ng/pull/5779))

  * `regexp-parser`: Fixed a double free after a regular expression compilation failure.
    ([#5769](https://github.com/syslog-ng/syslog-ng/pull/5769))

  * `file()`, `wildcard-file()`: Fixed undetected file replacement or truncation when using
    `follow-method(system-poll)`. Previously the reader kept watching the stale, deleted
    inode instead of following the newly created file.
    ([#5827](https://github.com/syslog-ng/syslog-ng/pull/5827))

  * `java`: Due to a known JVM dlopen/dlclose limitation, `java` module has been disabled in the
    official binaries. The module is still available to compile.
    ([#5783](https://github.com/syslog-ng/syslog-ng/pull/5783))

  * `transport`: Fixed a PROXY protocol header announcing an IPv6 (TCP6) address that could abort
    syslog-ng when built without IPv6 support. The connection is now kept alive using the real
    endpoint address, logging a warning instead.
    ([#5790](https://github.com/syslog-ng/syslog-ng/pull/5790))

  * `graylog2()`, `$(format-gelf)`: Fixed sending only a NUL byte for messages with a non-numeric PID.

    The `_pid` field was cast with `int()`, which fails on a PID such as `worker-3`, so the whole JSON
    object was dropped. `_pid` is now only sent when the PID is plain decimal digits; otherwise it is
    left out and the message is sent.
    ([#5798](https://github.com/syslog-ng/syslog-ng/pull/5798))

  * `template`: Fixed correlation context references with more than one digit (`${MACRO}@10` and above), which
    pointed to the wrong context element.
    ([#5806](https://github.com/syslog-ng/syslog-ng/pull/5806))

  * `disk-buffer`: fixed a file descriptor leak in the disk-buffer file allocator. Every disk-buffer file created leaked
    one descriptor on the spool directory's lock file for the lifetime of the process, so a configuration with
    high-cardinality templated destinations could exhaust the process descriptor limit. Present since 4.2.0.
    ([#5762](https://github.com/syslog-ng/syslog-ng/pull/5762))

  * `network-dest`, `syslog-dest`: Fixed switching a UDP destination between `network()` and `syslog()` not taking
    effect on reload. Previously the old message format was kept until syslog-ng was fully restarted.
    ([#5784](https://github.com/syslog-ng/syslog-ng/pull/5784))

  * `logmsg`: Fixed a `guint16` wraparound in the per-message NVTable dynamic field index. A single
    message carrying 65536 or more distinct dynamic fields (via SDATA, key-value, or JSON parsing)
    could silently lose all of its previously parsed fields once the counter wrapped back to zero.
    The index is now capped instead of wrapping.
    ([#5792](https://github.com/syslog-ng/syslog-ng/pull/5792))

  * `graylog2()`, `$(format-gelf)`: Fixed sending only a NUL byte when `frac-digits()` is set.

    The GELF `timestamp` field was cast with `int64()`, which fails on a fractional `${R_UNIXTIME}`, so
    the whole JSON object was dropped and only the terminating NUL byte was sent to Graylog.
    The timestamp is now emitted as a JSON number as-is, keeping sub-second precision as GELF allows.
    ([#5797](https://github.com/syslog-ng/syslog-ng/pull/5797))

  * `logmatcher`: fix memory leaks in glob and PCRE matchers

    Fix a `GPatternSpec` leak when the glob matcher is recompiled, a NULL dereference
    in `glob_free()` when compile was never called, and two PCRE leaks: `pcre2_code`
    not freed on recompile, and `nv_prefix` not freed on destroy.
    ([#5761](https://github.com/syslog-ng/syslog-ng/pull/5761))

  * `kafka`: fix several source driver bugs

    Fix a crash when the configured `topic()` is empty, propagate persist-state init failures, plug leaks of the
    `rd_kafka_conf_t` and topic-partition list, and correct `logging()` option value matching.
    ([#5732](https://github.com/syslog-ng/syslog-ng/pull/5732))

  * `logwriter`: Fixed a reconnect hang that could occur when a reopen was deferred while the writer's I/O job was
    still in flight, leaving the writer with no proto and nothing to wake it up again. Also fixed a related issue
    where a stale reopen timer could outlive a subsequently installed valid connection and trigger a spurious,
    duplicate reconnect.
    ([#5763](https://github.com/syslog-ng/syslog-ng/pull/5763))

  * `logproto`: Fixed a `guint32` wraparound in the RFC6587 octet-counting ("framed" transport)
    frame-length parser. A declared frame length of 4294967296 or higher was silently reduced
    modulo 2^32 instead of being rejected, letting a single oversized frame be reinterpreted as
    multiple, differently-bounded messages.
    ([#5794](https://github.com/syslog-ng/syslog-ng/pull/5794))

  * `http`: Fixed a bug where a message retried after a critical header callback failure was appended to the
    already-finished request body. All per-request state is now reset before returning on such errors.
    ([#5777](https://github.com/syslog-ng/syslog-ng/pull/5777))

  * `geoip2`: Fixed incomplete geodata extraction when the first value is a preferred-language name or an empty container.

    The entry walker now produces its own status on every successful path and preserves errors from skipped subtrees.
    Traversal errors are logged without rejecting the original message.
    ([#5814](https://github.com/syslog-ng/syslog-ng/pull/5814))

  * `logmsg`: Fixed a potential heap corruption when deserializing a corrupted or tampered
    disk-buffer record whose tag-name length field was `0xFFFFFFFF`. This value could free an
    internal thread-local scratch buffer without invalidating it, corrupting state relied upon
    by later records processed by the same thread. Also fixed a related bookkeeping
    inconsistency left behind after reallocating that buffer, and a scratch-buffer resource
    leak on the rejected-input path.
    ([#5795](https://github.com/syslog-ng/syslog-ng/pull/5795))

  * `syslog-format`: Fixed a potential crash when parsing an invalid UTF-8 message with `flags(sanitize-utf8)` enabled.
    The sanitizer used to allocate a stack buffer sized directly from the (sender-controlled) message length; it now uses a bounded, pooled buffer regardless of message size.
    ([#5786](https://github.com/syslog-ng/syslog-ng/pull/5786))

  * `network-source`, `syslog-source`: Fixed a reload leaving already-open TCP connections on the
    stale wire protocol when switching between `network()` and `syslog()` (or any other
    transport/logproto change) on the same source.

    Kept-alive connections are now compared against the new configuration's protocol and
    transport name; a mismatching connection is discarded on reload instead of being silently
    reused with the old framing, and its list node is now properly freed instead of leaked.
    ([#5785](https://github.com/syslog-ng/syslog-ng/pull/5785))

  * `cryptofuncs`, `sql()`: Fixed `$(md5)` and `$(md4)` template functions, and the Oracle index name
    generation of `sql()`, on OpenSSL 3 systems running in FIPS mode. They now fetch these digests
    explicitly as non-security hashes, and no longer use an uninitialized buffer when the digest is not
    available. Template functions return an empty value and log an error if the digest computation fails;
    unavailable digest types are reported when the configuration is loaded.
    ([#5802](https://github.com/syslog-ng/syslog-ng/pull/5802))

## Credits

syslog-ng is developed as a community project, and as such it relies
on volunteers, to do the work necessarily to produce syslog-ng.

Reporting bugs, testing changes, writing code or simply providing
feedback are all important contributions, so please if you are a user
of syslog-ng, contribute.

We would like to thank the following people for their contribution:

0443n, Mark Atwood, C3B2W23, Andras Elso,
Hofi, Bálint Horváth, Mike Kazantsev, Peter Krawczyk,
Andras Lang, Nancy Laurenda, Chen Lu, Fairuzan Roslan,
Balazs Scheidler, Josef Schlehofer, Anton Skorup,
Balazs Szucs, Uli Westhoff, Akos Zalavary
