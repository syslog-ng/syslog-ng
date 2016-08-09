#!/bin/bash
cd ..

functions=`ack-grep -h "def " src2/ | grep -v "test_" | grep -v "init" | sed 's/    //g' | cut -d" " -f2 | cut -d"(" -f1 | sort | grep -v "^def$"`

for i_func in ${functions[@]}; do
    number_of_usage=`grep -RnHi ${i_func} src2/ | wc -l`
    if [ ${number_of_usage} -eq 1 ] ; then
        echo ">>> This function: ${i_func} never used"
        grep -RnHi ${i_func} src/
    fi
done
