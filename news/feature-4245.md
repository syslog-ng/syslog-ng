`system()` source: the `system()` source was changed on systemd platforms to
fetch journal messages that relate to the current boot only (e.g.  similar
to `journalctl -fb`) and to ignore messages generated in previous boots,
even if those messages were succesfully stored in the journal and were not
picked up by syslog-ng.  This change was implemented as the journald access
APIs work incorrectly if time goes backwards across reboots, which is an
increasingly frequent event in virtualized environments and on systems that
lack an RTC.  If you want to retain the old behaviour, please bypass the
`system()` source and use `systemd-journal()` directly, where this option
can be customized. The change is not tied to `@version` as we deemed the new
behaviour fixing an actual bug. For more information consult #2836.

`systemd-journald()` source: add `match-boot()` and `matches()` options to
allow you to constrain the collection of journal records to a subset of what
is in the journal. `match-boot()` is a yes/no value that allows you to fetch
messages that only relate to the current boot. `matches()` allows you to
specify one or more filters on journal fields.

Examples:

```
source s_journal_current_boot_only {
  systemd-source(match-boot(yes));
};

source s_journal_systemd_only {
  systemd-source(matches(
    "_COMM" => "systemd"
    )
  );
};
```
