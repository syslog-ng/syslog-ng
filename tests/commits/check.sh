#!/bin/sh
#############################################################################
# Copyright (c) 2019 Balabit
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

commit_range=origin/master..HEAD

if [ -n "$TRAVIS_COMMIT" ]; then
  git fetch -q origin master:master
  commit_range="master..$TRAVIS_COMMIT"
fi

if [ $# -gt 0 ]; then
  commit_range=$1
fi

git log --no-merges --pretty="%H" $commit_range -- | (
  ret=0
  while read commit; do
    commit_has_valid_subject=1
    commit_has_signed_off_by=1

    subject=$(git show -s --format='%s' $commit)

    if ! echo "$subject" | egrep -q '^[a-z/_-]+:'; then
      commit_has_valid_subject=0
    fi

    if ! git show -s --format='%(trailers)' $commit | grep -q 'Signed-off-by:'; then
      commit_has_signed_off_by=0
    fi

    if [ $commit_has_valid_subject = 1 -a \
         $commit_has_signed_off_by = 1 ]; then
      commit_format='[\033[32;1m OK \033[0m] %.9s %s\n'
    else
      commit_format='[\033[31;1mFAIL\033[0m] %.9s %s\n'
      ret=1
    fi

    printf "$commit_format" "$commit" "$subject" >&2

    if [ $commit_has_valid_subject = 0 ]; then
      cat << EOT >&2
       * This commit subject does not match the project conventions.
EOT
    fi

    if [ $commit_has_signed_off_by = 0 ]; then
      cat << EOT >&2
       * This commit does not have a Signed-off-by line.
EOT
    fi
  done
  if [ $ret -ne 0 ]; then
    cat << EOT >&2

Please modify the commit messages to conform to this pattern:
---------------------------------- 8< ----------------------------------
modulename: short description of the commit

optional longer description

Signed-off-by: Name <email>
---------------------------------- 8< ----------------------------------

To update your commits, run \`git rebase -i ${commit_range%..*}\`, and replace
'pick' by 'reword' on each commit you wan to reword.

The "Signed-off-by" lines can be added automatically by git(1) using the
"--signoff" option when committing.  To add signed-off-by information to all
commits in your branch, run \`git rebase --signoff ${commit_range%..*}\`.
EOT
  fi
  exit $ret
)
