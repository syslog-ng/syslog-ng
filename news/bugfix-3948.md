`match(), subst() and regexp-parser()`: fixed storing of numbered (e.g.  $1,
$2, $3 and so on) and named capture groups in regular expressions in case
the input of the regexp is the same as one of the match variables being
stored.  In some cases the output of the regexp was clobbered and an invalid
value stored.
