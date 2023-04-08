`group-lines()` parser: this new parser correlates multi-line messages
received as separate, but subsequent lines into a single log message.
Received messages are first collected into streams related messages (using
key()), then collected into correlation contexts up to timeout() seconds.
The identification of multi-line messages are then performed on these
message contexts within the time period.

```
  group-lines(key("$FILE_NAME")
              multi-line-mode("smart")
	      template("$MESSAGE")
	      timeout(10)
	      line-separator("\n")
  );
```

Configuration options are as follows:
  key() -- specifies a template that determines what messages are forming a
           single stream. Messages where the template expansion results in the same
           key are considered part of the same stream. Using key() you can apply
           multi-line extraction even if different streams are interleaved in your
           input.

  multi-line-mode() -- same as multi-line-mode() for multi-line file
                       sources.  Value is one of: `smart`, `indented`,
                       `prefix-garbage` or `prefix-suffix`.

  multi-line-prefix(), multi-line-suffix(), multi-line-garbage() --
                       regular expressions associated with regexp based multi-line-mode
                       values.

  template() -- a template string that specifies what constitutes an line to
                group-lines(). In simplest cases this is $MSG or $RAWMSG.

  line-separator() -- in case a multi-line message is found, this value is
                      inserted between segments of the new multi-line message. Defaults to '\n'
                      that is the newline character.
