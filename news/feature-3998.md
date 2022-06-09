`grouping-by()`: added `inject-mode(aggregate-only)`

This inject mode will drop individual messages that make up the correlation
context (`key()` groups) and would only yield the aggregate messages
(e.g. the results of the correlation).
