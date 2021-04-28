date-parser: if the timestamp pattenr did not covered a field (for example seconds) that field had undefined value

The missing fields are initialized according to the following rules:
 1) missing all fileds -> use current date
 2) only miss year -> guess year based on current year and month (current year, last year or next year)
 3) the rest of the cases don't make much sense, so zero initialization of the missing field makes sense. And the year is initializeed to the current one.

