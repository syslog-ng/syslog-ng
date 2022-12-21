4.0.1
=====

_This is the combination of the news entries of 4.0.0 and 4.0.1._

This is a new major version of syslog-ng, ending the 3.x series which
started roughly 13 years ago, on 17th February 2009.

Like all releases in the 3.x series, 4.0.0 is not a breaking change either.
Long-term compatibility has been and continues to be an essential objective
of syslog-ng; thus, you can still run unchanged configurations that were
originally created for syslog-ng 3.0.0.

You can safely upgrade to 4.0.0 if you followed along 3.x, and you should
probably also consider upgrading if you are stuck with an older 3.x release.

The new version number primarily indicates that this version of syslog-ng is
much more than the software we released 13 years ago.  While it does have
certain "big-bang" items in its feature list, new features were continuously
introduced throughout our 3.x series as well.  Our engineering practices
have not changed simply because we were working on a new major release: this
is the continuation of our previous releases in every respect, produced in
the same manner, just with a more catchy version number.

For this reason, there is no separate deprecation or support period for 3.x
releases, similarly with our existing practice.  We support earlier syslog-ng
releases by providing maintenance and fixes in the new release track.
Fixes to problems are not backported to earlier releases by the syslog-ng
project.

## Highlights

### Introduce runtime type information to name-value pairs

syslog-ng uses a data model where a log message contains an unordered set
of name-value pairs. The values stored in these name-value pairs are
usually textual, so syslog-ng has traditionally stored these values in
text format.

With the increase of JSON-based message sources and destinations, types
became more important.  If we encounter a message where a name-value pair
originates from a JSON document, and this document contains a member that
is numeric, we may want to reproduce that as we send this data to a
consumer.

For example, sometimes we extract a numerical metric from a log message,
and we need to send this to a consumer, again with the correct type.

To be able to do this, we added runtime type information to the syslog-ng
message model: each name-value pair becomes a (name, type, value) triplet.

We introduced the following types:
  - string: simple textual data, mostly utf8 (but not always)
  - int: an integer representable by a 64 bit signed value
  - double: a double precision floating point number
  - boolean: true or false
  - datetime: Date and Time represented by the milliseconds since epoch
  - list: list of strings
  - json: JSON snippet
  - null: an unset value

Apart from the syslog-ng core supporting the notion of types, its use is
up to the sources, filters, rewrite rules, parsers and destinations that
set or make use of them in any way it makes the most sense for the component
in question.

### Type-aware comparisons

syslog-ng uses filter expressions to make routing decisions and during the
transformation of messages.  These filter expressions are used in filter
{} or if {} statements, for example.

In these expressions, you can use comparison operators. This example, for
instance, uses the '>' operator to check for HTTP response codes
greater-or-equal than 500:

```
     if ("${apache.response}" >= 500) {
     };
```

Earlier, we had two sets of operators, one for numeric (==, !=, <, >) and the
other for string-based comparisons (eq, ne, gt, lt).

The separate operators were cumbersome to use. Users often forgot which
operator was the right one for a specific case.

Typing allows us to do the right thing in most cases automatically, and a
syntax that allows the user to override the automatic decisions in the
rare case.

With that, starting with 4.0, the old-numeric operators have been
converted to be type-aware operators. It would compare as strings if both
sides of the comparisons are strings. It would compare numerically if at
least one side is numeric. A great deal of inspiration was taken from
JavaScript, which was considered to be a good model, since the problem
space is similar.

See this blog post for more details:
  https://syslog-ng-future.blog/syslog-ng-4-progress-3-38-1-release/

### Capture type information from JSON

When using json-parser(), syslog-ng converts all members of a JSON object
to syslog-ng name-value pairs.  Prior to the introduction of type support,
these name-value pairs were all stored as strings.  Any type information
originally present in the incoming JSON object was lost.

This meant that if you regenerated the JSON from the name-value pairs using
the $(format-json) template function, all numbers, booleans and other
types became strings in the output.

There has been a feature in syslog-ng that alleviated the loss of types.
This feature was called "type-hints".  Type-hints tell $(format-json) to
use a specific type on output, independently of a name-value pair's
original type, but this type conversion needed to be explicit in the
configuration.

An example configuration that parses JSON on input and produces a JSON on
output:

```
log {
    source { ... };
    parser { json-parser(prefix('.json.')); };
    destination { file(... template("$(format-json .json.*)\n")); };
};
```

