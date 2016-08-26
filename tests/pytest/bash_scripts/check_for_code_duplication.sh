#!/bin/bash
cd ..
clonedigger src/
mv output.html reports/code_duplication/
now=$(date +"%Y-%m-%dT%H:%M")
mv reports/code_duplication/output.html reports/code_duplication/output_${now}.html
