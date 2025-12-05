#!/bin/sh
############################################################################
# Copyright (c) 2025 Airbus Commercial Aircraft
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License version 2 as published
# by the Free Software Foundation, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
#
# As an additional exemption you are allowed to compile & link against the
# OpenSSL libraries as published by the OpenSSL project. See the file
# COPYING for details.
#
#############################################################################

# File:   cli02_crypt_verify.sh
# Author: Airbus Commercial Aircraft <secure-logging@airbus.com>
# Date:   2025-12-05
#
# Smoke Test of cli tools slogkey, slogencrypt and slogverify
# Needed keys are generated in test.

VERSION="Version 1.2.3"

# remove path and extension from $0
s=$0
SCRIPTNAME="$(
    b="${s##*/}"
    echo "${b%.*}"
)"
NOW=$(date +%Y-%m-%d_%H%M_%S)
echo " "
echo " "
echo "***********************************************************"
echo "*** ${SCRIPTNAME}, ${VERSION}, ${NOW}"
echo "***********************************************************"
echo " "

SCRIPT_DIR="$(cd -- "$(dirname -- "$0")" && pwd -P)"
echo "SCRIPT_DIR: ${SCRIPT_DIR}"

PATH_PREFIX_VALUE=$("${SCRIPT_DIR}"/get_prefix.sh)

# This path must fit the one given to configure script with option --prefix
# In case the config.h file has not been found, PREFIX must be set manually.
PREFIX=${PATH_PREFIX_VALUE}
echo "PREFIX: ${PREFIX}"
HOMESLOGTEST=${SCRIPT_DIR}
SFNCONF=syslog-ng-test-udp-nc.conf
# e.g.: /home/johndoe/Software/fork/modules/secure-logging/tests/syslog-ng-test-udp-nc.conf
MAX_LOOP=5
STOP_WAIT_TIME=2
BIN=${PREFIX}/bin
SBIN=${PREFIX}/sbin
ETC=${PREFIX}/etc
VAR=${PREFIX}/var
SUBFOLDER=slog
SUBFOLDER_TEST="test"
TEST=/tmp/${SUBFOLDER_TEST}/${SUBFOLDER}
HOME_BACKUP=${HOME}/${SUBFOLDER_TEST}/${SCRIPTNAME}
COPY_TO_HOME_BACKUP=false
MACADDRESS="01:23:45:67:89:AB"
SERIALNUMBER="12345678"

# error counter, success when this script returns 0
cnt_error=0

#-----------------------------------------------------------------------
# list current configuration and exit with error
check_script_config() {
    echo "ERROR! Precondition to start failed. Check configuration of $0"
    echo " "

    # Prefix is provided by a script. When this is not working
    # User can try to set it manually.
    echo "PREFIX: ${PREFIX}"

    echo "BIN: ${BIN}"
    echo "SBIN: ${SBIN}"
    echo "ETC: ${ETC}"
    echo "VAR: ${VAR}"
    echo "TEST: ${TEST}"

    echo "COPY_TO_HOME_BACKUP: ${COPY_TO_HOME_BACKUP}"
    if [ "${COPY_TO_HOME_BACKUP}" = "true" ]; then
        echo "HOME_BACKUP: ${HOME_BACKUP}"
    fi

    # stop working. exit now.
    exit 1
}

#-----------------------------------------------------------------------
# -- helper function to check of all files in given array do exist
# Usage: all_files_exist "path1" "path2" "path3" ...
check_missing() {
    for path in "$@"; do
        # Check if the path does NOT exist (-e works for files and directories)
        if [ ! -e "${path}" ]; then
            echo "Error: Required path '${path}' not found."
            check_script_config
            return 1 # ERROR, safe, check_script_config already exit 1 when file not found
        fi
    done
    return 0 # SUCESS, all files found
}

#-----------------------------------------------------------------------
# Helper function to stop running syslog-ng background process
stop_syslog() {
    echo " "
    echo "----------------------------------------"
    echo "--- STOP syslog-ng"
    echo "----------------------------------------"
    echo " "

    # stop running instance of syslog-ng
    if pgrep -x "syslog-ng" >/dev/null; then
        echo "    syslog-ng is running and will be stopped now"
        # stop old instance
        "${SBIN}/syslog-ng-ctl" stop
        sleep "${STOP_WAIT_TIME}"
    fi

    if pgrep -x "syslog-ng" >/dev/null; then
        echo "    ERROR! syslog-ng still running!"
        cnt_error=$((cnt_error + 1))
    else
        echo "    sylog-ng is not active"
    fi
}

