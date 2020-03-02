3.26.1
======

## Highlights

<Fill this block manually from the blocks below>

## Features

 * `http`: Added `ssl_version("tlsv1_3")` and `ssl_version("no-tlsv13")` options to respectively force and disable TLSv1.3. (#3063)
 * `scl`: Improved error message at init, when an `scl` is missing a dependency. (#3015)
 * 
   The `python-http-header` plugin makes it possible for users to implement HTTP header plugins in Python language. It is built on top of signal-slot mechanism: currently HTTP module defines only one signal, that is `signal_http_header_request` and `python-http-header` plugin implements a python binding for this signal. This means that when the `signal_http_header_request` signal is emitted then the connected slot executes the Python code.
   
   The Python interface is :
   ```
   def get_headers(self, body, headers):
   ```
   
   It should return string List. The headers that will be appended to the request's header.
   When the plugin fails, http module won't0 try to send the http request without the header items by default.
   If you want http module to trying to send the request without these headers, just disble `mark-errors-as-critical` function.
    
   Original code was written by Ferenc Sipos.
   
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
   </details> (#3123)
 * `azure-auth-header`: Azure auth header is a plugin that generates
   authorization header for applications connecting to Azure.
   It can be used as a building block in higher level SCLs.
   Implemented as a `signal-slot` plugin.
   
   <details>
     <summary>Example config, click to expand!</summary>
   
   ```
   @version:3.25
   @include "scl.conf"
   destination d_http {
     http(
       url("http://127.0.0.1:8888")
       method("PUT")
       user_agent("syslog-ng User Agent")
       body("${ISODATE} ${MESSAGE}")
       azure-auth-header(
         workspace-id("workspace-id")
         secret("aa1a")
         method("POST")
         path("/api/logs")
         content-type("application/json")
       )
     );
   };
   source s_gen {
     example-msg-generator(num(1) template("Test message\n"));
   };
   log {
     source(s_gen);
     destination(d_http);
   };
   ```
   </details> (#3078)
 * `$(list-search)`: Added a new template function, which returns the first index of a pattern in a list.
   Starts the search at `start_index`. 0 based. If not found, returns empty string.
   Usage:
    * Literal match:
        `$(list-search <pattern> ${list})`
        or
        `$(list-search --mode literal <pattern> ${list})`
    * Prefix match: `$(list-search --mode prefix <pattern> ${list})`
    * Substring match: `$(list-search --mode substring <pattern> ${list})`
    * Glob match: `$(list-search --mode glob <pattern> ${list})`
    * Regex match: `$(list-search --mode pcre <pattern> ${list})`
   Add `--start-index <index>` to change the start index. (#2955)
 * `geoip2`: Added `template()` option as an alias for the positional argument string, to match the grammar convention. (#3051)
 * `python`: allow specifying persist name from python.
   
   From now on users can specify a persist name template from python code.
   
   ```
   @staticmethod
   def generate_persist_name(options):
       return options["file_name"]
   ```
   
   Currently, in python destination, persist name is generated from the python class name. This can cause inconvenience if the same python class is planned to be used multiple times. For example if someone writes a PythonFileDestination, then the persist name would be python(PythonFileDestination). If a user wants to track many files, that causes persist collision, that needs to be resolved manually in configuration. After this change, module developers can specify a persist name template, that can be generate persist name uniquely (file name can be in included during generation)... Hence there is no need for users to manually resolve persist name collision. Similar example works with python source and fetcher too.
   
   Persist name from config takes precedence over `generate_persist_name`.
   
   Persist name is exposed through `self.persist_name`. (#3016)
 * `set-severity()`: new rewrite rule for changing message severity
   
   `set-severity()` receives a template and sets message severity by evaluating the
   template.
   
   Numerical and textual severity levels are both supported.
   
   Examples:
   
   ```
   rewrite {
     set-severity("info");
     set-severity("6");
     set-severity("${.json.severity}");
   };
   ``` (#3115)
 * `config-option-database`: Added support for `parser`, `diskq` and `hook-commands` blocks. (#3029)
 * `config version`: make the config version check of the configuration more
   liberal by accepting version numbers that had no changes relative to the
   current version.  This means that if you are running 3.25 and the last
   semantic change in the configuration was 3.22, then anything between 3.22
   and 3.25 (inclusive) is accepted by syslog-ng without a warning at startup. (#3074)
 * `loggly`: Added `transport()` option, so users can now use it with `tls` (or any `network()` supported transport). (#3149)
 * `$SEVERITY instead of $LEVEL`: syslog-ng historically choose the term
   `level` to refer to the severity of the message that was used in the
   template language, filter expressions and so on. RFC3164 however, a few
   years later adopted the term `severity` for the same purpose. This change
   will add support for `$SEVERITY` macro and the `severity()` filter
   expression, while keeping the old ones for compatibility. (#3128)
 * file-source: new option to multi-line file sources: `multi-line-timeout()`
   
   After waiting multi-line-timeout() seconds without reading new data from the file, the last (potentially partial) message will be flushed and sent through the pipeline as a LogMessage.
   Since the multi-line file source detects the end of a message after finding the beginning of the subsequent message (indented or no-garbage/suffix mode), this option can be used to flush the last multi-line message in the file after a multi-line-timeout()-second timeout.
   
   There is no default value, i.e. this timeout needs to be explicitly configured.
   
   Example config:
   ```
   file("/some/folder/events"
       multi-line-mode("prefix-garbage")
       multi-line-prefix('^EVENT: ')
       multi-line-timeout(10)
       flags("no-parse")
   );
   ``` (#2963)

## Bugfixes

 * `configure.ac`: fixed gethostbyname function location detection (#3135)
 * http: Fixed a crash, when worker was set to 0. We do not allow nonnegative values anymore. (#3116)
 * `snmp-dest`: `engine-id()` option now handles 5 to 32 characters, instead of the strict 10 before. (#3058)
 * http: Fixed handling of ssl-version() option, which was ignored before.
   
   Prior this fix, these values of `ssl-version` in http destination were ignored by syslog-ng:
   `tlsv1_0`, `tlsv1_1`, `tlsv1_2`, `tlsv1_3`. (#3083)
 * `network` sources: Added workaround for a TLS 1.3 bug to prevent data loss
   
   Due to a bug in the OpenSSL TLS 1.3 implementation (openssl/openssl#10880),
   it is possible to lose messages when one-way communication protocols are used, -
   such as the syslog protocol over TLS ([RFC 5425](https://tools.ietf.org/html/rfc5425),
   [RFC 6587](https://tools.ietf.org/html/rfc6587)) - and the connection is closed
   by the client right after sending data.
   
   The bug is in the TLS 1.3 session ticket handling logic of OpenSSL.
   
   To prevent such data loss, we've disabled TLS 1.3 session tickets in all
   syslog-ng network sources. Tickets are used for session resumption, which is
   currently not supported by syslog-ng.
   
   The `loggen` testing tool also received some bugfixes (#3064), which reduce the
   likelihood of data loss if the target of loggen has not turned off session
   tickets.
   
   If you're sending logs to third-party OpenSSL-based TLS 1.3 collectors, we
   recommend turning session tickets off in those applications as well until the
   OpenSSL bug is fixed. (#3082)
 * `cmake`: Now we install `loggen` headers, as we do with `autotools`. (#3067)
 * `graylog2`, `format-gelf`: Fixed sending empty message, when ${PID} is not set.
   Also added a default value "-" to empty `short_message` and `host` as they are mandatory fields. (#3112)
 * loggen: fix dependency error with cmake + openssl from nonstandard location (#3062)
 * ConfigOptionDatabase: Fixed reading 'grammar' and 'parser' files on 'POSIX' environment (#3125)
 * `file source`: Fixed `file` source not able to process new message after `log-msg-size()` increase. (#3075)
 * `checkpoint parser`: Fixed parsing ISO timestamp. (#3056)
 * `secret-storage`: Fixed some cases, where diagnostical logs were truncated. (#3141)
 * `python`: message interface changed to unicode from bytes in Python 3
   
   Prior this change, the type of an object like `msg["MSG"]` was Bytes in Python. However, typically `LogMessage` carries strings, which causes inconvenience, because users need to explicitely convert to string (unicode), using `str()` or `.decode()`. This change simplifies the api by converting to unicode automatically.
   Only Python3 is affected. In Python2 Bytes and string is the same, so there was no need for explicit conversion. (#3153)
 * `loggen, dqtool`: Fixed a crash, when writing error/debug message or relocating qfile. (#3069)
 * build: Fixed a compatibility related build error on Solaris 11. (#3070)
 * `loggen`: Fixed address resolution when only loopback interface was configured. (#3048)

## Notes to developers

 * `http`: define HTTP signals
   
   This changeset defines the Signal interface for HTTP - with one signal at this time.
   
   What's in the changeset?
   
   * List ADT (abstract data type for list implementations) added to lib.
     * Interface:
       * append
       * foreach
       * is_empty
       * remove_all
   
   Implement the List ADT in http module with `struct curl_slist` for storing the headers.
   
   * HTTP signal(s):
   Currently only one signal is added, `header_request`.
   
   Note, that the license for `http-signals.h` is *LGPL* . (#3044)
 * packaging: Now we include the `packaging/rhel/` folder in our release tarball. (#3071)
 * `snmp test in Light`: snmp destination test case written in Light test framework
   
   snmp destination related functional tests can be run from Light framework.
   This test requires snmptrapd as an external dependency. If you don't want to
   run this test, you can use the pytest's marker discovery feature:
   
   ```python -m pytest ... -m 'not snmp'``` 
   
   The test is run by syslog-ng's Travis workflow. (#3126)
 * `dbld`: introduce syslog-ng-kira as a new CI image (#3125)
 * packaging: Added RHEL 8 / CentOS 8 support to `syslog-ng.spec` (#3034)
 * `NEWS.md`: From now on, for every PR that we want to include in the newsfile,
   we must create the news entry with the PR itself. See `news/README.md`. (#3066)
 * FunctionalTests: Make functional tests Python3 compatible (#3144)
 * `dbld`: add Ubuntu 19.10 and 20.04 (#3091)
 * `signal-slot-connector`: a generic event handler interface for syslog-ng modules
   
   * The concept is simple:
   
   There is a SignalSlotConnector which stores Signal - Slot connections 
   Signal : Slot = 1 : N, so multiple slots can be assigned to the same Signal.
   When a Signal is emitted, the connected Slots are executed.
   Signals are string literals and each module can define its own signals.
   
   * Interface/protocol between signals and slots
   
   A macro (SIGNAL) can be used for defining signals as string literals:
   
   ```
   SIGNAL(module_name, signal, signal_parameter_type)
   ```
   
   The parameter type is for expressing a kind of contract between signals and slots.
   
   usage:
   
   ```
      #define signal_cfg_reloaded SIGNAL(config, reloaded, GlobalConfig)
   
      the generated literal:
   
      "config::signal_reloaded(GlobalConfig *)"
   ```
    
    emit passes the argument to the slots connected to the emitted signal. (#3043)
 * `cmake`: add `add_module` function to cmake
   
   Reason of this changeset is to normalize CMakeLists.txt files for modules. (#3106)
 * `dbld`: Added option to customize `shell` command.
   
   Discussed in #2982, one can loose important information with
   the automatic rm option. (i.e.: during an accidental reboot)
   However removing the rm option by default, will increase the
   chance of working with VERY old Docker images.
   
   With this change, it is possible to override the option with
   `rules.conf`, while keeping the default behaviour.
   
   The simplest example: use existing images, start a new one if
   there is none. (use docker rm manually if you want to update)
   
   ```
   DOCKER_SHELL=$(DOCKER) inspect $* > /dev/null 2>&1; \
     if [ $$? -eq 0 ]; then \
       $(DOCKER) start -ia $*; \
     else \
       $(DOCKER) run $(DOCKER_RUN_ARGS) -ti --name $* balabit/syslog-ng-$* /source/dbld/shell; \
     fi
   ``` (#3038)
 * `example-modules`: add example http slot plugin
   
   This plugin is an example plugin that implements a slot for a HTTP signal (`signal_http_header_request`).
   
   When the plugin is `attached`, it `CONNECT` itself to the signal.
   When the signal is emitted by the http module, the slot is executed and append the `header` to the http headers.
   
   `header` is set in the config file.
   
   <details>
     <summary>Example config, click to expand!</summary>
   
   ```
   version:3.25
   
   @include "scl.conf"
   
   destination d_http {
     http(
       url("http://127.0.0.1:8888")
       method("PUT")
       user_agent("syslog-ng User Agent")
       http-test-slots(
         header("xxx:aaa") # this will be appended to the http headers
       )
       body("${ISODATE} ${MESSAGE}")
     );
   };
   
   source s_generator {
     example-msg-generator(num(1) template("test msg\n") );}
   ;
   source s_net {
     network(port(5555));
   };
   
   log {
     source(s_generator);
     destination(d_http);
   };
   ```
   </details> (#3093)

## Other changes

 * `afmongodb`: remove the support of deprecated legacy configurations (#3092)
 * `http`: `use-system-cert-store` functionality has been changed
   
   This option was added in order to support monolithic installations
   that ships even libcurl... But later, as it came out, it's not enough.
   What we really need is to do some autodetection regarding ca-file and ca-dir.
   In monolithic installations we enable this by default in the related SCLs. (#3086)
 * `packaging`: Moved `scl` files to the core package. (#2979)
 * doc: Added manual page for `persist-tool`. (#3072)
 * `python`: Added `--with-python3-home` configure option to use a hard-coded PYTHONHOME for Python-based plugins
   
   This can be useful when a Python interpreter is bundled with syslog-ng.
   Relocation is supported, for example: --with-python3-home='${exec_prefix}' (#3134)

## Credits

syslog-ng is developed as a community project, and as such it relies
on volunteers, to do the work necessarily to produce syslog-ng.

Reporting bugs, testing changes, writing code or simply providing
feedback are all important contributions, so please if you are a user
of syslog-ng, contribute.

We would like to thank the following people for their contribution:

<Fill this by the internal news file creating tool>
