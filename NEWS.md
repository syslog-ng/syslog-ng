4.8.3
=====

## Highlights

Version 4.8.3 fixes a problem in the syslog-ng release process (side 
effects of the master -> develop change on GitHub). Nothing has changed
at the code level compared to the 4.8.2 release, if you use the 
'official' tarball source release. The problem only affects you, if you
use the release archives (listed as 'source code (zip)' and 'source code
(tar.gz)' under the release and the tags page) instead of the 'official'
release tarball. For example, Debian uses these files. In that case some
of the very latest commits, including the CVE fix, were missing from the
archives.

## Credits

syslog-ng is developed as a community project, and as such it relies
on volunteers, to do the work necessarily to produce syslog-ng.

Reporting bugs, testing changes, writing code or simply providing
feedback are all important contributions, so please if you are a user
of syslog-ng, contribute.

We would like to thank the following people for their contribution:

Hofi, Tamas Pal
