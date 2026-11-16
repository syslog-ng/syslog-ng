`cryptofuncs`, `sql()`: Fixed `$(md5)` and `$(md4)` template functions, and the Oracle index name
generation of `sql()`, on OpenSSL 3 systems running in FIPS mode. They now fetch these digests
explicitly as non-security hashes, and no longer use an uninitialized buffer when the digest is not
available. Template functions return an empty value and log an error if the digest computation fails;
unavailable digest types are reported when the configuration is loaded.
