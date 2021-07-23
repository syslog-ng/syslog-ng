`log-threaded-dest`: Descendant drivers from LogThreadedDest no longer inherit
batch-lines() and batch-timeout() automatically. Each driver have to opt-in for
these options with `log_threaded_dest_driver_batch_option`.

`log_threaded_dest_driver_option` has been renamed to `log_threaded_dest_driver_general_option`,
and `log_threaded_dest_driver_workers_option` have been added similarly to the
batch-related options.
