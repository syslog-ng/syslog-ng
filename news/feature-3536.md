fortigate-parser(): add a new parser to parse fortigate logs

log {
  source { network(transport("udp") flags(no-parse)); };
  parser { fortigate-parser(); };
  destination { };
};

An adapter to automatically recognize fortigate logs in app-parser() has
also been added.
