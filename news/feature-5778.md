`logthrdest`: Exponential reconnect backoff with jitter is opt-in via a positive `time-reopen-max()`;
when unset or zero, reconnects retain the fixed `time-reopen()` delay.
With backoff enabled, delays range from half the capped exponential delay (rounded down, at least one second)
to that delay, inclusive, so even the first retry can be shorter than `time-reopen()`.
The cap is applied before jitter and may be smaller than `time-reopen()`; exponential growth stops after ten doublings.
