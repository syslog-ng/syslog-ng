`regexp-parser()`: due to a change introduced in 3.37, named capture groups
are stored indirectly in the LogMessage to avoid copying of the value.  In
this case the name-value pair created with the regexp is only stored as a
reference (name + length of the original value), which improves performance
and makes such name-value pairs use less memory.  One omission in the
original change in 3.37 is that syslog-ng does not allow builtin values to
be stored indirectly (e.g.  $MESSAGE and a few of others) and this case
causes an assertion to fail and syslog-ng to crash with a SIGABRT. This
abort is now fixed. Here's a sample config that reproduces the issue:

    regexp-parser(patterns('(?<MESSAGE>.*)'));
