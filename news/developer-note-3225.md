stats: add external stats counters

* Motivation:
There are situations when someone wants to expose internal variables through stats-api.
Without this changeset, we need two distinct variables: one is for the internal state, other is for the
stats registration (internal state cannot depends on the life cycle of the StatsCounterGroup).


* Proposed solution: External stats counters

When someone registers
 * a "normal" stats counter, the registry API provides a pointer to an instance of a StatsCounterItem.
StatsCounterItems are stored in a StatsCounterGroup (as an array, "indexed" by type), StatsCounterGroup
is part of StatsCluster. Life cycle is defined by the counter group.
StatsCounterItem has-an `atomic_gssize` variable. So the `stats_counter_dec/sub/add/...` API modifies this
`atomic_gssize` variable which is stored in an array (which is part of the StatsCounterGroup).

 * an "external" stats counter, the registry API takes a pointer to an `atomic_gssize` variable.
The provided StatsCounterItem will store this pointer AND mark the StatsCounterItem instance as read-only:
this means that the value, stored by the StatsCounterItem, can only be modified by the owner of the external
`atomic_gssize` variable.

* Implementation:
StatsCounterItem is a union: it stores an `atomic_gssize` or a pointer to an `atomic_gssize` value (so a direct
or an indirect value).

Currently, it is possible to register a StatsCounterItem and get back an external counter. The other direction
is not supported: when a counter is registered as an internal, it can't be re-registered as an external (assert).
Re-register an already registered external counter with a different `atomic_gssize` pointer is not supported (assert).

When the `use_count` reaches zero, the pointer is set to NULL and `live_mask` also updated.
This means that after all the references to an external StatsCounterItem removed (untrack/unregister calls), then it is
possibble to re-register a new StatsCounterItem (normal, or external).
As in case of the original/normal stats counter the live mask is not updated in this case (`use_count` reaches zero),
trying to register an external variable ends in an assert.
