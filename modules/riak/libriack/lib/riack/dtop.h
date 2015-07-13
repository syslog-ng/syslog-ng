/* riack -- Riak C client library
 * Copyright (C) 2015  Gergely Nagy <algernon@madhouse-project.org>
 * Copyright (C) 2015  Parth Oberoi <htrapdev@gmail.com>
 *
 * This library is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file lib/riack/dtop.h
 */

/** @defgroup riack_dtop Riak DtOp
 *
 *
 * @addtogroup riack_dtop
 * @{
 */

#ifndef __RIACK_DTOP_H__
#define __RIACK_DTOP_H__

#include <riack/proto/riak_kv.pb-c.h>
#include <riack/proto/riak_dt.pb-c.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef DtOp riack_dt_op_t;
typedef SetOp riack_setop_t;

typedef enum
{
  RIACK_DT_OP_FIELD_NONE,
  RIACK_DT_OP_FIELD_SETOP
} riack_dt_op_field_t;

riack_dt_op_t *riack_dt_op_new (void);
void riack_dt_op_free (riack_dt_op_t *dtop);

int riack_dt_op_set (riack_dt_op_t *dtop, ...);

#ifdef __cplusplus
} /* extern "C" */
#endif

/** @} */

#endif
