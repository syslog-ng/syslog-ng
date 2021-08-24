`disk-buffer()`: performance improvements

Based on our measurements, the following can be expected compared to the previous syslog-ng release (v3.33.1):
- non-reliable disk buffer: up to 50% performance gain;
- reliable disk buffer: up to 80% increase in performance.

([#3743](https://github.com/syslog-ng/syslog-ng/pull/3743), [#3746](https://github.com/syslog-ng/syslog-ng/pull/3746),
[#3754](https://github.com/syslog-ng/syslog-ng/pull/3754), [#3756](https://github.com/syslog-ng/syslog-ng/pull/3756),
[#3757](https://github.com/syslog-ng/syslog-ng/pull/3757))
