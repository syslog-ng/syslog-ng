Allow dupnames flag to be used in PCRE expressions, allowing duplicate names for named subpatterns
as explained here: https://www.pcre.org/original/doc/html/pcrepattern.html#SEC16 .

Example:
filter f_filter1 {
  match("(?<FOOBAR>bar)|(?<FOOBAR>foo)" value(MSG) flags(store-matches, dupnames)
 );
};
