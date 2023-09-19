pdbtool: add validation for types of <value> tags

In patterndb, you can add extra name-value pairs following a match with the tags.
But the actual value of these name-value pairs were never validated against their types,
meaning that an incorrect value could be set using this construct. 
