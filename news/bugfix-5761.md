`logmatcher`: fix memory leaks in glob and PCRE matchers

Fix a `GPatternSpec` leak when the glob matcher is recompiled, a NULL dereference
in `glob_free()` when compile was never called, and two PCRE leaks: `pcre2_code`
not freed on recompile, and `nv_prefix` not freed on destroy.
