---
name: Bug report
about: Use this template for reporting a bug.
title: ''
labels: bug
assignees: ''

---
# syslog-ng
## Version of syslog-ng
<!-- $ syslog-ng --version -->

## Platform
<!-- Name and version of OS -->

## Debug bundle
<!--
Create a debug bundle on your system with the syslog-ng-debun script which is included in the syslog-ng package.

Overview of the CLI options of syslog-ng-debun:
-r: run actual information gathering
-d: run syslog-ng in debug mode
-p: perform packet capture
-s: do strace
-t: timeout period for running debug/pcap/strace
-w: wait period before starting debug mode
-l: light information gathering (respects privacy)
-R: alternate installation directory for syslog-ng

$ syslog-ng-debun -r
-->

# Issue
## Failure
<!--
Backtrace, error messages or detailed description of failure comes here.

$ gdb syslog-ng
 run
-->

## Steps to reproduce
<!-- A description of how to trigger this bug. -->

## Configuration
<!-- $ cat /path/to/syslog-ng.conf -->

## Input and output logs (if possible)
