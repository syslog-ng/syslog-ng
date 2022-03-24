`set()`: make sure that template formatting options (such as time-zone() or
frac-digits()) are propagated to all references of the rewrite rule
containing a set(). Previously the clone() operation used to implement
multiple references missed the template related options while cloning set(),
causing template formatting options to be set differently, depending on
where the set() was referenced from.
