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

#include <riack/putreq.h>
#include <riack/message.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <netinet/in.h>
#include <errno.h>


riack_put_req_t *
riack_req_put_new (void)
{

  riack_put_req_t *putreq = (riack_put_req_t*)malloc(sizeof(riack_put_req_t));
  rpb_put_req__init(putreq);
  
  
  return putreq;
}

void
riack_req_put_free (riack_put_req_t *putreq)
{
  
  rpb_put_req__free_unpacked(putreq,NULL);
  
  
}

int
riack_req_put_set (riack_put_req_t *putreq, riack_req_put_field_t bflag, ...)
{
  va_list args;
  if (putreq == NULL)
    return -EINVAL;
  int flag; 
  char *bucket;
  char *bucket_type;
  
  char *key;
  riack_content_t *content;
  
  va_start(args, bflag);
  bucket = (char *)va_arg(args, char *);
  if (bucket == NULL)
    return -EINVAL;
  else {
    if (putreq->bucket.data)
      free(putreq->bucket.data);
    putreq->bucket.data = (unsigned char *)strdup(bucket);
    putreq->bucket.len = strlen(bucket);
    }
  while ((flag = va_arg(args, int)) != 0)
  {
    
    switch (flag) {
      case (RIACK_REQ_PUT_FIELD_BUCKET_TYPE):
        bucket_type  = (char *)va_arg(args, char *);
        if (putreq->type.data)
          free(putreq->type.data);
        putreq->type.data = (unsigned char *)strdup(bucket_type);
        putreq->type.len = strlen(bucket_type);
        
        break;
      case (RIACK_REQ_PUT_FIELD_KEY):
        key = (char *)va_arg(args, char *);
        if(putreq->key.data)
          free(putreq->key.data);
        putreq->key.data = (unsigned char *)strdup(key);
        putreq->key.len = strlen(key);
        break;
      case (RIACK_REQ_PUT_FIELD_CONTENT):
        content = (riack_content_t *)va_arg(args, riack_content_t *);
        putreq->content = content;
        
        break;
        
     }
  }
  if(flag == RIACK_REQ_PUT_FIELD_NONE)
    return 0;
     
  return -errno;
}

riack_message_t *
riack_putreq_serialize(riack_put_req_t *putreq)
{
  uint32_t length;
  riack_message_t * object;
  length = rpb_put_req__get_packed_size(putreq);

  object = (riack_message_t *) malloc(length + sizeof (object->length) + sizeof (object->message_code));
  object->length = htonl (length + 1);
  object->message_code = 11;
  rpb_put_req__pack(putreq, object->data);
  return object;
}

