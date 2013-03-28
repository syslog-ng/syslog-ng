#ifndef PLUGIN_TYPES_H_INCLUDED
#define PLUGIN_TYPES_H_INCLUDED

/*
 * Plugin types are defined by the configuration grammar (LL_CONTEXT_*)
 * tokens, but in order to avoid having to directly include cfg-grammar.h
 * (and for the ensuing WTF comments) define this headers to make it a bit
 * more visible that the reason for including that is the definition of
 * those tokens.
 *
 * In the future we might as well generate this header file, but for now we
 * simply poison the namespace with all bison generated macros, as no
 * practical issue has come up until now.
 */

#include "cfg-grammar.h"

#endif
