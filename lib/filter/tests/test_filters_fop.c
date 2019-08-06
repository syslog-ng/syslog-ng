/*
 * Copyright (c) 2018 Balabit
 * Copyright (c) 2019 Szemere
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 */
#include "filter/filter-op.h"
#include "filter/filter-expr.h"
#include "filter/filter-pri.h"
#include "filter/filter-expr-parser.h"
#include "test_filters_common.h"
#include "cfg-lexer.h"
#include "apphook.h"

#include <criterion/criterion.h>
#include <criterion/parameterized.h>

static FilterExprNode *
_compile_standalone_filter(gchar *config_snippet)
{
  GlobalConfig *cfg = cfg_new_snippet();
  FilterExprNode *tmp;

  CfgLexer *lexer = cfg_lexer_new_buffer(cfg, config_snippet, strlen(config_snippet));
  cr_assert(lexer, "Couldn't initialize a buffer for CfgLexer");

  cr_assert(cfg_run_parser(cfg, lexer, &filter_expr_parser, (gpointer *) &tmp, NULL));

  cfg_free(cfg);

  return tmp;
}

typedef struct _FilterParams
{
  gchar *config_snippet;
  gboolean expected_result;
} FilterParams;

ParameterizedTestParameters(filter_op, test_or_evaluation)
{
  static FilterParams test_data_list[] =
  {
    // Filters inside evaluates to TRUE
    {.config_snippet = "    facility(2) or     facility(2)", .expected_result = TRUE  },
    {.config_snippet = "    facility(2) or not facility(2)", .expected_result = TRUE  },
    {.config_snippet = "not facility(2) or     facility(2)", .expected_result = TRUE  },
    {.config_snippet = "not facility(2) or not facility(2)", .expected_result = FALSE },
    // note: The above expression evaluated in the following way: not (TRUE or (not TRUE))
    //       The expression below has the same expected result, but for a different reason.
    {.config_snippet = "(not facility(2)) or (not facility(2))", .expected_result = FALSE },

    // Filters inside evaluates to FALSE
    {.config_snippet = "    facility(3) or     facility(3)", .expected_result = FALSE },
    {.config_snippet = "    facility(3) or not facility(3)", .expected_result = TRUE  },
    {.config_snippet = "not facility(3) or     facility(3)", .expected_result = TRUE  },
    {.config_snippet = "not facility(3) or not facility(3)", .expected_result = TRUE  },
    // same as before
    {.config_snippet = "(not facility(3)) or (not facility(3))", .expected_result = TRUE  },

    // Mixed TRUE and FALSE evaluations
    {.config_snippet = "    facility(2) or     facility(3)", .expected_result = TRUE  },
    {.config_snippet = "    facility(2) or not facility(3)", .expected_result = TRUE  },
    {.config_snippet = "not facility(2) or     facility(3)", .expected_result = FALSE },
    {.config_snippet = "not facility(2) or not facility(3)", .expected_result = TRUE  },
    // same as before
    {.config_snippet = "(not facility(2)) or (not facility(3))", .expected_result = TRUE },
  };

  return cr_make_param_array(FilterParams, test_data_list, G_N_ELEMENTS(test_data_list));
}

ParameterizedTest(FilterParams *params, filter_op, test_or_evaluation)
{
  const gchar *msg = "<16> openvpn[2499]: PTHREAD support initialized";
  FilterExprNode *filter = _compile_standalone_filter(params->config_snippet);
  testcase(msg, filter, params->expected_result);
}

TestSuite(filter_op, .init = setup, .fini = teardown);
