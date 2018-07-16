#!/bin/bash
#############################################################################
# Copyright (c) 2016 Balabit
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

COPYRIGHTVERBOSITY="${COPYRIGHTVERBOSITY-1}" # 0..4, 0=least amount of output
COLUMNS="118" # hack when running in a pipe
PREVIEW="" # non-null to preview files in error

main() {
 if [ $# -ne 3 -a $# -ne 2 ]; then
  echo "usage: $0 <repo dir> <result dir> [<license policy>]" >&2
  echo "export COPYRIGHTVERBOSITY=2 # 0..4, where 0=shorter output" >&2
  return 1
 fi

 REPO="$1"
 BUILDDIR="$2"
 DATADIR="`dirname "$0"`"
 POLICY="$DATADIR/${3-policy}"
 local WORKDIR="`tempfile -p "cpy-" -s ".check_copyright.tmp"`"
 rm "$WORKDIR" &&
 mkdir "$WORKDIR" ||
  { echo "error: can't create $WORKDIR"; return 1 ; }

 [ -d "$BUILDDIR" ] ||
  { echo "error: not a dir $BUILDDIR"; return 1 ; }
 [ -d "$DATADIR" ] ||
  { echo "error: not a dir $DATADIR"; return 1 ; }
 [ -f "$POLICY" ] ||
  { echo "error: not a file $POLICY"; return 1 ; }
 local LOG="$BUILDDIR/copyright-run.log"
 local ERR="$BUILDDIR/copyright-err.log"

 echo "debug: more verbose copyright log in $LOG" >&2

 local SIGS="2 3 6 7 8 9 11 15"
 trap -- "rm --recursive \"$WORKDIR\" ; exit 1;" $SIGS

 cd "$REPO" ||
  { echo "error: not a dir $REPO"; return 1 ; }

 main_logged 2>&1 |
 if [ 0$COPYRIGHTVERBOSITY -ge 2 ]; then
  tee "$LOG"

  grep "^error: " "$LOG" > "$ERR"
 else
  tee "$LOG" |
  grep "^error: " |
  tee "$ERR"
 fi

 local ERRORCNT="`grep --count --extended-regexp "^error: " "$ERR"`"

 {
  if [ 0$COPYRIGHTVERBOSITY -ge 2 ]; then
   if [ 0$ERRORCNT -eq 0 ]; then
    echo "no problem, we're as clean as the ocean" >&2
   else
    echo "$ERRORCNT errors in our philosophy - bummer" >&2
   fi
  elif [ 0$COPYRIGHTVERBOSITY -ge 1 ]; then
   echo "$ERRORCNT"
  fi
 } 2>&1 |
 tee -a "$LOG"

 rm --recursive "$WORKDIR" 2>/dev/null
 trap -- "" $SIGS
 [ 0$ERRORCNT -eq 0 ]
}

main_logged() {
 rm --recursive "$WORKDIR" 2>/dev/null
 mkdir --parents "$WORKDIR" || return 1

 if ! parse_expected_licenses; then
  echo "error: can't parse licenses" >&2
  return 1
 fi

 LICENSES="$WORKDIR/licenses.txt"
 HOLDERS="$WORKDIR/holders.txt"
 MISSING="$BUILDDIR/missing.txt"

 find \
  -L \
  . \
  -iname '.git' -prune \
  -o \
 -type f \
 -exec grep -Iq . {} \; -print |
 sed "s~^\./~~" |
 prune_ignored_paths |
 sort |
 while read FILE; do
  local PATHMATCH="`try_match_path "$FILE"`"

  if [ "$PATHMATCH" = "ignore" ]; then
   if [ 0$COPYRIGHTVERBOSITY -ge 2 ]; then
    echo "debug: ignoring $FILE" >&2
   fi
  else
   cat "$FILE" |
   escape |
   join_lines |
   extract_holder_license "$FILE" |
   try_process_holder_license "$PATHMATCH"
  fi
 done

 sort_holders_licenses
}

try_match_path() {
 local FILE="$1"
 local PATFILE

 local PATHMATCH=""
 for PATFILE in "$WORKDIR"/license.path.*.txt
 do
  if
   echo "$FILE" |
   grep --quiet --extended-regexp --file "$PATFILE"
  then
   local KIND="`\
    echo "$PATFILE" |
    sed --regexp-extended "s~^.*/license\.path\.([^/]+)\.txt$~\1~"`"
   if [ "$KIND" = "ignore" ]; then
    local PATHMATCH="$KIND"
    break
   fi
   if [ -z "$PATHMATCH" ]; then
    local PATHMATCH="$KIND"
   else
    local PATHMATCH="$PATHMATCH,$KIND"
   fi
  fi
 done
 echo "$PATHMATCH"
}

try_process_holder_license() {
 local PATHMATCH="$1"

 read
 HOLDER="$REPLY"
 read
 LICENSE="$REPLY"

 if [ -z "$HOLDER" -a -z "$LICENSE" ]; then
  {
   if
    grep --quiet --ignore-case "copyright" "$FILE"
   then
    printf "error: can't parse $FILE (nonstandard license declaration?)"
   else
    printf "error: missing license $FILE (nonstandard license declaration?)"
   fi
   if [ -n "$PREVIEW" ]; then
    printf " "
    cat "$FILE"
   fi
  } |
  escaped_preview >&2
  if [ -z "$PATHMATCH" ]; then
   echo "error: unmatched path $FILE (add to $POLICY)" >&2
  else
   if [ "0$COPYRIGHTVERBOSITY" -ge 1 ]; then
    echo "info: $FILE path expects $PATHMATCH" >&2
   fi
  fi
 else
  if [ "0$COPYRIGHTVERBOSITY" -ge 4 ]; then
   {
    printf "$FILE\n" >&2
    printf "%s" "$HOLDER" | unescape >&2
    printf "%s" "$LICENSE" | unescape >&2
   } |
   sed "s~^~debug: ~"
  fi

  if [ -z "$HOLDER" ]; then
   echo "error: missing copyright holders in $FILE" >&2
   if [ -n "$PREVIEW" ]; then
    {
     printf "debug: "
     cat "$FILE"
    } |
    escaped_preview >&2
   fi
  else
   printf "%s" "$HOLDER" |
   unescape >> $HOLDERS
  fi

  if [ -z "$LICENSE" ]; then
   echo "error: missing license in $FILE" >&2
   if [ -n "$PREVIEW" ]; then
    {
     printf "debug: "
     cat "$FILE"
    } |
    escaped_preview >&2
   fi
  else
   got_both_holder_and_license
  fi
 fi

 if [ -z "$HOLDER" -o -z "$LICENSE" ]; then
  printf "%s\n" "$FILE" >> $MISSING
 fi
}

got_both_holder_and_license() {
   printf "%s\n" "$LICENSE" >> $LICENSES

   local LICTEXTMATCH="`\
    grep --line-regexp --fixed-strings --with-filename \
     "$LICENSE" "$WORKDIR"/license.text.*.txt |
    head --lines 1 |
    sed --regexp-extended "s~^.*/license\.text\.([^/]+)\.txt:.*$~\1~"`"

   if [ -n "$PATHMATCH" -a -n "$LICTEXTMATCH" ]; then
    compare_expected_detected
   else
    if [ -z "$PATHMATCH" ]; then
     echo "error: unmatched path $FILE (add to $POLICY)" >&2

     if [ -n "$LICTEXTMATCH" -a "0$COPYRIGHTVERBOSITY" -ge 1 ]; then
      echo "info: $FILE detected $LICTEXTMATCH" >&2
     fi
    fi

    if [ -z "$LICTEXTMATCH" ]; then
     {
      printf "error: unknown license $FILE (typo in declaration?)"
      if [ -n "$PREVIEW" ]; then
       echo " $LICENSE"
      fi
     } |
     escaped_preview >&2

     if [ -n "$PATHMATCH" -a "0$COPYRIGHTVERBOSITY" -ge 1 ]; then
      echo "info: $FILE path expects $PATHMATCH" >&2
     fi
    fi
   fi
}

compare_expected_detected() {
    if is_balabit_copyright "$HOLDER"
    then
     local LICTEXTMATCHB="$LICTEXTMATCH"
    else
     local LICTEXTMATCHB="$LICTEXTMATCH,non-balabit"
    fi
    if [ "$PATHMATCH" = "$LICTEXTMATCHB" ]; then
     if [ "0$COPYRIGHTVERBOSITY" -ge 3 ]; then
      echo "debug: match $FILE expected $PATHMATCH = detected $LICTEXTMATCHB" >&2
     fi
    else
     if
      is_license_compatible "$PATHMATCH" "$LICTEXTMATCHB"
     then
      if [ 0$COPYRIGHTVERBOSITY -ge 1 ]; then
       printf "%s\n" "warning: compatible $FILE expected:$PATHMATCH detected:$LICTEXTMATCHB" >&2
      fi
     else
      printf "%s" "error: mismatch $FILE" >&2
      if [ 0$COPYRIGHTVERBOSITY -ge 1 ]; then
       printf " expected:$PATHMATCH detected:$LICTEXTMATCHB" >&2
      fi
      printf "\n" >&2
     fi
    fi
}

extract_holder_license() {
 local FILE="$1"

 case "$FILE" in
  configure.in)
    extract_holder_license_sh
    return $?
 esac

 local EXT="`echo "$FILE" | sed -r "s~^.*\.([^.]+)$~\1~"`"
 case "$EXT" in
  c|h|ym|java)
    extract_holder_license_c
    ;;
  ac|am|cmake|conf|sh|pl|py)
    extract_holder_license_sh
    ;;
  *)
    echo "info: unknown file extension $FILE" >&2
 esac
}

extract_holder_license_sh() {
sed --regexp-extended "
 s~^\
(#![^<]*<br>|)\
(#[^<]*<br>|)\
#############################################################################<br>\
(\
(# Copyright \(c\) ([0-9, -]+) [^ <][^<]*<br>)+\
)\
#<br>\
(\
#( [^ <]([^<]|<[^b][^>]*>|<b[^r>][^>]*>|<br[^>]+>)*)<br>\
(#(| [^ <]([^<]|<[^b][^>]*>|<b[^r>][^>]*>|<br[^>]+>)*)<br>)*\
#( [^ <]([^<]|<[^b][^>]*>|<b[^r>][^>]*>|<br[^>]+>)*)<br>\
|\
(#(| [^ <]([^<]|<[^b][^>]*>|<b[^r>][^>]*>|<br[^>]+>)*)<br>)*\
#( [^ <]([^<]|<[^b][^>]*>|<b[^r>][^>]*>|<br[^>]+>)*)<br>\
)\
#<br>\
#############################################################################<br>\
.*$\
~\3\n\6~
t success
 s~^.*$~~
:success
" |
 sed --regexp-extended "
  s~(^|<br>)# ?~\1~g
"
}

extract_holder_license_c() {
# note that holders must be present in the present implementation
sed --regexp-extended "
 s~^\
 */\*<br>\
(\
( \* Copyright \(c\) ([0-9, -]+) [^ <][^<]*<br>)+\
)\
( \*<br>)+\
(\
 \*( [^ <]([^<]|<[^b][^>]*>|<b[^r>][^>]*>|<br[^>]+>)*)<br>\
( \*(| [^ <]([^<]|<[^b][^>]*>|<b[^r>][^>]*>|<br[^>]+>)*)<br>)*\
 \*( [^ <]([^<]|<[^b][^>]*>|<b[^r>][^>]*>|<br[^>]+>)*)<br>\
|\
( \*(| [^ <]([^<]|<[^b][^>]*>|<b[^r>][^>]*>|<br[^>]+>)*)<br>)*\
 \*( [^ <]([^<]|<[^b][^>]*>|<b[^r>][^>]*>|<br[^>]+>)*)<br>\
)\
( \*<br>)*\
 \*/<br>\
.*$\
~\1\n\5~
t success
 s~^.*$~~
:success
" |
 sed --regexp-extended "
  s~(^|<br>) \* ?~\1~g
"
}

is_balabit_copyright() {
 echo "$@" |
 grep --quiet --extended-regexp "\
^\
(Copyright \(c\) ([0-9, -]+) [^ <][^<]*<br>)*\
Copyright \(c\) ([0-9, -]+) Bala[bB]it<br>\
(Copyright \(c\) ([0-9, -]+) [^ <][^<]*<br>)*\
$"
 return $?
}

parse_expected_licenses() {
 generate_known_licenses || return 1

 cat "$POLICY" |
 grep --invert-match --extended-regexp "^\s*$" |
 sed "s~[\\]~&&~g" |
 sed "
  s~^ ~L ~
  t e
  s~^~P ~
  :e
 " |
 {
 local ERR=0
 while read KIND LINE
 do
  case "$KIND" in
   L)
    local LICENSE="`
     echo "$LINE" |
     sed -rn "
      s~^(ignore|((GPLv2\+|LGPLv2\.1\+)(_SSL)?|4-BSD)(,non-balabit)?)$~&~
      T e
      p
      :e
     "
    `"
    if [ -z "$LICENSE" ]; then
     echo "error: can't parse expected license in policy: '${LINE}'" >&2
     local ERR=1
    fi
    ;;
   P)
    if [ -n "$LICENSE" ]; then
     echo "$LINE" |
     sed --regexp-extended "
      s~^~^~
      s~$~(/.*)?$~
     " >> "$WORKDIR/license.path.$LICENSE.txt"
    else
     echo "error: ignored unknowned licensed path '${LINE}' in $POLICY" >&2
     local ERR=1
    fi
    ;;
   *)
    echo "error: assertion failed in parse_expected_licenses: unknown key '$KIND $LINE'" >&2
    local ERR=1
  esac
 done

 return $ERR
 }
}

is_license_compatible() {
 local EXPECTED="$1"
 local SUBMITTED="$2"

 if [ "$EXPECTED" = "$SUBMITTED" ]; then
  return 0
 fi

 case "$EXPECTED" in
  GPLv2+_SSL)
   [ "$SUBMITTED" = "LGPLv2.1+_SSL" ]
   ;;
  *)
   false
 esac
}

generate_known_licenses() {
 for FILE in "$DATADIR"/license.text.*.txt; do
  local NAME="`basename "$FILE"`"
  cat "$FILE" |
  escape |
  join_lines > "$WORKDIR/$NAME"
 done
}

prune_ignored_paths() {
 local IGNORE="$WORKDIR/ignore.paths.txt"
 if [ ! -f "$IGNORE" ]; then
  git submodule status |
  cut -d " " -f 3 |
  sed --regexp-extended "
   s~^~^~
   s~$~(/.*)?$~
  " |
  cat > "$IGNORE"
  echo "*~" >> "$IGNORE"
 fi

 grep --invert-match --extended-regexp --file "$IGNORE"
}

sort_holders_licenses() {
 {
  for FILE in $LICENSES $HOLDERS; do
   local DEST="$BUILDDIR/`basename $FILE`"
   if [ -f "$FILE" ]; then
    cat "$FILE" |
    sort |
    uniq -c |
    sort -n > "$DEST"

    if [ "0$COPYRIGHTVERBOSITY" -ge 2 ]; then
     echo "$DEST"
     cat "$DEST"
    fi
   fi
  done

  if [ -f "$HOLDERS" ]; then
   local DEST="$BUILDDIR/holders.name.txt"
   cat "$HOLDERS" |
   sed --regexp-extended "s~^ \* Copyright \(c\) [0-9, -]*~~" |
   sort |
   uniq -c |
   sort -n > "$DEST"

    if [ "0$COPYRIGHTVERBOSITY" -ge 2 ]; then
     echo "$DEST"
     cat "$DEST"
    fi
  fi
 } |
 sed "s~^~debug: ~" >&2
}

# note: needs escaped data as input
join_lines() {
 sed "
:join_lines
N
s~\n~<br>~
t join_lines" "$@" |
sed "s~$~<br>~"
}

escape() {
 sed "
s~&~\&amp;~g
s~<~\&lt;~g
s~>~\&gt;~g
" "$@"
}

unescape() {
 sed "
s~<br>~\n~g
s~&gt;~>~g
s~&lt;~<~g
s~&amp;~\&~g
" "$@"
}

escaped_preview() {
 escape "$@" |
 join_lines |
 preview |
 sed "s~<br>$~~" # TODO
}

preview() {
 if [ -n "$COLUMNS" ]; then
  local COLS="$COLUMNS"
 elif which tput >/dev/null; then
  local COLS="`tput cols`"
 fi
 [ -z "$COLS" ] &&
  local COLS=80

 cat "$@" |
 if [ -n "$PREVIEW" ]; then
  dd bs=1 count=$COLS status=noxfer 2>/dev/null
 else
  cat
 fi
 echo
}

main "$@"
