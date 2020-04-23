@include "scl.conf"

log {
  source { network(transport("udp")); };
  parser { panos-parser(); };
  destination {     
	elasticsearch-http(
	index("syslog-ng-${YEAR}-${MONTH}-${DAY}")
	type("")
	url("http://localhost:9200/_bulk")
	template("$(format-json
	--scope rfc5424 
	--scope dot-nv-pairs --rekey .* --shift 1 --exclude *future_* --exclude *devicegroup_* 
	--scope nv-pairs --exclude DATE --key ISODATE @timestamp=${ISODATE})")
    );};
};