To augment the above with type hinting, you could use:

```
log {
    source { ... };
    parser { json-parser(prefix('.json.')); };
    destination { file(... template("$(format-json .json.* .json.value=int64(${.json.value})\n")); };
};
```

NOTE the presence of the int64() type hint in the 2nd example.

The new feature introduced with typing is that syslog-ng would
automatically store the JSON type information as a syslog-ng type, thus it
will transparently carry over types from inputs to output, without having
to be explicit about them.

### Typing support for various components in syslog-ng

Typing is a feature throughout syslog-ng, and although the gust of it has
been explained in the highlights section, some further details are
documented in the list down below:

  * type-aware comparisons in filter expressions: as detailed above, the
    previously numeric operators become type-aware, and the exact comparison
    performed will be based on types associated with the values we compare.

  * json-parser() and $(format-json): JSON support is massively improved
    with the introduction of types.  For one: type information is retained
    across input parsing->transformation->output formatting.  JSON lists
    (arrays) are now supported and are converted to syslog-ng lists so they
    can be manipulated using the $(list-*) template functions.  There are
    other important improvements in how we support JSON.

  * set(), groupset(): in any case where we allow the use of templates,
    support for type-casting was added, and the type information is properly
    promoted.

  * db-parser() type support: db-parser() gets support for type casts,
    <value> assignments within db-parser() rules can associate types with
    values using the "type" attribute, e.g.  `<value
    name="foobar" type="integer">$PID</value>`.  The “integer” is a type-cast that
    associates $foobar with an integer type.  db-parser()’s internal parsers
    (e.g.  @NUMBER@) will also associate type information with a name-value
    pair automatically.

  * add-contextual-data() type support: any new name-value pair that is
    populated using add-contextual-data() will propagate type information,
    similarly to db-parser().

  * map-value-pairs() type support: propagate type information

  * SQL type support: the sql() driver gained support for types, so that
    columns with specific types will be stored as those types.

  * template type support: templates can now be casted explicitly to a
    specific type, but they also propagate type information from
    macros/template functions and values in the template string

  * value-pairs type support: value-pairs form the backbone of specifying a
    set of name-value pairs and associated transformations to generate JSON
    or a key-value pair format.  It also gained support for types, the
    existing type-hinting feature that was already part of value-pairs was
    adapted and expanded to other parts of syslog-ng.

  * `python()` typing: support for typing was added to all Python components
    (sources, destinations, parsers and template functions), along with more
    documentation & examples on how the Python bindings work.  All types except
    json() are supported as they are queried- or changed by Python code.

  * on-disk serialized formats (e.g.  disk buffer/logstore): we remain
    compatible with messages serialized with an earlier version of
    syslog-ng, and the format we choose remains compatible for “downgrades”
    as well.  E.g.  even if a new version of syslog-ng serialized a message,
    the old syslog-ng and associated tools will be able to read it (sans
    type information of course)

### Improved support for lists (arrays)

For syslog-ng, everything is traditionally a string.  A convention was
started with syslog-ng in v3.10, where a comma-separated format
could be used as a kind of array using the `$(list-*)` family of template
functions.

For example, $(list-head) takes off the first element in a list, while
$(list-tail) takes the last.  You can index and slice list elements using
the $(list-slice) and $(list-nth) functions and so on.

syslog-ng has started to return such lists in various cases, so they can
be manipulated using these list-specific template functions.  These
include the xml-parser(), or the $(explode) template function, but there
are others.

Here is an example that has worked since syslog-ng 3.10:

  ```
    # MSG contains foo:bar:baz
    # - the $(list-head) takes off the first element of a list
    # - the $(explode) expression splits a string at the specified separator, ':' in this case.
    $(list-head $(explode : $MSG))
  ```

New functions that improve these features:
  - JSON arrays are converted to lists, making it a lot easier to slice
    and extract information from JSON arrays.  Of course, $(format-json)
    will take lists and convert them back to arrays.

  - The $* is a new macro that converts the internal list of match
    variables ($1, $2, $3 and so on) to a list, usable with $(list-*)
    template functions.  These match variables have traditionally been
    filled by regular expressions when a capture group in a regexp
    matches.

  - The set-matches() rewrite operation performs the reverse; it assigns
    the match variables to list elements, making it easier to use list
    elements in template expressions by assigning them to $1, $2, $3 and
    so on.

  - Top-level JSON arrays (e.g.  ones where the incoming JSON data is an
    array and not an object) are now accepted, and the array elements are
    assigned to the match variables.


