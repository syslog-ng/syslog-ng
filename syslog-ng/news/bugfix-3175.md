`format-json`: fix printing of embedded zeros

Prior to 2.64.1, `g_utf8_get_char_validated()` in glib falsely identified embedded zeros as valid utf8 characters. As a result, format json printed the embedded zeroes as `\u0000` instead of `\x00`. This change fixes this problem.
