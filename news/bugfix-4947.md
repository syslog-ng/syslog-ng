`csv-parser()`: fix escape-backslash-with-sequences dialect on ARM

`csv-parser()` produced invalid output on platforms where char is an unsigned type.
