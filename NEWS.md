3.35.1
======

## syslog-ng OSE APT repository

From now on, Ubuntu and Debian packages will be published with every syslog-ng release in the form of an APT repository.

We, syslog-ng developers, provide these packages and the APT repository "as is" without warranty of any kind,
on a best-effort level.

Currently, syslog-ng packages are released for the following distribution versions (x86-64):
  - Debian: bullseye, buster, stretch, sid, testing
  - Ubuntu: Impish, Focal, Bionic, Xenial

For instructions on how to install syslog-ng on Debian/Ubuntu distributions, see the
[README](https://github.com/syslog-ng/syslog-ng/blob/master/README.md#debianubuntu).

## Highlights

 * `throttle()`: added a new `filter` that allows rate limiting messages based on arbitrary keys in each message.
   Note: messages over the rate limit are dropped (just like in any other filter).

   ```
   filter f_throttle {
     throttle(
       template("$HOST")
       rate(5000)
     );
   };
   ```
   ([#3781](https://github.com/syslog-ng/syslog-ng/pull/3781))

 * `mqtt()`: added a new `source` that can be used to receive messages using the MQTT protocol.
   Supported transports: `tcp`, `ws`, `ssl`, `wss`

   Example config:
   ```
   source {
       mqtt{
           topic("sub1"),
           address("tcp://localhost:4445")
       };
   };
   ```
   ([#3809](https://github.com/syslog-ng/syslog-ng/pull/3809))

## Features
 * `afsocket`: Socket options, such as ip-ttl() or tcp-keepalive-time(), are
   traditionally named by their identifier defined in socket(7) and unix(7) man
   pages.  This was not the case with the pass-unix-credentials() option, which -
   unlike other similar options - was also possible to set globally.

   A new option called so-passcred() is now introduced, which works similarly
   how other socket related options do, which also made possible a nice code
   cleanup in the related code sections.  Of course the old name remains
   supported in compatibility modes.

   The PR also implements a new source flag `ignore-aux-data`, which causes
   syslog-ng not to propagate transport-level auxiliary information to log
   messages.  Auxiliary information includes for example the pid/uid of the
   sending process in the case of UNIX based transports, OR the X.509
   certificate information in case of SSL/TLS encrypted data streams.

   By setting flags(ignore-aux-data) one can improve performance at the cost of
   making this information unavailable in the log messages received through
   affected sources.
   ([#3670](https://github.com/syslog-ng/syslog-ng/pull/3670))
 * `network`: add support for PROXY header before TLS payload

   This new transport method called `proxied-tls-passthrough` is capable of detecting the
   [PROXY header](http://www.haproxy.org/download/1.8/doc/proxy-protocol.txt) before the TLS payload.
   Loggen has been updated with the`--proxied-tls-passthrough` option for testing purposes.

   ```
   source s_proxied_tls_passthrough{
     network(
       port(1234)
       transport("proxied-tls-passthrough"),
       tls(
         key-file("/path/to/server_key.pem"),
         cert-file("/path/to/server_cert.pem"),
         ca-dir("/path/to/ca/")
       )
     );
   };
   ```
   ([#3770](https://github.com/syslog-ng/syslog-ng/pull/3770))
 * `mqtt() destination`: added `client-id` option. It specifies the unique client ID sent to the broker.
   ([#3809](https://github.com/syslog-ng/syslog-ng/pull/3809))

## Bugfixes

 * `unset()`, `groupunset()`: fix unwanted removal of values on different log paths

   Due to a copy-on-write bug, `unset()` and `groupunset()` not only removed values
   from the appropriate log paths, but from all the others where the same message
   went through. This has been fixed.
   ([#3803](https://github.com/syslog-ng/syslog-ng/pull/3803))
 * `regexp-parser()`: fix storing unnamed capture groups under `prefix()`
   ([#3810](https://github.com/syslog-ng/syslog-ng/pull/3810))
 * `loggen`: cannot detect plugins on platforms with non .so shared libs (osx)
   ([#3832](https://github.com/syslog-ng/syslog-ng/pull/3832))

## Packaging

 * `debian/control`: Added `libcriterion-dev` as a build dependency, where it is available from APT.
   (`debian-bullseye`, `debian-testing`, `debian-sid`)
   ([#3794](https://github.com/syslog-ng/syslog-ng/pull/3794))
 * `centos-7`: `kafka` and `mqtt` modules are now packaged.

   The following packages are used as dependencies:
    * `librdkafka-devel` from EPEL 7
    * `paho-c-devel` from copr:copr.fedorainfracloud.org:czanik:syslog-ng-githead
   ([#3797](https://github.com/syslog-ng/syslog-ng/pull/3797))
 * `debian`: Added bullseye support.
   ([#3794](https://github.com/syslog-ng/syslog-ng/pull/3794))
 * `bison`: support build with bison 3.8
   ([#3784](https://github.com/syslog-ng/syslog-ng/pull/3784))

## Notes to developers

 * `dbld`: As new distributions use python3 by default it makes sense to explicitly state older platforms which use python2
   instead of the other way around, so it is not necessary to add that new platform to the python3 case.
   ([#3780](https://github.com/syslog-ng/syslog-ng/pull/3780))
 * `dbld`: move dbld image cache from DockerHub to GitHub

   In 2021, GitHub introduced the GitHub Packages service. Among other
   repositories - it provides a standard Docker registry. DBLD uses
   this registry, to avoid unnecessary rebuilding of the images.
   ([#3782](https://github.com/syslog-ng/syslog-ng/pull/3782))

## Credits

syslog-ng is developed as a community project, and as such it relies
on volunteers, to do the work necessarily to produce syslog-ng.

Reporting bugs, testing changes, writing code or simply providing
feedback are all important contributions, so please if you are a user
of syslog-ng, contribute.

We would like to thank the following people for their contribution:

Andras Mitzki, Antal Nemes, Attila Szakacs, Balazs Scheidler,
Balázs Barkó, Benedek Cserhati, Colin Douch, Gabor Nagy, Laszlo Szemere,
László Várady, Norbert Takacs, Parrag Szilárd, Peter Czanik (CzP),
Peter Kokai, Robert Paschedag, Ryan Faircloth, Szilárd Parrag,
Thomas Klausner, Zoltan Pallagi
