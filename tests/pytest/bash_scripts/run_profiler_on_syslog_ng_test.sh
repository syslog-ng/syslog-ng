#!/bin/bash
cd ..
mkdir -p reports/profile_on_syslog_ng_test
python3 -m cProfile -o reports/profile_on_syslog_ng_test/profile_`date +"%Y-%m-%dT%H:%M"`.profile run_tests.py -t