### Python support

syslog-ng has had support for Python-based processing elements since 3.7,
released in 2015, which was greatly expanded early 2017 (3.9, LogParser) and
late 2018 (3.18, LogSource and LogFetcher).

This support has now been improved in a number of ways to make its use both
easier and its potential more powerful.

A framework was added to syslog-ng that allows seamless implementation of
syslog-ng features in Python, with a look and feel of that of a native
implementation.  An example for using this framework is available in the
`modules/python-modules/example` directory, as well as detailed
documentation in the form of modules/python-modules/README.md that is
installed to /etc/syslog-ng/python.

The framework consists of these changes:

  * `syslogng` Python package: native code provided by the syslog-ng core
    has traditionally been exported in the `syslogng` Python module.  An
    effort was made to make these native classes exported by the C layer
    more discoverable and more intuitive.  As a part of this effort, the
    interfaces for all key Python components (LogSource, LogFetcher,
    LogDestination, LogParser) were exposed in the syslogng module, along
    with in-line documentation.

  * `/etc/syslog-ng/python`: syslog-ng now automatically adds this directory to
    the PYTHONPATH so that you have an easy place to add Python modules required
    by your configuration.

  * Python virtualenv support for production use: more sophisticated Python
    modules usually have 3rd party dependencies, which either needed to be
    installed from the OS repositories (using the apt-get or yum/dnf tools) or
    PyPI (using the pip tool).  syslog-ng now acquired support for an embedded
    Python virtualenv (/var/lib/syslog-ng/python-venv or similar, depending on
    the installation layout), meaning that these requirements can be installed
    privately, without deploying them in the system PYTHONPATH where it might
    collide with other applications.  The base set of requirements that
    syslog-ng relies on can be installed via the `syslog-ng-update-virtualenv`
    script, which has been added to our rpm/deb postinst scripts.

    Our mod-python module validates this virtualenv at startup and activates it
    automatically if the validation is successful.  You can disable this behaviour
    by loading the Python module explicitly with the following configuration
    statement:

            @module mod-python use-virtualenv(no)

    You can force syslog-ng to use a specific virtualenv by activating it first,
    prior to executing syslog-ng.  In this case, syslog-ng will not try to use
    its private virtualenv, rather it would use the one activated when it was
    started.  It assumes that any requirements needed for syslog-ng
    functionality implemented in Python are deployed by the user. These
    requirements are listed in the /usr/lib/syslog-ng/python/requirements.txt
    file.

  * SCL snippets in Python plugins: by adding an `scl/whatever.conf` file to
    your Python-based syslog-ng plugin, you can easily wrap a Python-based
    log processing functionality with a syslog-ng block {}, so the user can
    use a syntax very similar to native plugins in their main configuration.

  * `confgen` in Python: should a simple block {} statement not be enough to
     wrap the functionality implemented in Python, the mod-python module now
     supports confgen functions to be implemented in Python.  confgen
     has been a feature in syslog-ng for a long time that allows you to
     generate configuration snippets dynamically by executing an external
     program or script.  This has now been ported to Python, e.g.
     syslog-ng can invoke a Python function to generate parts of its
     configuration.

     Example:

      ```
      @version: 4.0
      python {
      from syslogng import register_config_generator
      def generate_foobar(args):
              print(args)
              return "tcp(port(2000))"
      #
      # this registers a plugin in the "source" context named "foobar"
      # which would invoke the generate_foobar() function when a foobar() source
      # reference is encountered.
      #
      register_config_generator("source", "foobar", generate_foobar)
      };
      log {
              # we are actually calling the generate_foobar() function in this
              # source, passing all parameters as values in the "args" dictionary
              source { foobar(this(is) a(value)); };
              destination { file("logfile"); };
      };
      ```

