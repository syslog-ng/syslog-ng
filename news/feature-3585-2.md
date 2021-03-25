`usertty() destination`: Support changing the terminal disable timeout with the `time-reopen()` option.
Default timeout change to 60 from 600. If you wish to use the old 600 timeout, add `time-reopen(600)`
to your config in the `usertty()` driver.
