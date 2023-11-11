#ifndef FILTERX_COMPARISON_H_INCLUDED
#define FILTERX_COMPARISON_H_INCLUDED

#include "filterx/filterx-expr.h"

#define FCMPX_EQ                   0x0001
#define FCMPX_LT                   0x0002
#define FCMPX_GT                   0x0004
#define FCMPX_TYPE_AWARE           0x0010
#define FCMPX_STRING_BASED         0x0020
#define FCMPX_NUM_BASED            0x0040
#define FCMPX_TYPE_AND_VALUE_BASED 0x0080

#define FCMPX_OP_MASK      0x0007
#define FCMPX_MODE_MASK    0x00F0

FilterXExpr *filterx_comparison_new(FilterXExpr *lhs, FilterXExpr *rhs, gint operator);


#endif
