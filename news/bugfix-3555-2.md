`date-parser()`: fix non-mandatory parsing of timezone name

When %Z is used, the presence of the timezone qualifier is not mandatory,
so don't fail that case.
