3.14.1

<!-- Fri, 23 Feb 2018 09:08:05 +0100 -->

## Features

 * Password protected ssl keys (#1888)
 * Add OpenBSD module to system() source (#1875)
 * Add Ubuntu Trusty support to Docker build (#1849)
 * Add filters as selectors in contextual data (#1838)

## Bugfixes

 * Fix increased memory usage during saving disk-buffer (#1867)
 * Fix maximum record length limitations of disk-buffer (#1874)
 * Fix a memory leak in cfg-lexer (#1843)
 * Fix some issues found by pylint in python module (#1881, #1830)
 * Fix a crash due to a race condition in kv-parser() (#1871)
 * Fix a crash due to a race condition in file() destination (#1858)
 * Fix deprecated API usage in python module tests (#1829)
 * Fix a race condition in internal() source (#1815)
 * Fix a locale issue in merge-grammar python tool (#1868)
 * Fix compile problems with autotools when '--disable-all-modules' used (#1853)
 * Fix a file descriptor leak in persist-state (#1847)
 * Fix a file descriptor leak in pseudofile() (#1846)
 * Fix memory/fd leaks in loggen tool (#1844, #1845)
 * Fix compile problems on Fedora, RHEL6, CentOS6 and SUSE based platforms (#1837)
 * Fix a crash when large variety of keys added to messages (#1836)
 * Fix compile problems when PATH_MAX not defined (#1828)
 * Fix integer overflow problems in grammar (#1823)
 * Fix a memory leak in filter() (#1812)
 * Fix memory leak of persist-name() option (#1816)
 * Fix message corruption caused by a bug in the subst() rewrite rule (#1801)
 * Fix silently dropped messages in elasticsearch2() when sending in bulk mode (#1800)
 * Fix broken disk-buffer() support in elasticsearch2() (#1807)
 * Fix Hy support in python module (#1754)
 * Fix an event scheduler related crash during reloading syslog-ng (#1711)
 * Fix a crash with SIGBUS when persist file cannot grow (#1785)

## Other changes

 * Improve error reporting in "block" definitions in config (#1809)
 * Add warning message when disk-buffer() directory is changed in configuration (#1861)
 * Syslog-ng debun improvements (#1840)
 * Refactor in rewrite() module init (#1818)
 * Missing child program (exit status 127) handling is changed in program() destination:
    stopping destination instead of polling for the child program (#1817)
 * Refactor in filter() module (#1814)
 * Improve thread synchronization in mainloop and refactor (#1813)
 * Adapted json-c v0.13 API changes to json-parser (#1810)

## Notes to the developers

 * Full cmake support achieved (#1777, #1819, #1811, #1808, #1805, #1802, #1841, #1806)
 * Add support for modules to have module specific global options (#1885)
 * Improved MacOS support (#1862, #1864, #1865)
 * Add new option to exclude directories in style-checker tool (#1834)
 * Ivykis dependency updated to 0.42.2 release (#1711)
 * Journald grammar, source and header files are part of dist tarball (#1852)
 * Add valgrind support for unit tests (#1839)

## Credits

syslog-ng is developed as a community project, and as such it relies
on volunteers, to do the work necessarily to produce syslog-ng.

Reporting bugs, testing changes, writing code or simply providing
feedback are all important contributions, so please if you are a user
of syslog-ng, contribute.

We would like to thank the following people for their contribution:
Andras Mitzki, Antal Nemes, Balazs Scheidler, Björn Esser, Fabien Wernli, Gabor Nagy, Gergely Nagy,
Janos Szigetvari, Juhász Viktor, Laszlo Budai, Laszlo Szemere, László Várady, Orion Poplawski,
Attila Szalay, Shen-Ta Hsieh, Tamas Nagy, Peter Kokai, Norbert Takacs, Zoltan Pallagi.
