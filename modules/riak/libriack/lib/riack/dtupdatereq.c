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

#include <riack/dtupdatereq.h>
#include <riack/message.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <netinet/in.h>

riack_dt_update_req_t *
riack_req_dt_update_new (void)
{
  riack_dt_update_req_t *dtupdatereq = (riack_dt_update_req_t*)malloc(sizeof (riack_dt_update_req_t));
  dt_update_req__init(dtupdatereq);
  return dtupdatereq;
}

void 
riack_req_dt_update_free (riack_dt_update_req_t *dtupdatereq)
{
  dt_update_req__free_unpacked (dtupdatereq, NULL);
}

int 
riack_req_dt_update_set (riack_dt_update_req_t *dtupdatereq, ...)
{
  va_list args;
  if (dtupdatereq == NULL)
    return -EINVAL;
  int flag; 
  char *bucket;
  char *bucket_type;
  riack_dt_op_t *dtop;
  char *key;

  va_start(args, dtupdatereq);
  
  while ((flag = va_arg(args, int)) != RIACK_REQ_DT_UPDATE_FIELD_NONE)
    {
      switch (flag)
        {
          case (RIACK_REQ_DT_UPDATE_FIELD_BUCKET):
            bucket = (char *)va_arg(args, char *);
            if (dtupdatereq->bucket.data)
              free(dtupdatereq->bucket.data);
            dtupdatereq->bucket.data = (unsigned char *)strdup(bucket);
            dtupdatereq->bucket.len = strlen(bucket);
            break;
            
          case (RIACK_REQ_DT_UPDATE_FIELD_BUCKET_TYPE):
            bucket_type  = (char *)va_arg(args, char *);
            if (dtupdatereq->type.data)
              free(dtupdatereq->type.data);
            dtupdatereq->type.data = (unsigned char *)strdup(bucket_type);
            dtupdatereq->type.len = strlen(bucket_type);
            break;
            
          case (RIACK_REQ_DT_UPDATE_FIELD_KEY):
            key = (char *)va_arg(args, char *);
            if(dtupdatereq->key.data)
              free(dtupdatereq->key.data);
            if (key)
              {
                dtupdatereq->key.data = (unsigned char *)strdup(key);
                dtupdatereq->key.len = strlen(key);
              }
            break;
            
          case (RIACK_REQ_DT_UPDATE_FIELD_DT_OP):
            dtop = (riack_dt_op_t *)va_arg(args, riack_dt_op_t *);
            dtupdatereq->op = dtop;      
          break;
        
        }
    }
  
  if(flag == RIACK_REQ_DT_UPDATE_FIELD_NONE)
    return 0;
  
  return -errno;
}

riack_message_t * 
riack_dtupdatereq_serialize(riack_dt_update_req_t *dtupdatereq)
{
  uint32_t length;
  riack_message_t * object;
  length = dt_update_req__get_packed_size(dtupdatereq);

  object = (riack_message_t *) malloc(length + sizeof (object->length) + sizeof (object->message_code));
  object->length = htonl (length + 1);
  object->message_code = 82;
  dt_update_req__pack(dtupdatereq, object->data);
  return object;

}