echo " "
echo "#-1"
"${SCRIPT_DIR}"/get_git_info.sh

mkdir -p "${TEST}"
if [ "${COPY_TO_HOME_BACKUP}" = "true" ]; then
    mkdir -p "${HOME_BACKUP}"
    check_missing "${HOME_BACKUP}"
fi

# -- do initial checks -----
check_missing "${TEST}" "${PREFIX}" "${BIN}" "${SBIN}"

# -- Ensure syslog-ng engined is not running -----
stop_syslog

check_missing "${VAR}" "${ETC}" "${HOMESLOGTEST}" "${SBIN}/syslog-ng" "${SBIN}/syslog-ng-ctl" \
    "${BIN}/slogencrypt" "${BIN}/slogverify" "${BIN}/slogkey" "${BIN}/loggen"

# cleanup files from previous tests
rm -f "${TEST}"/*.key "${TEST}"/*.dat "${TEST}"/*.txt "${TEST}"/*.chk 2>/dev/null
rm -f "${TEST}"/*.out "${TEST}"/*.log "${TEST}"/*.slo* 2>/dev/null

if [ "${COPY_TO_HOME_BACKUP}" = "true" ]; then
    rm -f "${HOME_BACKUP}"/*.key "${HOME_BACKUP}"/*.dat "${HOME_BACKUP}"/*.txt "${HOME_BACKUP}"/*.chk 2>/dev/null
    rm -f "${HOME_BACKUP}"/*.out "${HOME_BACKUP}"/*.log "${HOME_BACKUP}"/*.slo* 2>/dev/null
fi

if ! [ -f "${TEST}/host.key" ]; then
    if ! [ -f "${TEST}/master.key" ]; then
        # create master.key needed to create host.key
        echo " "
        echo "#-2 Create master key"
        echo "${BIN}/slogkey" \
            "-m ${TEST}/master.key"
        "${BIN}/slogkey" \
            -m "${TEST}/master.key"
        if ! [ -f "${TEST}/master.key" ]; then
            echo "ERROR! Not found: ${TEST}/master.key"
            check_script_config
        fi
        echo "hexdump -C ${TEST}/master.key"
        hexdump -C "${TEST}/master.key"
    else
        echo " "
        echo "INFO: ${TEST}/master.key was already available!"
        echo " "
    fi
    # create derived host.key h0.key
    echo " "
    echo "#-3 Create host key h0.key"
    echo "${BIN}/slogkey" \
        "-d ${TEST}/master.key" \
        "${MACADDRESS}" \
        "${SERIALNUMBER}" \
        "${TEST}/h0.key"

    "${BIN}/slogkey" \
        -d "${TEST}/master.key" \
        "${MACADDRESS}" \
        "${SERIALNUMBER}" \
        "${TEST}/h0.key"
    cp "${TEST}"/h0.key "${TEST}"/host.key
else
    echo " "
    echo "INFO: ${TEST}/host.key was already available! Usage count must be taken into account!"
    echo " "
fi

check_missing "${TEST}/h0.key" "${TEST}/host.key" "${HOMESLOGTEST}/${SFNCONF}"

cp -f "${HOMESLOGTEST}/${SFNCONF}" "${TEST}/syslog-ng.conf"

check_missing "${TEST}/syslog-ng.conf"

# -- stop syslog-ng engine -----
stop_syslog

# -- create log entries by slogencrypt -----
echo "----------------------------------------"
echo "--- create text files used later as log entry"
echo "----------------------------------------"
echo " "
i=0
while [ "${i}" -lt "${MAX_LOOP}" ]; do
    i=$((i + 1))

    if [ "${i}" -eq 1 ]; then
        # 2047 + 1 ('\n')
        LOG_MESSAGE="BY DOUGLAS ADAMS: Published by The Random House Publishing Group, The Hitchhiker’s Guide to the Galaxy, The Restaurant at the End of the Universe, Life, the Universe and Everything, So Long, and Thanks for All the Fish, Mostly Harmless, The Ultimate Hitchhiker’s Guide to the Galaxy, The Salmon of Doubt, Dirk Gently’s Holistic Detective Agency, The Long Tea-Time of the Soul, The Original Hitchhiker Radio Scripts, The Meaning of Liff (with John Lloyd), Last Chance to See (with Mark Cowardine), The Deeper Meaning of Liff (with John Lloyd), for Jonny Brock and Clare Gorst and all other Arlingtonians, for tea, sympathy and a sofa Far out in the uncharted backwaters of the unfashionable end of the Western Spiral arm of the Galaxy lies a small unregarded yellow sun. Orbiting this at a distance of roughly ninety-eight million miles is an utterly insignificant little blue-green planet whose ape-descended life forms are so amazingly primitive that they still think digital watches are a pretty neat idea. This planet has—or rather had—a problem, which was this: most of the people living on it were unhappy for pretty much of the time. Many solutions were suggested for this problem, but most of these were largely concerned with the movements of small green pieces of paper, which is odd because on the whole it wasn’t the small green pieces of paper that were unhappy.And so the problem remained; lots of the people were mean, and most of them were miserable, even the ones with digital watches. Many were increasingly of the opinion that they’d all made a big mistake in coming down from the trees in the first place. And some said that even the trees had been a bad move, and that no one should ever have left the oceans. And then, one Thursday, nearly two thousand years after one man had been nailed to a tree for saying how great it would be to be nice to people for a change, a girl sitting on her own in a small café in Rickmansworth suddenly realized what it was that had been going wrong all this time, and she finally knew how the "

    elif [ "${i}" -eq 2 ]; then

        LOG_MESSAGE="BY DOUGLAS ADAMS: Published by The Random House Publishing Group, The Hitchhiker’s Guide to the Galaxy, The Restaurant at the End of the Universe, Life, the Universe and Everything, So Long, and Thanks for All the Fish, Mostly Harmless, The Ultimate Hitchhiker’s Guide to the Galaxy, The Salmon of Doubt, Dirk Gently’s Holistic Detective Agency, The Long Tea-Time of the Soul, The Original Hitchhiker Radio Scripts, The Meaning of Liff (with John Lloyd), Last Chance to See (with Mark Cowardine), The Deeper Meaning of Liff (with John Lloyd), for Jonny Brock and Clare Gorst and all other Arlingtonians, for tea, sympathy and a sofa Far out in the uncharted backwaters of the unfashionable end of the Western Spiral arm of the Galaxy lies a small unregarded yellow sun. Orbiting this at a distance of roughly ninety-eight million miles is an utterly insignificant little blue-green planet whose ape-descended life forms are so amazingly primitive that they still think digital watches are a pretty neat idea. This planet has—or rather had—a problem, which was this: most of the people living on it were unhappy for pretty much of the time. Many solutions were suggested for this problem, but most of these were largely concerned with the movements of small green pieces of paper, which is odd because on the whole it wasn’t the small green pieces of paper that were unhappy.And so the problem remained; lots of the people were mean, and most of them were miserable, even the ones with digital watches. Many were increasingly of the opinion that they’d all made a big mistake in coming down from the trees in the first place. And some said that even the trees had been a bad move, and that no one should ever have left the oceans. And then, one Thursday, nearly two thousand years after one man had been nailed to a tree for saying how great it would be to be nice to people for a change, a girl sitting on her own in a small café in Rickmansworth suddenly realized what it was that had been going wrong all this time, and she finally knew how the 🌍0123456789abcdefghijklmnopqrstuvwxyz This will be shortened"

    elif
        [ "${i}" -eq 3 ]
    then

        LOG_MESSAGE="Sir Reginald Fluffington III, a hamster of distinguished lineageand questionable judgment, was not your average rodent. He was a time traveler. His machine, a modified hamster wheel powered by existential dread and sunflower seeds, was notoriously imprecise. His first jump was meant to be a simple trip to last Tuesday to retrieve a misplaced almond. Instead, he landed in ancient Rome, right in the middle of Julius Caesar's salad. Confused but undeterred, Sir Reginald, mistaking a crouton for his lost almond, scurried away with it. This single act caused a cascade of chaos. Caesar, distracted by the sudden appearance and disappearance of a furry blur in his lunch, missed the crucial part of a senator's warning about Brutus. The course of history, as they say, was mildly inconvenienced. Sir Reginald's next trip took him to the deck of the Titanic. His mission: to warn the captain. He squeaked frantically in Hamster-ese, pointing a tiny paw at a crude drawing of an iceberg he'd made on a piece of lint. The captain, a man named Smith, simply chuckled, picked him up, and declared him the ship's new mascot, 'Unsinkable Sam'. The irony was lost on everyone, especially Sir Reginald, who spent the fateful night trying to gnaw through the ship's rudder cables. He failed, but his efforts were noted in a forgotten footnote of a survivor's diary. His most ambitious journey was to the Cretaceous period. He arrived, squeaked a mighty 'Eureka!', and was promptly eaten by a Compsognathus. Or so it would seem. Inside the dinosaur's stomach, Sir Reginald discovered something extraordinary: a perfectly preserved, prehistoric sunflower seed of immense size. Using his last ounce of existential dread, he powered up the wheel one final time. He reappeared in his cage, a single, gigantic seed in his paws, just seconds after he'd left. He had circumnavigated time, space, and the digestive tract of a dinosaur, all to find the ultimate snack. He never time-traveled again. The risks were too high, and besides, this new seed would last🚀🌍🔭"

    else
        LOG_MESSAGE="[INFO] This is log number ${i}, Hello, World! 🌍, Voilà! Le garçon déçu a mangé un gâteau."
        LOG_MESSAGE="${LOG_MESSAGE} Fröhliche Grüße aus Düsseldorf, Straße № 1ß. ¿Qué pasa, señor?"
        LOG_MESSAGE="${LOG_MESSAGE} The price is > 100€, < 20£, or ~ \$15."
        LOG_MESSAGE="${LOG_MESSAGE} (Special chars: !@#$%^&*\`§©®™) 🤪"
    fi

    echo "${LOG_MESSAGE}" >"${TEST}/plainlog_${i}.txt"

    sleep 0.25
done

# Example from man page:
#  slogverify --iterative --prev-key-file host.key.2 --prev-mac-file mac.dat.2 --mac-file mac.dat
#       /var/log/messages.slog.3 /var/log/verified/messages.3

# Note:
# Old version of slogverify does NOT provide an initial MAC file.
# DO NOT USE `touch "${ETC}/mac0.dat"` to create an initial mac dat.
#
# slogencrypt has been fixed now, so that it is working without an existing mac file.
# When testing old versions of slogencrypt, mac1.dat is copied as mac0.dat.
# If this happens, slogverify is not working correctly for the first chunk using mac0.dat.

i=0
while [ "${i}" -lt "${MAX_LOOP}" ]; do
    i=$((i + 1))
    j=$((i - 1))
    echo " "
    OUT="-- Loop i: ${i}, j: ${j} ---"
    echo "${OUT}"
    echo "---- Encrypting file"

    echo "${BIN}/slogencrypt" \
        "-k ${TEST}/h${j}.key" \
        "-m ${TEST}/mac${j}.dat" \
        "${TEST}/h${i}.key" \
        "${TEST}/mac${i}.dat" \
        "${TEST}/plainlog_${i}.txt" \
        "${TEST}/plainlog_${i}.out"

    echo " "
    "${BIN}/slogencrypt" \
        -k "${TEST}/h${j}.key" \
        -m "${TEST}/mac${j}.dat" \
        "${TEST}/h${i}.key" \
        "${TEST}/mac${i}.dat" \
        "${TEST}/plainlog_${i}.txt" \
        "${TEST}/plainlog_${i}.out"

    echo " "
    find "${TEST}/" -iname '*.dat'
    find "${TEST}/" -iname '*.key'
    find "${TEST}/" -iname '*.txt'
    find "${TEST}/" -iname '*.out'

    if ! [ -f "${TEST}/mac${i}.dat" ]; then
        echo "WARNING ${TEST}/mac${i}.dat does not exist. Wait 3 seconds.."
        sleep 3
    fi
    if ! [ -f "${TEST}/mac${i}.dat" ]; then
        echo "ERROR! ${TEST}/mac${i}.dat does still not exist."
        exit 1
    fi
    if ! [ -f "${TEST}/h${i}.key" ]; then
        echo "ERROR! ${TEST}/h${i}.key does not exist."
        exit 1
    fi
    if ! [ -f "${TEST}/plainlog_${i}.out" ]; then
        echo "ERROR! ${TEST}/plainlog_${i}.out does not exist."
        exit 1
    fi
    if ! [ -f "${TEST}/mac0.dat" ]; then
        echo "!!!"
        echo "!!! WARNING: mac0.dat was not existent !!!"
        echo "!!! This is not expected to happen when latest syslog-ng is used."
        echo "!!! A copy of mac1.dat will be used later instead but will cause a"
        echo "!!! verification error later"
        echo "!!!"
        cnt_error=$((cnt_error + 1))
        cp "${TEST}/mac${i}.dat" "${TEST}/mac0.dat"
    fi

    echo " "
    echo "${TEST}/plainlog_${i}.txt"
    cat "${TEST}/plainlog_${i}.txt"

    echo " "
    echo "${TEST}/plainlog_${i}.out"
    cat "${TEST}/plainlog_${i}.out"

    echo " "
    echo "---- Iterative verification"
    echo " "
    echo " "
    echo "${BIN}/slogverify -i" \
        "-p ${TEST}/h${j}.key" \
        "-r ${TEST}/mac${j}.dat" \
        "-m ${TEST}/mac${i}.dat" \
        "${TEST}/plainlog_${i}.out" \
        "${TEST}/plainlog_${i}.chk"

    echo " "
    "${BIN}/slogverify" -i \
        -p "${TEST}/h${j}.key" \
        -r "${TEST}/mac${j}.dat" \
        -m "${TEST}/mac${i}.dat" \
        "${TEST}/plainlog_${i}.out" \
        "${TEST}/plainlog_${i}.chk" 2>&1 | tee "${TEST}/slogverify-iterative-mode-result${i}.log"

    echo " "
    # check if output files do exist
    if ! [ -f "${TEST}/plainlog_${i}.out" ]; then
        echo "ERROR: ${TEST}/plainlog_${i}.out does not exist"
        cnt_error=$((cnt_error + 1))
    else
        echo "${TEST}/plainlog_${i}.out"
        cat "${TEST}/plainlog_${i}.out"
    fi
    echo " "
    if ! [ -f "${TEST}/plainlog_${i}.chk" ]; then
        echo "ERROR: ${TEST}/plainlog_${i}.chk does not exist"
        cnt_error=$((cnt_error + 1))
    else
        echo "${TEST}/plainlog_${i}.chk"
        cat "${TEST}/plainlog_${i}.chk"
    fi
    echo " "

    # [SLOG] ERROR: Log claims to be past entry from past archive. We cannot rewind back to this key without key0. This is going to fail.; entry='2'
    # [SLOG] WARNING: Decryption not successful; entry='2'
    # [SLOG] WARNING: Unable to recover; entry='3'
    # [SLOG] WARNING: Aggregated MAC mismatch. Log might be incomplete;
    # [SLOG] WARNING: Aggregated MAC mismatch. Log might be incomplete;
    # [SLOG] ERROR: There is a problem with log verification. Please check log manually;
    if ! [ -f "${TEST}/slogverify-iterative-mode-result${i}.log" ]; then
        printf "ERROR: %s does not exist\n" "${TEST}/slogverify-iterative-mode-result${i}.log" >&2
        cnt_error=$((cnt_error + 1))
    else
        if grep -q "ERROR: Log claims to be past entry from past archive" "${TEST}/slogverify-iterative-mode-result${i}.log"; then
            cnt_error=$((cnt_error + 1))
        fi
        if grep -q "Decryption not successful" "${TEST}/slogverify-iterative-mode-result${i}.log"; then
            cnt_error=$((cnt_error + 1))
        fi
        if grep -q "Unable to recover" "${TEST}/slogverify-iterative-mode-result${i}.log"; then
            cnt_error=$((cnt_error + 1))
        fi
        if grep -q "Aggregated MAC mismatch" "${TEST}/slogverify-iterative-mode-result${i}.log"; then
            cnt_error=$((cnt_error + 1))
        fi
        if grep -q "ERROR: There is a problem with log verification." "${TEST}/slogverify-iterative-mode-result${i}.log"; then
            cnt_error=$((cnt_error + 1))
        fi
    fi

done

echo " "
echo " "
echo "#-7"
echo "----------------------------------------"
echo "--- Check generated keys, log, mac"
echo "----------------------------------------"
echo " "

# -- key counter -----

echo " "
echo "#-8"
i=0
ALL=$((MAX_LOOP + 1))
while [ "${i}" -lt "${ALL}" ]; do
    echo " "
    echo "${BIN}/slogkey --counter ${TEST}/h${i}.key"
    "${BIN}"/slogkey --counter "${TEST}/h${i}.key"
    i=$((i + 1))
done

# -- sha256sum -----

echo " "
echo "#-9"
echo " "
i=0
ALL=$((MAX_LOOP + 1))
while [ "${i}" -lt "${ALL}" ]; do
    sha256sum "${TEST}/h${i}.key"
    i=$((i + 1))
done

echo " "
i=0
ALL=$((MAX_LOOP + 1))
while [ "${i}" -lt "${ALL}" ]; do
    sha256sum "${TEST}/mac${i}.dat"
    i=$((i + 1))
done

# check whether mac.dat has been accessed - it should not because syslog-ng not
# running!
# If it has been provided by slogencrypt, content should be the same as
# mac0.dat.
echo " "
if [ -f "${TEST}/mac.dat" ]; then
    sha256sum "${TEST}/mac.dat"
fi

# -- hexdump -----

echo " "
echo "#-10"
echo " "
i=0
ALL=$((MAX_LOOP + 1))
while [ "${i}" -lt "${ALL}" ]; do
    echo "hexdump -C ${TEST}/h${i}.key"
    hexdump -C "${TEST}/h${i}.key"
    i=$((i + 1))
done

echo " "
i=0
ALL=$((MAX_LOOP + 1))
while [ "${i}" -lt "${ALL}" ]; do
    echo "hexdump -C ${TEST}/mac${i}.dat"
    hexdump -C "${TEST}/mac${i}.dat"
    i=$((i + 1))
done

if [ "${COPY_TO_HOME_BACKUP}" = "true" ]; then
    # copy from /tmp/test/slog/ to home folder ~/test/<scriptname>/
    cp -R "${TEST}/." "${HOME_BACKUP}/"

    # list content
    echo "#-11"
    echo "ls -al ${HOME_BACKUP}/"
    ls -al "${HOME_BACKUP}/"
fi

echo " "
echo "#-12"
i=0
ALL=$((MAX_LOOP + 1))
while [ "${i}" -lt "${ALL}" ]; do
    echo "Check if files mac${i}.dat and h${i}.key do exist .."
    if ! [ -f "${TEST}/mac${i}.dat" ]; then
        echo "ERROR: ${TEST}/mac${i}.dat does not exist"
        cnt_error=$((cnt_error + 1))
    fi
    if ! [ -f "${TEST}/h${i}.key" ]; then
        echo "ERROR: ${TEST}/h${i}.key does not exist"
        cnt_error=$((cnt_error + 1))
    fi
    i=$((i + 1))
done
echo " "

# cleanup /tmp/${SUBFOLDER_TEST}/
rm -rf /tmp/"${SUBFOLDER_TEST}"/

echo " "
echo " "
echo "#-13"
echo "----------------------------------------"
echo "--- Used binaries"
echo "----------------------------------------"
echo " "
echo "ls -al ${BIN}/"
ls -al "${BIN}/"
echo " "
echo "ls -al ${SBIN}/"
ls -al "${SBIN}/"
echo " "

sha256sum "${BIN}/slogkey"
sha256sum "${BIN}/slogverify"
sha256sum "${BIN}/slogencrypt"
sha256sum "${BIN}/loggen"
sha256sum "${SBIN}/syslog-ng"
sha256sum "${SBIN}/syslog-ng-ctl"
echo " "

if [ "${COPY_TO_HOME_BACKUP}" = "true" ]; then
    echo "#-14"
    #.txt files are deleted without prompting, .log files remain
    echo "You might want to call this script like:"
    echo "$0 2>&1 | tee ${HOME_BACKUP}/protocol_${SCRIPTNAME}_${NOW}.log"
    echo " "
fi

echo "#-15"
echo "return cnt_error: ${cnt_error}"
#if ((cnt_error == 0)); then
if [ "${cnt_error}" -eq 0 ]; then
    echo "PASS"
else
    echo "Found ERROR"
    echo "FAIL"
fi

echo " "
echo Done
echo " "
# exit "${cnt_error:-0}"
exit "${cnt_error}"
