set(REWRITE_HEADERS
    rewrite/rewrite-expr.h
    rewrite/rewrite-set-tag.h
    rewrite/rewrite-set.h
    rewrite/rewrite-subst.h
    rewrite/rewrite-expr-parser.h
    rewrite/rewrite-groupset.h
    rewrite/rewrite-unset.h
    PARENT_SCOPE
    )

set(REWRITE_SOURCES
    rewrite/rewrite-expr.c
    rewrite/rewrite-set-tag.c
    rewrite/rewrite-set.c
    rewrite/rewrite-subst.c
    rewrite/rewrite-expr-parser.c
    rewrite/rewrite-groupset.c
    rewrite/rewrite-unset.c
    PARENT_SCOPE
    )

add_test_subdirectory(tests)
