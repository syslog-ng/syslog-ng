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

/** @file lib/riack/dtupdatereq.h
 */

/** @defgroup riack_dtupdatereq Riak DtUpdateReq
 *
 *
 * @addtogroup riack_dtupdatereq
 * @{
 */

#ifndef __RIACK_DTUPDATEREQ_H__
#define __RIACK_DTUPDATEREQ_H__

#include <riack/proto/riak_kv.pb-c.h>
#include <riack/proto/riak_dt.pb-c.h>
#include <riack/message.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef RpbContent riack_content_t;
typedef RpbPutReq riack_put_req_t;
typedef DtOp riack_dt_op_t;
typedef SetOp riack_setop_t;
typedef DtUpdateReq riack_dt_update_req_t;


typedef enum
  {
    RIACK_REQ_DT_UPDATE_FIELD_NONE,
    RIACK_REQ_DT_UPDATE_FIELD_BUCKET,
    RIACK_REQ_DT_UPDATE_FIELD_BUCKET_TYPE,
    RIACK_REQ_DT_UPDATE_FIELD_KEY,
    RIACK_REQ_DT_UPDATE_FIELD_DT_OP,

  } riack_req_dt_update_field_t;


riack_dt_update_req_t *riack_req_dt_update_new (void);
void riack_req_dt_update_free (riack_dt_update_req_t *dtupdatereq);

int riack_req_dt_update_set (riack_dt_update_req_t *dtupdatereq, ...);

riack_message_t * riack_dtupdatereq_serialize(riack_dt_update_req_t *dtupdatereq);

//used to serialize the data that is to be sent to riak


#ifdef __cplusplus
} /* extern "C" */
#endif

/** @} */

#endif
