#!/bin/bash -x

syslog_ng_ose_version="ose_3.6"
cd ..
syslog_ng_ose_testdb_path=`pwd`
syslog_ng_ose_source_path="$HOME/syslog_ng_${syslog_ng_ose_version}"
date_now=`date +"%Y-%m-%dT%H:%M"`

actual_report_dir="${syslog_ng_ose_testdb_path}/reports/coverage_on_slng_while_running_testdb/${date_now}"
mkdir -p ${actual_report_dir}

function compile_syslog_ng_with_enable_gcov() {
    cd
    rm -rf ${syslog_ng_ose_source_path}
    git clone -b 3.6/master https://github.com/balabit/syslog-ng.git ${syslog_ng_ose_source_path}
    cd ${syslog_ng_ose_source_path}
    ./autogen.sh
    mkdir -p `pwd`/install_dir
    ./configure --enable-gcov --with-ivykis=internal --prefix=`pwd`/install_dir --enable-pacct
    make
    sudo make install
    sudo chown -R `whoami`:`whoami` `pwd`/install_dir
}
function lcov_before_test() {
    lcov --no-external --capture --initial --directory `pwd` --output-file ${actual_report_dir}/syslog_ng_base.info
}
function run_testdb() {
    cd ${syslog_ng_ose_testdb_path}
    py.test syslog_ng_tests/option_related_tests/global_option/test_bad_hostname.py -k"default"
}
function lcov_after_test() {
    cd ${syslog_ng_ose_source_path}
    lcov --no-external --capture --directory `pwd` --output-file ${actual_report_dir}/syslog_ng_test.info
}
function lcov_merge_trace_files() {
    lcov --add-tracefile ${actual_report_dir}/syslog_ng_base.info --add-tracefile ${actual_report_dir}/syslog_ng_test.info --output-file ${actual_report_dir}/syslog_ng_total.info
}
function lcov_filter() {
    lcov --remove ${actual_report_dir}/syslog_ng_total.info '/usr/include/*' -o ${actual_report_dir}/syslog_ng_filtered.info
}
function generate_html() {
    genhtml --prefix `pwd` --ignore-errors source ${actual_report_dir}/syslog_ng_filtered.info --legend --title "commit SHA1" --output-directory=${actual_report_dir}/syslog_ng_lcov
}

function run_coverage_on_slng_code_base_while_running_testdb() {
    compile_syslog_ng_with_enable_gcov
    lcov_before_test
    run_testdb
    lcov_after_test
    lcov_merge_trace_files
    lcov_filter
    generate_html
}

run_coverage_on_slng_code_base_while_running_testdb
