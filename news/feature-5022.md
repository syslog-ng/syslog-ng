`directory-monitor`: Added a kqueue based directory monitor implementation.

`wildcard-file()` sources are using a [directory monitor](https://syslog-ng.github.io/admin-guide/060_Sources/020_File/001_File_following) as well to aid detection of changes in the folders of the followed files. The new kqueue-based directory monitor uses far fewer resources than the `poll` based version on BSD-based systems.
