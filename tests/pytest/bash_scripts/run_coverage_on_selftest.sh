#!/bin/bash -x

#sudo pip install pytest-cov
#sudo apt-get install python-coverage python3-coverage
mkdir -p ../reports/coverage_on_selftest
cd ../reports/coverage_on_selftest
py.test-3 --cov ../../src/ ../../src/
python3 -m coverage html -d `date +"%Y-%m-%dT%H:%M"`
