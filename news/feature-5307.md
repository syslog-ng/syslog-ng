`ivykis`: We have switched to [our own fork](https://github.com/balabit/ivykis) of ivykis as the source for builds when using syslog-ng’s internal ivykis option (`--with-ivykis=internal` in autotools or `-DIVYKIS_SOURCE=internal` in CMake).

We recommend switching to this internal version, as it includes new features not available in the [original version](https://github.com/buytenh/ivykis) and likely never will be.
