`rpm`: fix --without maxminddb option

If maxminddb development package was installed on the build system: rpmbuild fails if `--without maxminddb` was used.
