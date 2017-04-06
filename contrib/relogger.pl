#!/usr/local/bin/perl -w

# take syslog messages from stdin - push them through syslog again

# by Ed Ravin <eravin@panix.com>.  Made available to the
# public courtesy of PANIX (http://www.panix.com).
# This script is licensed under the GPL.
# Requires Date::Parse and Time::HiRes modules


my $usage=
  "relogger.pl [--facility fac] [--priority pri] [--replayspeed factor]\n";

use strict;
use Sys::Syslog qw(:DEFAULT setlogsock);
use Getopt::Long;

use Date::Parse;  # for str2time
use Time::HiRes qw ( sleep );

my %opt;
die $usage unless
	GetOptions (\%opt, "debug", "facility=s", "priority=s", "replayspeed=s");

setlogsock('unix')
        if grep /^ $^O $/xo, ("linux", "openbsd", "freebsd", "netbsd");

my $facility=    $opt{'facility'} || "mail";
my $priority=    $opt{'priority'} || "info";
my $replayspeed= $opt{'replayspeed'} || 0;
my $debug=       $opt{'debug'} || 0;

die "$0: Option 'replayspeed' must be a valid floating point number\n"
	unless $replayspeed =~ /^[0-9]*\.?[0-9]*$/;
my $progname= "";

# Aug  5 20:28:17 grand-central postfix/qmgr[4389]: AC2BB7F9A: removed
#			my $thistime= str2time($date);
#			warn "$0: cannot parse date '$date'\n" if !$thistime;

my $lasttimestamp= 0;
my $timestamp;
my $timestep= 0;

while(<>)
{
    if ( ((my ($timestr, $process, $msg))=  /^(.*) \S+ ([^ []*)\[\d+\]: (.*)$/ ) == 3)
    {
	$timestamp= str2time($timestr) ||
		warn "$0: cannot parse timestamp '$timestr'\n";
        if ($progname ne $process)
        {
	    closelog;
            openlog "$process", 'ndelay,pid', $facility or die "$0: openlog: $!\n";
            $progname= $process;
        }
    
	$timestep= $timestamp - $lasttimestamp;
	if ($replayspeed and $timestep > 0 and $lasttimestamp > 0)
	{
		warn "sleeping for " . $timestep * $replayspeed . " seconds...\n" if $debug;
		sleep( $timestep * $replayspeed);
	}

        syslog $priority, "%s", $msg unless $debug;
		warn "$process $facility/$priority $msg\n" if $debug;
	$lasttimestamp= $timestamp;
    }
	else
	{
		warn "$0: cannot parse input line $.: $_\n";
	}
}


__END__

=head1 NAME

relogger.pl - re-inject syslog log files back into syslog

=head1 SYNOPSIS

B<relogger.pl> [I<--facility fac>] [I<--priority pri>] [I<--replayspeed factor>] [I<--debug]>]

=head1 DESCRIPTION

B<relogger.pl> takes syslog-formatted messages on standard input and re-sends
them via the default syslog mechanism.  The existing timestamps are stripped
off the message before it is re-sent.  Delays between messages can be enabled
with the I<--replayseed> option (see B<OPTIONS> below to simulate the
arrival times of the original messages.

<relogger.pl> was written to help test configurations for programs
like B<logsurfer> or B<swatch> that parse log output and take
actions based on what messages appear in the logs.

=head1 OPTIONS

=item B<--facility> I<fac> specify the syslog facility to log the messages
to.  Standard syslog messages do not store the facility the message was
logged on, so this cannot be determined from the input.  The default is the
B<mail> facility.

=item B<--priority> I<pri> specify the syslog priority to log the messages
to.  The default is the B<info> priority.  As with B<--facility>, this
information cannot be discovered from the input.

=item B<--replayspeed> I<factor> attempt to parse the timestamps
of the input messages, and simulate the original arrival times by sleeping
between each message.  The sleep time is multiplied by I<factor>.  To send
simulated log events with time spacing at the same time as the original
arrival times, use a I<factor> of 1.  To send simulated log events at twice
the speed of the original logs, use a I<factor> of 0.5 (i.e. sleep only
half the original time between log messages).

=item B<--debug> send all output to standard error, rather than to syslog.
Also prints an extra diagnostic message or two.

=head1 BUGS

B<relogger.pl> is a beta-quality tool for testing logging configurations.
It is not yet recommended for production use.

It would be nice to be able to specify the input filename on the command
line, instead of requiring it to be on standard input.

It would be nice to be able to control the syslog mechanism on the
command line (i.e. specify whether to use a local or remote host)
rather than just using the system default.

The original PID in the message is replaced by the current PID of
B<relogger.pl> in the simulated message.  Also, the PID of B<relogger.pl>
will appear in the simulated message even if the original one did not
supply a PID.

In spite of using Time::HiRes to enable sleeping in fractional seconds,
some environments seem to still round off to seconds.  This needs a bit
more investigation.

=head1 AUTHOR

B<relogger.pl> was written by Ed Ravin <eravin@panix.com>, and is made
available to the public by courtesy of PANIX (http://www.panix.com).
This script is licensed under the GPL.  B<relogger.pl> requires the
Date::Parse and the Time::HiRes Perl modules.

