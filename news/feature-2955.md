`$(list-search)`: Added a new template function, which returns the first index of a pattern in a list.
Starts the search at `start_index`. 0 based. If not found, returns empty string.
Usage:
 * Literal match:
     `$(list-search <pattern> ${list})`
     or
     `$(list-search --mode literal <pattern> ${list})`
 * Prefix match: `$(list-search --mode prefix <pattern> ${list})`
 * Substring match: `$(list-search --mode substring <pattern> ${list})`
 * Glob match: `$(list-search --mode glob <pattern> ${list})`
 * Regex match: `$(list-search --mode pcre <pattern> ${list})`
Add `--start-index <index>` to change the start index.
