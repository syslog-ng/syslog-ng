#!/bin/bash
cd ..
python3 -m cProfile -o reports/profile_on_selftest/profile_`date +"%Y-%m-%dT%H:%M"`.profile run_tests.py -s
