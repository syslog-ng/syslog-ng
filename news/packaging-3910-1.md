`rhel`: Removed `bison` and `flex` from the specfile.

The tarball has the grammar files built already, so these are not necessary.
Also, the distros have too old `bison` anyways to build the grammar files, if it would be necessary.
