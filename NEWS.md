3.20.1
======

## Features

 * Add persist-tool (#2511)
 * Collectd destination (#1433)
 * Config reload status feedback (#2367)
 * Netskope parser (#2541)
 * Websense parser (#2471)
 * Json list support (#2536)
 * Xml-parser: add list support (#2544)

## Bugfixes

 * DNS memory leak/segfault fix (#2500)
 * Elasticsearch2: fix bulk send for client-mode("http") (#2478)
 * Few leak fix (#2563)
 * Filter-re: ref/unref NVTable around regex eval (#2494)
 * Fix allowing negative version number in config (#2548)
 * Fix app-parser() per reload memory leak (#2469)
 * Fix non-reliable disk-queue truncating problem on load (#2406)
 * Fix threaded source/destination crash when reverting configuration (#2555)
 * Http: add missing free for self->body_template (#2558)
 * Java, elasticsearch2, explicit unsupport for named templates (#2534)
 * Loggen: parameter handling (#2477)
 * Python-source: fix crash when posting from python thread (#2533)
 * Read acknowledgments send by Riemann (#2523)
 * Redis, Riemann: fix ref/unref-ing templates (#2514, #2530)
 * Syslog-ng@default: use pid file location on control socket (#2489)
 * Threaded-dest: Fix integer overflow (#2512)
 * Threaded-dest: make persist_name local (#2516)
 * Xml/geoip2: make prefix optional (#2538)

## Other changes

 * Autotools, cmake: add detection for pylint, nosetests (#2537,#2564)
 * Autotools: relaxing python dependency requirement  (#2472)
 * Cfg-parser: printing error positions in case of parse failure (#2455)
 * Cmake: add missing detections (#2510)
 * Collect-cov.sh: make coverage should work with lcov in bionic (#2515)
 * Configure: fix "--with-docbook*" option usage (#2465)
 * Custom LGTM.com query for detecting uses of gmtime that are not covered by a lock (#2413)
 * Fix static uClibc-ng support (#2501)
 * Format non-literal fixes (#2567)
 * Grammar: remove the pragma less include (#2550)
 * Http-worker: refactor & fix curl_easy_getinfo error message (#2527)
 * Lib/compat: replace strcasestr() implementation (#2482)
 * Libtest: Adding stopwatch.h into libtest_HEADERS list (#2553)
 * Packaging: fix the description of mod-examples (#2522)
 * Python-debugger: fix macro completion (#2439)
 * Python, java destination add already introduced retry (#2559)
 * Remove elastic v1 support (#2554)
 * Simplify libtest queue utils (#2556)
 * Syslog-ng.8.xml: remove unneeded default-modules section (#2475)
 * Travis: use the latest Bison version (macOS) (#2529)
 * Various fixes for issues reported by LGTM (#2524)

## Notes to the developers

 * Example-msg-generator: num option (#2565)
 * Own grammar support in generator plugin (#2552)
 * ProtoClient: provide process_in function to logwriter (#2468)
 * Pytest_framework: eliminating __registered_instances, exposing SyslogNgCtl to user api (#2503)
 * Pytest_framework: MessageReader: Explain the local context around python asserts (#2507)
 * Pytest_framework: support for implicit groups in config.create_logpath (#2490)
 * Pytest: Renaming pytest_framework to python_functional (#2542)
 * Python-destination: send can return worker_insert_result_t, flush support (#2487)
 * Python: internal() source exposed via syslogng.Logger (#2505)
 * Remove unused submodules (#2525)
 * Simpler names for WORKER_INSERT_RESULT_T in language bindings (#2506)
 * Split xml-parser into xml-parser and xml-scanner (#2459)


## Credits

  syslog-ng is developed as a community project, and as such it relies
  on volunteers, to do the work necessarily to produce syslog-ng.

  Reporting bugs, testing changes, writing code or simply providing
  feedback are all important contributions, so please if you are a user
  of syslog-ng, contribute.

  We would like to thank the following people for their contribution:
  Andras Mitzki, Andrej Valek, Antal Nemes, Attila Szakacs, Balazs Scheidler,
  Bas van Schaik, Fᴀʙɪᴇɴ Wᴇʀɴʟɪ, Gabor Nagy, Laszlo Boszormenyi, Laszlo Budai,
  Lorand Muzamel, László Várady, Mehul Prajapati, Naveen Revanna, Peter Czanik,
  Peter Kokai, Romain Tartière, Stephen, Terez Nemes, Norbert Takács,
  Soubhik Chakraborty, NottyRu, Chris Packham.

