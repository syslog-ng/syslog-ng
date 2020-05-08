3.27.1
======

## Highlights

 * `DESTIP/DESTPORT/PROTO`: new macros. ([#2899](https://github.com/syslog-ng/syslog-ng/pull/2899))
 * `set-facility()`: add new rewrite operation to change the syslog facility associated with the message. ([#3136](https://github.com/syslog-ng/syslog-ng/pull/3136))
 * `network tls`: Added `ca-file()` option. With this option the user can set a bundled CA-file to verify the peer. ([#3145](https://github.com/syslog-ng/syslog-ng/pull/3145))
 * `python-http-header`: It is possible with the Python HTTP header module to write custom HTTP response code handlers for `http`. ([#3159](https://github.com/syslog-ng/syslog-ng/pull/3159))

## Features

 * `DESTIP/DESTPORT/PROTO`: new macros

   These new macros express the destination ip, destination port and used protocol on a source.

   The use-case behind the PR is as follows:
   * someone has an appliance which sends out log messages via both UDP and TCP
   * the format of the two are different, and he wants to capture either with the simplest possible filter
   * `netmask()` doesn't work because the IP addresses are the same
   * `host()` doesn't work because the hostnames are the same

   Example:

   ```
   log {
     source { network(localip(10.12.15.215) port(5555) transport(udp)); };
     destination { file("/dev/stdout" template("destip=$DESTIP destport=$DESTPORT proto=$PROTO\n")); };
   };
   ```

   Output:

   ```
   destip=10.12.15.215 destport=5555 proto=17
   ```
   ([#2899](https://github.com/syslog-ng/syslog-ng/pull/2899))
 * `set-facility()`: add new rewrite operation to change the syslog facility
   associated with the message.

   ```
   log {
       source { system(); };
       if (program("postfix")) {
         rewrite { set-facility("mail"); };
       };
       destination { file("/var/log/mail.log"); };
       flags(flow-control);
   };
   ```
   ([#3136](https://github.com/syslog-ng/syslog-ng/pull/3136))
 * `network tls`: Added `ca-file()` option. With this option the user can set a bundled CA-file to verify the peer.
   ([#3145](https://github.com/syslog-ng/syslog-ng/pull/3145))
 * `http`: When a HTTP response is received, emit a signal with the HTTP response code.
   (Later it can be extended to read the response and parse it in a slot...).

   This PR also extends the Python HTTP header module with the possibility of writing custom HTTP response code handlers. When someone implements an auth header plugin in Python, it could be useful (for example invalidating a cache).

   <details>
     <summary>Example config, click to expand!</summary>

   ```

   @version: 3.25

   python {
   from syslogng import Logger

   logger = Logger()

   class TestCounter():
       def __init__(self, options):
           self.header = options["header"]
           self.counter = int(options["counter"])
           logger.debug(f"TestCounter class instantiated; options={options}")

       def get_headers(self, body, headers):
           logger.debug(f"get_headers() called, received body={body}, headers={headers}")

           response = ["{}: {}".format(self.header, self.counter)]
           self.counter += 1
           return response

       def on_http_response_received(self, http_code):
           self.counter += http_code
           logger.debug("HTTP response code received: {}".format(http_code))

       def __del__(self):
           logger.debug("Deleting TestCounter class instance")
   };

   source s_network {
     network(port(5555));
   };

   destination d_http {
       http(
           python_http_header(
               class("TestCounter")
               options("header", "X-Test-Python-Counter")
               options("counter", 11)
               # this means that syslog-ng will trying to send the http request even when this module fails
               mark-errors-as-critical(no)
           )
           url("http://127.0.0.1:8888")
       );
   };

   log {
       source(s_network);
       destination(d_http);
       flags(flow-control);
   };
   ```
   </details>

   ([#3159](https://github.com/syslog-ng/syslog-ng/pull/3159))
 * `java/python`: add support for the "arrow" syntax.

   ```
   options("key" => "value")
   ```
   ([#3161](https://github.com/syslog-ng/syslog-ng/pull/3161)) ([#3247](https://github.com/syslog-ng/syslog-ng/pull/3247))
 * `python`: persist support for python

   This feature enables users to persist data between reloads or restarts. The intended usage is to support bookmarking and acknowledgement in the future. It is not suitable for local database use cases.
   ([#3171](https://github.com/syslog-ng/syslog-ng/pull/3171))
 * `rewrite`: Added conditional `set-tag()` option. With this option the user can put condition statement inside set-tag option.

   ```
   rewrite { set-tag("tag" condition(match("test" value("MSG")))); };
   ```
   ([#3190](https://github.com/syslog-ng/syslog-ng/pull/3190))
 * `scl`: add sumologic destinations: `sumologic-syslog()` and `sumologic-http()`
   ([#3194](https://github.com/syslog-ng/syslog-ng/pull/3194))
 * `iterate`: new template function

   The iterate template function generates a series from an initial number and a `next` function.

   For example you can generate a sequence of nonnegative numbers with

   ```
   source {
     example-msg-generator(
       num(3)
       template("$(iterate $(+ 1 $_) 0)")
     );
   };
   ```
   ([#3205](https://github.com/syslog-ng/syslog-ng/pull/3205))
 * `telegram`: new `max-size` option

   Telegram message will be truncated for `max-size` size. Telegram does not accept message larger than 4096 utf8 characters. The default value is 4096.
   ([#3206](https://github.com/syslog-ng/syslog-ng/pull/3206))
 * `example-message-generator` : add support for `values(name1 => value1, name2 => value2,..)` syntax.

   Example

   ```
   @version: 3.27
   log {
     source { example-msg-generator(template("message parameter")
                                    num(10)
                                    values("PROGRAM" => "program-name"
                                           "current-second" => "$C_SEC"
                                   ));
            };
     destination { file(/dev/stdout template("$(format-json --scope all-nv-pairs)\n")); };
   };
   ```
   ([#3237](https://github.com/syslog-ng/syslog-ng/pull/3237))
 * `example-msg-generator`: support `freq(0)` for fast message generation

   ```
   log {
      source { example-msg-generator(freq(0) num(100)); };
      destination { file("/dev/stdout"); };
   };
   ```
   ([#3245](https://github.com/syslog-ng/syslog-ng/pull/3245))

## Bugfixes

 * `file`: changed `time-reap()` timer's schedule to respect the documentation (expires after last message)
   ([#3133](https://github.com/syslog-ng/syslog-ng/pull/3133))
 * `dbld`: fix building problems

   - fix rpm package build on centos-7
   - fix devshell image build
   - fix ubuntu-trusty image build
   - fix deb package build on ubuntu-trusty
   - fix rpm package build on fedora-30
   ([#3143](https://github.com/syslog-ng/syslog-ng/pull/3143))
 * `tls (network)`: Properly log an error message, when `key-file()` or `cert-file()` is missing.
   ([#3145](https://github.com/syslog-ng/syslog-ng/pull/3145))
 * `loggen`: fix crash with invalid parameterization
   ([#3146](https://github.com/syslog-ng/syslog-ng/pull/3146))
 * `format-json`: fix printing of embedded zeros

   Prior to 2.64.1, `g_utf8_get_char_validated()` in glib falsely identified embedded zeros as valid utf8 characters. As a result, format json printed the embedded zeroes as `\u0000` instead of `\x00`. This change fixes this problem.
   ([#3175](https://github.com/syslog-ng/syslog-ng/pull/3175))
 * `configure`: fix `--with-net-snmp` configure option
   ([#3180](https://github.com/syslog-ng/syslog-ng/pull/3180))
 * `python`: fix `Py_None` reference counting in logger methods (trace, debug, info, warning, error)
   ([#3187](https://github.com/syslog-ng/syslog-ng/pull/3187))
 * `afmongodb`: do not build module when `ENABLE_MONGODB=OFF`
   ([#3188](https://github.com/syslog-ng/syslog-ng/pull/3188))
 * `telegram`: automatically truncate messages larger than 4096 utf8 characters to avoid telegram destination to get stuck
   ([#3206](https://github.com/syslog-ng/syslog-ng/pull/3206))
 * `compat/glib`: fix recursive call issue on CentOS-6/RHEL-6/platforms
   ([#3212](https://github.com/syslog-ng/syslog-ng/pull/3212))
 * `timeutils`: fix crash in `%f` conversion when non-numeric character is in usec field (e.g. ".asd123")
   ([#3270](https://github.com/syslog-ng/syslog-ng/pull/3270))

## Packaging

 * `macOS`: add example startup configuration.
   ([#3172](https://github.com/syslog-ng/syslog-ng/pull/3172))
 * `rpm`: fix --without maxminddb option

   If maxminddb development package was installed on the build system: rpmbuild fails if `--without maxminddb` was used.
   ([#3208](https://github.com/syslog-ng/syslog-ng/pull/3208))

## Notes to developers

 * `light`: Support to relocate reports dir other than current base dir

   For example
   ```
   python -m pytest -lvs functional_tests/source_drivers/file_source/test_acceptance.py --installdir=/install --reports /tmp/
   ```
   ([#3157](https://github.com/syslog-ng/syslog-ng/pull/3157))
 * `CONTRIBUTING.md`: contribution guide updated
   ([#3174](https://github.com/syslog-ng/syslog-ng/pull/3174))
 * `libtest`: Now we install `config_parse_lib.h`, `fake-time.h`, `mock-cfg-parser.h` and `queue_utils_lib.h`
   which help unit testing outside of core.
   ([#3179](https://github.com/syslog-ng/syslog-ng/pull/3179))
 * `tests`: Wait until snmptrapd process able to write traps into output file
   ([#3185](https://github.com/syslog-ng/syslog-ng/pull/3185))
 * `mongodb`: Replaced the deprecated `get_server_status()` API with `command_simple()`.

   This means, that `syslog-ng` can now be built with `-Werror` on systems with 1.15 `libmongoc`.
   ([#3199](https://github.com/syslog-ng/syslog-ng/pull/3199))
 * `stats`: add external stats counters

   There are situations when someone wants to expose internal variables through stats-api.
   Without this changeset, we need two distinct variables: one is for the internal state, other is for the
   stats registration (internal state cannot depends on the life cycle of the `StatsCounterGroup`).
   ([#3225](https://github.com/syslog-ng/syslog-ng/pull/3225))

## Other changes

 * `afsnmp`: merge two existing SNMP modules (trapd-parser and destination) into one.

   Motivation: keep closely related modules together and decrease the number of small packages.

   Packaging related changes:

   libsnmptrapd-parser.so has been removed, both the snmp destination and the trapd parser are part of afsnmp.so .

    * deb: we already had mod-snmp which shipped snmp-dest. From now, this packages installs the snmptrapd-parser syslog-ng module. The syslog-ng-mod-snmptrapd-parser package has been removed.
    * rpm: snmpdest renamed to afsnmp and the snmptrapd-parser is installed by this package from now
   ([#3142](https://github.com/syslog-ng/syslog-ng/pull/3142))
 * `dbld`: remove `ubuntu-cosmic` as it has reached EOL
   ([#3143](https://github.com/syslog-ng/syslog-ng/pull/3143))
 * `afsocket-source`: present the number of connections in stats

   It helps in the debug process if we can see the number of source connections counted by syslog-ng internally.
   ([#3193](https://github.com/syslog-ng/syslog-ng/pull/3193))

## Credits

syslog-ng is developed as a community project, and as such it relies
on volunteers, to do the work necessarily to produce syslog-ng.

Reporting bugs, testing changes, writing code or simply providing
feedback are all important contributions, so please if you are a user
of syslog-ng, contribute.

We would like to thank the following people for their contribution:

Airbus Commercial Aircraft, Andras Mitzki, Antal Nemes, Attila Szakacs, Balazs Scheidler, Gabor Nagy, Kokan, Laszlo Budai, Laszlo Szemere, László Várady, Mehul Prajapati, Roberto Meléndez, Stephan Marwedel, Steven Haigh, Peter Czanik, Thomas De Schampheleire, Vatsal Sisodiya, Vivin Peris
