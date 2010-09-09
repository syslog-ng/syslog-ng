#!/usr/bin/env perl
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

sub include_block
{
    my ($start_re, $end_re) = @_;

    open(GR, "<" .$ENV{'top_srcdir'}. "/lib/cfg-grammar.y") || die "Error opening cfg-grammar.y";

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