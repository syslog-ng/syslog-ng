#ifndef FILTERX_BOOLALG_H_INCLUDED
#define FILTERX_BOOLALG_H_INCLUDED

#include "filterx/filterx-expr.h"

FilterXExpr *filterx_unary_not_new(FilterXExpr *rhs);
FilterXExpr *filterx_binary_or_new(FilterXExpr *lhs, FilterXExpr *rhs);
FilterXExpr *filterx_binary_and_new(FilterXExpr *lhs, FilterXExpr *rhs);

#endif
