`logmsg`: Fixed `$SDATA` rendering a bogus `[none]` element when a value named exactly `.SDATA.` was set, and
avoided an unneeded payload copy or growth when a shared message is modified.
