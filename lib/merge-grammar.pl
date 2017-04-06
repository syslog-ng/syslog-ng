#!/usr/bin/env perl
#############################################################################
# Copyright (c) 2010-2013 Balabit
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
#
# As an additional exemption you are allowed to compile & link against the
# OpenSSL libraries as published by the OpenSSL project. See the file
# COPYING for details.
#
#############################################################################
#
# This script imports the rules from the main grammar file. It was
# originally written in perl in order not to require Python for plugin
# development, but since Python is already used for the test suite, I guess
# it is not so important anymore.
#
# TODOs:
#  * rewrite it in Python
#  * be more clever about which rules to include as bison generates a lot of
#    warnings about unused rules
#

use File::Basename;

sub include_block
{
    my ($start_re, $end_re) = @_;

    open(GR, "<" .$ENV{'top_srcdir'}. "/lib/cfg-grammar.y") ||
            open(GR, "<" . dirname($0) . "/cfg-grammar.y") || die "Error opening cfg-grammar.y";

    my $decl_started = 0;
    while (<GR>) {
        if (/$start_re/) {
            $decl_started = 1;
        }
        elsif (/$end_re/) {
            $decl_started = 0;
            break;
        }
        elsif ($decl_started) {
            print;
        }
    }
    close(GR);
}

while (<>) {
    if (/INCLUDE_DECLS/) {
        include_block("START_DECLS", "END_DECLS");
    }
    elsif (/INCLUDE_RULES/) {
        include_block("START_RULES", "END_RULES");
    }
    else {
        print;
    }
}
