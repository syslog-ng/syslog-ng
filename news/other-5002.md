multiline-options: Allow `multi_line_timeout` to be set to a non-integer value.

Since `multi_line_timeout` is suggested to be set as a multiple of `follow-freq`, and `follow-freq` can be much smaller than one second, it makes sense to allow this value to be a non-integer as well.
