#!/usr/bin/awk -f
#
# syslog2ng
#
# Translator from syslog.conf to syslog-ng.conf
# by Jonathan W. Marks <j-marks@uiuc.edu>
#
# Rev 2

BEGIN {
	# Handle the various platforms- determine proper log device
	# Output the basic options and source statement.
	print \
"source msgs {\n" \
"	system();\n" \
"	network(transport(udp));\n" \
"	internal();\n" \
"};\n";
}

$1 !~ /^[:space:]*#/ && NF == 2 {

	# Output a comment with the line being translated.
	print "# " $0 "\n";

	# Output any new filters to be created, saving filter ID numbers
	# needed by destination
	requiredFilterNos = make_filters($1);

	# Output the destination to be used, saving destination ID number
	destNo = make_destination($2)

	# Output the log path, connecting the required filters to the
	# destination.
	make_log(destNo, requiredFilterNos);
}

function make_filters(filterstr, filterNumbers) {

	# Split the components of the filter specifier. For each component,
	# generate the appropriate filter, and collect the filter numbers.

	split(filterstr, termlist, ";");
	for (termNo in termlist) {
		newNum = make_filter(termlist[termNo]);
		filterNumbers = filterNumbers " " newNum;
	}
	return filterNumbers;
}

function make_filter(spec, negate) {

	# Find the severity and facility list.
	dot = index(spec, ".");
	severity = substr(spec, (dot + 1));
	split(substr(spec, 1, (dot - 1)), faclist, ",");

	if (severity == "none") { negate = 1 };
	if (severity == "*")    { severity = "debug" };

	# Create an ID string using severity and facility list to hash
	# into all_filters. Then we can tell whether weve already built
	# a filter like this.
	filterID = severity;
	for (facno in faclist) {
		filterID = filterID " " faclist[facno];
	}

	# If this is a new filter, output the syslog-ng directives for it
	# and save its ID and number in all_filters.
	if (! (filterID in all_filters)) {
		all_filters[filterID] = ++filterNum;

		printf "filter f_" filterNum " {\n\t";
		nPrinted = 0;

		# If using all facilities, no need to include them all in
		# filter-- its really only a filter based on severity
		if (faclist[1] != "*") {
			printf("%sfacility(", (negate ? "not " : ""));
			for (facno in faclist) {
				printf("%s" faclist[facno], \
					(nPrinted++ > 0 ? "," : ""));
			}
			printf(")%s", (severity != "none" ? " and " : ""));
		}
		if (severity != "none") {
			if (severity ~ /^[^=]/) {
				printf("level(" severity "%s)",
					(severity == "emerg" ? "" : "..emerg"));
			}
			else {
				printf("level(" substr(severity, 2) ")");
			}
		}
		printf(";\n };\n\n");
	}

	return all_filters[filterID];
}

function make_destination(d, destNo) {

	# If weve already built this destination, dont do it again.
	# Just return the ID number.
	if (d in destinations) {
		return destinations[d];
	}

	# Remember the destination ID number in case we need it again.
	destNo = ++dno;
	destinations[d] = destNo;

	# Output the syslog-ng directive for the destination.
	printf "destination d_" destNo " { \n";

	if (d ~ /^@/) {
		printf "\tnetwork(\"" substr(d, 2) "\" transport(udp) port(514));\n";
	}
	else if (d ~ /^\|\//) {
		printf "\tpipe(\"" substr(d, 2) "\");\n";
	}
	else if (d ~ /^\//) {
		printf "\tfile(\"" d "\");\n";
	}
	else if (d ~ /^-\//) {
		printf "\tfile(\"" substr(d, 2) "\");\n";
	}
	else {
		printf "\tusertty(\"" d "\");\n";
	}

	print "};\n";
	return destNo;
}

function make_log(destNo, filterNos) {

	# Note the destination number and filter numbers, then output
	# a syslog-ng directive connecting them.
	n_entries = split(filterNos, filters, " ");
	printf "log { source(msgs); " ;
	for (i = 1; i <= n_entries; i++) {
		printf "filter(f_" filters[i] "); ";
	}
	print "destination(d_" destNo "); };\n";
}
