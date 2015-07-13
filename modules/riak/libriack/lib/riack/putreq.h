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

/** @file lib/riack/putreq.h
 */

/** @defgroup riack_putreq Riak RpbPutReq
 *
 *
 * @addtogroup riack_putreq
 * @{
 */

#ifndef __RIACK_PUTREQ_H__
#define __RIACK_PUTREQ_H__

#include <riack/proto/riak_kv.pb-c.h>
#include <riack/message.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef RpbContent riack_content_t;
typedef RpbPutReq riack_put_req_t;

typedef enum
  {
    RIACK_REQ_PUT_FIELD_NONE,
    RIACK_REQ_PUT_FIELD_BUCKET,
    RIACK_REQ_PUT_FIELD_BUCKET_TYPE,
    RIACK_REQ_PUT_FIELD_KEY,
    RIACK_REQ_PUT_FIELD_CONTENT,
  } riack_req_put_field_t;


riack_put_req_t *riack_req_put_new (void);
void riack_req_put_free (riack_put_req_t *putreq);

int riack_req_put_set (riack_put_req_t *putreq, riack_req_put_field_t RIACK_REQ_PUT_FIELD_BUCKET_TYPE, ...);

riack_message_t * riack_putreq_serialize(riack_put_req_t *putreq);
//used to serialize the data that is to be sent to riak


#ifdef __cplusplus
} /* extern "C" */
#endif

/** @} */

#endif