## Features

  * `kubernetes()` source and `kubernetes-metadata-parser()`: these two
    components gained the ability to enrich log messages with Kubernetes
    metadata.  When reading container logs, syslog-ng would query the Kubernetes
    API for the following fields and add them to the log-message.  The returned
    meta-data is cached in memory, so not all log messages trigger a new query.

        .k8s.pod_uuid
        .k8s.labels.<label_name>
        .k8s.annotations.<annotation_name>
        .k8s.namespace_name
        .k8s.pod_name
        .k8s.container_name
        .k8s.container_image
        .k8s.container_hash
        .k8s.docker_id

  * `java()` destinations: fixed compatibility with newer Java versions,
    syslog-ng is now able to compile up to Java 18.

  * `disk-buffer`: Added `prealloc()` option to preallocate new disk-buffer
    files.
    ([#4056](https://github.com/syslog-ng/syslog-ng/pull/4056))

  * `disk-buffer`: The default value of `truncate-size-ratio()` has been changed to 1,
    which means truncation is disabled by default. This means that by default, the
    disk-buffer files will gradually become larger and will never reduce in size.
    This improves performance.
    ([#4056](https://github.com/syslog-ng/syslog-ng/pull/4056))

  * `log-level()`: added a new global option to control syslog-ng's own internal
    log level.  This augments the existing support for doing the same via the
    command line (via -d, -v and -t options) and via syslog-ng-ctl.  This change
    also causes higher log-levels to include messages from lower log-levels,
    e.g.  "trace" also implies "debug" and "verbose".  By adding this capability
    to the configuration, it becomes easier to control logging in containerized
    environments where changing command line options is more challenging.

    `syslog-ng-ctl log-level`: this new subcommand in syslog-ng-ctl allows
    setting the log level in a more intuitive way, compared to the existing
    `syslog-ng-ctl verbose|debug|trace -s` syntax.

    `syslog-ng --log-level`: this new command line option for the syslog-ng
    main binary allows you to set the desired log-level similar to how you
    can control it from the configuration or through `syslog-ng-ctl`.
    ([#4091](https://github.com/syslog-ng/syslog-ng/pull/4091))

  * `network`/`syslog`/`tls` context options: SSL_CONF_cmd support

    SSL_CONF_cmd TLS configuration support for `network()` and `syslog()` driver has been added.

    OpenSSL offers an alternative, software-independent configuration
    mechanism through the SSL_CONF_cmd interface to support a common
    solution for setting the so many various SSL_CTX and SSL options that
    can be set earlier via multiple, separated openssl function calls only.
    This update implements that similar to the mod_ssl in Apache.

    IMPORTANT: The newly introduced `openssl-conf-cmds` always has the
    highest priority, its content parsed last, so it will override any other
    options that can be found in the `tls()` section, does not matter if
    they appear before or after `openssl-conf-cmds`.

    As described in the SSL_CONF_cmd documentation, the order of operations
    within openssl-conf-cmds() is significant and the commands are executed
    in top-down order.  This means that if there are multiple occurrences of
    setting the same option then the 'last wins'.  This is also true for
    options that can be set multiple ways (e.g.  used cipher suites and/or
    protocols).

    Example config:
    ```
    source source_name {
        network (
            ip(0.0.0.0)
            port(6666)
            transport("tls")
            tls(
                ca-dir("/etc/ca.d")
                key-file("/etc/cert.d/serverkey.pem")
                cert-file("/etc/cert.d/servercert.pem")
                peer-verify(yes)

                openssl-conf-cmds(
                    # For system wide available cipher suites use: /usr/bin/openssl ciphers -v
                    # For formatting rules see: https://www.openssl.org/docs/man1.1.1/man3/SSL_CONF_cmd.html
                    # For quick and dirty testing try: https://github.com/rbsec/sslscan
                    #
                    "CipherString" => "ECDHE-RSA-AES128-SHA",                                   # TLSv1.2 and bellow
                    "CipherSuites" => "TLS_CHACHA20_POLY1305_SHA256:TLS_AES_256_GCM_SHA384",    # TLSv1.3+ (OpenSSl 1.1.1+)

                    "Options" => "PrioritizeChaCha",
                    "Protocol" => "-ALL,TLSv1.3",
                )
            )
        );
    };
    ```


  * `network`/`syslog`/`http` destination: OCSP stapling support

    OCSP stapling support for network destinations and for the `http()` module has been added.

    When OCSP stapling verification is enabled, the server will be requested to send back OCSP status responses.
    This status response will be verified using the trust store configured by the user (`ca-file()`, `ca-dir()`, `pkcs12-file()`).

    Note: RFC 6961 multi-stapling and TLS 1.3-provided multiple responses are currently not validated, only the peer certificate is verified.

    Example config:
    ```
    destination {

        network("test.tld" transport(tls)
            tls(
                pkcs12-file("/path/to/test.p12")
                peer-verify(yes)
                ocsp-stapling-verify(yes)
            )
        );

        http(url("https://test.tld") method("POST") tls(peer-verify(yes) ocsp-stapling-verify(yes)));
    };
    ```
    ([#4082](https://github.com/syslog-ng/syslog-ng/pull/4082))


  * Python `LogMessage` class: get_pri() and get_timestamp() methods were added that
    allow the query of the syslog-style priority and the message timestamp,
    respectively. The return value of get_pri() is an integer, while
    get_timestamp() returns a Python datetime.datetime instance. Some macros
    that were previously unavailable from Python (e.g. the STAMP, R_STAMP and
    C_STAMP macros) are now made available.

  * Python `Logger`: the low-level Logger class exported by syslog-ng was
    wrapped by a logging.LogHandler class so that normal Python APIs for logging
    can now be used.

  * `db-parser()` and `grouping-by()`: added a `prefix()` option to both
    `db-parser()` and `grouping-by()` that allows specifying an extra prefix
    to be prepended to all name-value pairs that get extracted from messages
    using patterns or <value> tags.

  * `csv-parser()`: add a new dialect, called escape-backslash-with-sequences
    which uses "\" as an escape character but also supports C-style escape
    sequences, like "\n" or "\r".

## Bugfixes

  * `tcp()`, `network()` or `syslog()` destinations: fixed a crash that could
    happen after reload when a kept-alive connection is terminated, in case
    the target server is configured using a hostname (and not an IP address)
    and that name becomes unresolvable (e.g. dropped from DNS or /etc/hosts)
    ([#4044](https://github.com/syslog-ng/syslog-ng/pull/4044))

  * `python()` destination: Fixed a crash, when trying to resolve the
    "R_STAMP", "P_STAMP" or "STAMP" macros from Python code.
    ([#4057](https://github.com/syslog-ng/syslog-ng/pull/4057))

  * Python `LogSource` & `LogFetcher`: a potential deadlock was fixed in
    acknowledgement tracking.

  * Python `LogTemplate`: the use of template functions in templates
    instantiated from Python caused a crash, which has been fixed.

  * `grouping-by()` persist-name() option: fixed a segmentation fault in the
    grammar.
    ([#4180](https://github.com/syslog-ng/syslog-ng/pull/4180))

  * `$(format-json)`: fix a bug in the --key-delimiter option introduced in
    3.38, which causes the generated JSON to contain multiple values for the
    same key in case the key in question contains a nested object and
    key-delimiter specified is not the dot character.
    ([#4127](https://github.com/syslog-ng/syslog-ng/pull/4127))

  * `add-contextual-data()`: add compatibility warnings and update advise in
    case of the value field of the add-contextual-data() database contains an
    expression that resembles the new type-hinting syntax: type(value).

  * `syslog-ng --help` screen: the output for the --help command line option has
    included sample paths to various files that contained autoconf style
    directory references (e.g. ${prefix}/etc for instance). This is now fixed,
    these paths will contain the expanded path. Fixes Debian Bug report #962839:
    https://bugs.debian.org/cgi-bin/bugreport.cgi?bug=962839
    ([#4143](https://github.com/syslog-ng/syslog-ng/pull/4143))

  * `csv-parser()`: fixed the processing of the dialect() parameter, which was
    not taken into consideration.

  * `apache-accesslog-parser()`: Apache may use backslash-style escapes in the
    `request` field, so support it by setting the csv-parser() dialect to
    `escape-backslash-with-sequences`.  Also added validation that the
    `rawrequest` field contains a valid HTTP request and only extract `verb`,
    `request` and `httpversion` if this is the case.

  * `riemann`: fixed severity levels of Riemann diagnostics messages, the error
    returned by riemann_communicate() was previously only logged at the trace
    level and was even incomplete: not covering the case where
    riemann_communicate() returns NULL.
    ([#4238](https://github.com/syslog-ng/syslog-ng/pull/4238))

## Packaging

  * `python`: python2 support is now completely removed.  `syslog-ng` can no
    longer be configured with `--with-python=2`.
    ([#4057](https://github.com/syslog-ng/syslog-ng/pull/4057))

  * `python`: Python 2 support is now completely removed from the syslog-ng
    functional test framework, called Light, too. Light will support only Python 3
    from now.
    ([#4174](https://github.com/syslog-ng/syslog-ng/pull/4174))

  * Python virtualenv support for development use: syslog-ng is now capable of
    using a build-time virtualenv, where all Python development tools are
    automatically deployed by the build system.  You can control if you want to
    use this using the --with-python-packages configure option. There are
    three possible values for this parameter:
       * `venv`: denoting that you want to use the virtualenv and install
         all these requirements automatically using pip, into the venv.
       * `system`: meaning that you want to rely on the system Python
         without using a virtualenv. syslog-ng build scripts would install
         requirements automatically to the system Python path usually
         `/usr/local/lib/pythonX.Y`
       * `none`: disable deploying packages automatically. All
         dependencies are assumed to be present in the system Python before
         running the syslog-ng build process.

    Please note that syslog-ng has acquired quite a number of these
    development time dependencies with the growing number of functionality
    the Python binding offers, so using the `system` or `none` settings are
    considered advanced usage, meant to be used for distro packaging.

  * `make dist`: fixed make dist of FreeBSD so that source tarballs can
    easily be produced even if running on FreeBSD.
    ([#4163](https://github.com/syslog-ng/syslog-ng/pull/4163))

  * Debian and derivatives: The `syslog-ng-mod-python` package is now built with `python3` on the following platforms:
      * `debian-stretch`
      * `debian-buster`
      * `ubuntu-bionic`
    ([#4057](https://github.com/syslog-ng/syslog-ng/pull/4057))

  * `dbld`: Removed support for `ubuntu-xenial`.
    ([#4057](https://github.com/syslog-ng/syslog-ng/pull/4057))

  * `dbld`: Updated support from Fedora 35 to Fedora 37

  * Leaner production docker image: the balabit/syslog-ng docker image stops
    pulling in logrotate and its dependencies into the image. logrotate
    recursively pulled in cron and exim4 which are inoperable within the
    image anyway and causes the image to be larger as well as increasing the
    potential attack surface.

  * Debian packaging: logrotate became Suggested instead of Recommended to
    avoid installing logrotate by default.

  * `scl`: To match the way `scl`s are packaged in debian, we have added a `syslog-ng-scl` package.
    This makes it possible to upgrade from the official debian `syslog-ng` package to the ose-repo provided one.
    ([#4252](https://github.com/syslog-ng/syslog-ng/pull/4252)) ([#4256](https://github.com/syslog-ng/syslog-ng/pull/4256))

## Other changes

  * `sumologic-http()` improvements

    Improved defaults: `sumologic-http()` originally sent incomplete
    messages (only the `$MESSAGE` part) to Sumo Logic by default.  The new
    default is a JSON object, containing all name-value pairs.  This is a
    breaking change if you used the default value as it was, but this is not
    really anticipated.  To override the new message format or revert to the
    old default, the `template()` option can be used.

    `sumologic-http()` enables batching by default to significantly increase
    the destination's performance.

    The `tls()` block has become optional, Sumo Logic servers will be
    verified using the system's certificate store by default.
    ([#4124](https://github.com/syslog-ng/syslog-ng/pull/4124))


### Installation packages

  * Debian/Ubuntu Packages
    https://github.com/syslog-ng/syslog-ng#debianubuntu

  * RedHat, CentOS and Fedora Packages
    https://www.syslog-ng.com/community/b/blog/posts/rpm-packages-from-syslog-ng-git-head/

  * docker image: Nightly production docker images are now available as `balabit/syslog-ng:nightly`
    ([#4117](https://github.com/syslog-ng/syslog-ng/pull/4117))

  * docker image: added jemalloc to the production image, which improves
    performance, decreases memory fragmentation and makes syslog-ng to
    return memory to the system much more aggressively.

  * Removed support for Debian stretch.


## Credits

syslog-ng is developed as a community project, and as such it relies
on volunteers, to do the work necessarily to produce syslog-ng.

Reporting bugs, testing changes, writing code or simply providing
feedback are all important contributions, so please if you are a user
of syslog-ng, contribute.

We would like to thank the following people for their contribution:

Andras Mitzki, Attila Szakacs, Attila Szalay, Balazs Scheidler, Bálint
Horváth, Gabor Nagy, István Hoffmann, Joshua Root, László Várady, Szilárd
Parrag
