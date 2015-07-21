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

#include <riack/setop.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>

riack_setop_t *
riack_setop_new (void)
{
  riack_setop_t *setop = (riack_setop_t*)malloc(sizeof(riack_setop_t));
  set_op__init(setop);
  return setop;
}

void 
riack_setop_free (riack_setop_t *setop)
{
  if (setop !=NULL)
    {
      set_op__free_unpacked(setop,NULL);
      setop = NULL;
    }
}


int 
riack_setop_set (riack_setop_t *setop, ...)
{

  va_list args;
  char *adds;
  char *removes;
  int flag;
  int idx;
  
  if (setop == NULL)
    return -EINVAL;
  
  va_start(args, setop);
  while ((flag = va_arg(args, int)) != RIACK_SETOP_FIELD_NONE)
    {
      switch (flag)
        {
          case (RIACK_SETOP_FIELD_BULK_ADD):
            idx = va_arg(args, int);
            //if (setop->adds[idx].data)
              //free(setop->adds[idx].data);
            if(idx == 0)
              setop->adds = malloc(sizeof(ProtobufCBinaryData));
            else
              setop->adds = realloc(setop->adds, sizeof(ProtobufCBinaryData) * (idx+1));
            setop->n_adds = idx  + 1;
            adds = (char *)va_arg(args, char *);
            
            setop->adds[idx].data = NULL;
            setop->adds[idx].data = (unsigned char *)strdup(adds);
            setop->adds[idx].len = strlen(adds);
            break;
          
          case (RIACK_SETOP_FIELD_ADD):
            if (setop->adds)
              free(setop->adds[0].data);
            adds = (char *)va_arg(args, char *);
            if (adds)
              {
                setop->n_adds = 1;
                setop->adds = malloc(sizeof(ProtobufCBinaryData));
                setop->adds[0].data = (unsigned char *)strdup(adds);
                setop->adds[0].len = strlen(adds);
              }
            break;
          
          case (RIACK_SETOP_FIELD_REMOVE):
            if(setop->removes)
              free(setop->removes[0].data);
            removes = (char *)va_arg(args, char *);
            if (removes)
              {
                 setop->n_removes = 1;
                 setop->removes = malloc(sizeof(ProtobufCBinaryData));
                 setop->removes[0].data = (unsigned char *)strdup(removes);
                 setop->removes[0].len = strlen(removes);
              }
            break;
          }
        }
  if(flag == RIACK_SETOP_FIELD_NONE)
    return 0;
 
  return -errno;   
}
