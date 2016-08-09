#!/bin/bash
cd ..
pycallgraph -v --include '*src*' --include '*tests*' graphviz --output-file=reports/callgraph_on_selftest/callgraph_`date +"%Y-%m-%dT%H:%M"`.png -- run_tests.py -s
