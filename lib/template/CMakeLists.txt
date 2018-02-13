set(TEMPLATE_HEADERS
    template/templates.h
    template/macros.h
    template/function.h
    template/simple-function.h
    template/repr.h
    template/compiler.h
    template/user-function.h
    template/escaping.h
    template/common-template-typedefs.h
    PARENT_SCOPE)

set(TEMPLATE_SOURCES
    template/templates.c
    template/macros.c
    template/simple-function.c
    template/repr.c
    template/compiler.c
    template/user-function.c
    template/escaping.c
    PARENT_SCOPE)

add_test_subdirectory(tests)
