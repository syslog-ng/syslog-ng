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

#include <riack/content.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>


riack_content_t *
riack_content_new (void)
{

  riack_content_t *content = (riack_content_t*)malloc(sizeof(riack_content_t));
  rpb_content__init(content);
  return content;
}

void
riack_content_free (riack_content_t *content)
{

  rpb_content__free_unpacked(content,NULL);
}

int
riack_content_set (riack_content_t *content, ...)
{
  va_list args;
  if (content == NULL)
    return -EINVAL;
  int flag; 
  char *val;
  char *cont_type;
  char *cont_encod;
  char *charset;
  int length;
  
  va_start(args, content);
  while ((flag = va_arg(args, int)) != 0)
    {
      switch (flag)
        {
          case (-1):
            break;
    
          case (RIACK_CONTENT_FIELD_VALUE):
            val = (char *)va_arg(args, char *);
            if ((val != NULL) && ((length = va_arg(args, int)) > 0))
              {
                char sub_val[length+1];
                strncpy(sub_val, val, length);
                sub_val[length] ='\0';
                if (content->value.data)
                  free(content->value.data);
                content->value.data = (unsigned char *)strdup(sub_val);
                content->value.len = length;
              }
            else if (val == NULL)
              {
                if (content->value.data)
                  free(content->value.data);
                content->value.data = NULL;
                content->value.len = 0;
              }
            else
              {
                if (content->value.data)
                  free(content->value.data);
                content->value.data = (unsigned char *)strdup(val);
                content->value.len = strlen(val);
              }
            break;
            
          case (RIACK_CONTENT_FIELD_CONTENT_TYPE):
            if (content->content_type.data)
              free(content->content_type.data);
            cont_type = (char *)va_arg(args, char *);
            if (cont_type)
              {
                content->has_content_type = 1;
                content->content_type.data = (unsigned char *)strdup(cont_type);
                content->content_type.len = strlen(cont_type);
              }
            else 
              {
                content->content_type.data = NULL;
                content->content_type.len = 0;
                content->has_content_type = 0;
              }
        
            break;
            
          case (RIACK_CONTENT_FIELD_CONTENT_ENCODING):
            if (content->content_encoding.data)
              free(content->content_encoding.data);
            cont_encod = (char *)va_arg(args, char *);
            content->content_encoding.data = (unsigned char *)strdup(cont_encod);
            content->content_encoding.len = strlen(cont_encod);
            break;
            
          case (RIACK_CONTENT_FIELD_CHARSET):
            if (content->charset.data)
              free(content->charset.data);
            charset = (char *)va_arg(args, char *);
            if (charset)
              {
                content->has_charset = 1;
                content->charset.data = (unsigned char *)strdup(charset);
                content->charset.len = strlen(charset);
              }
            else
              {
                content->has_charset = 0;
                content->charset.data = NULL;
                content->charset.len = 0;
              }
            break;
        }
    }
  
  if(flag == RIACK_CONTENT_FIELD_NONE)
    return 0;
     
  return -errno;
}
