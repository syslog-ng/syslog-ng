#! /bin/sh

root=$(cd $(dirname $0) && cd .. && pwd)

populate() {
        mfam="$1"
        dir="$(dirname ${mfam})"
        if [ "${dir}" != "." ] && [ "${dir}" != "${root}" ] && [ -d "${dir}" ]; then
                echo "${root}/Mk/subdir.mk" "=>" "${dir}/Makefile"
                ln -sf "${root}/Mk/subdir.mk" "${dir}/Makefile"
        fi
        if [ -e "${root}/${dir}/Makefile.am" ]; then
                incs=$(grep "^include" "${root}/${dir}/Makefile.am" | sed -e "s,^include ,,")
                for inc in ${incs}; do
                        populate "${inc}"
                done
        fi
}

populate ./Makefile.am
