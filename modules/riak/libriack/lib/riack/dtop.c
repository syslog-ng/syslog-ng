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

#include <riack/dtop.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>

riack_dt_op_t *
riack_dt_op_new (void)
{
  riack_dt_op_t *dtop = (riack_dt_op_t*)malloc(sizeof (riack_dt_op_t));
  dt_op__init(dtop);
  return dtop;
}


void 
riack_dt_op_free (riack_dt_op_t *dtop)
{
  dt_op__free_unpacked(dtop, NULL);
}


int 
riack_dt_op_set (riack_dt_op_t *dtop, ...)
{
  va_list args;
  int flag;
  if (dtop == NULL)
    return -EINVAL;
  riack_setop_t *setop;
  
  va_start(args, dtop);
  while ((flag = va_arg(args, int)) != 0)
  {
    
    switch (flag) {
      case(-1) :
        break;
    case (RIACK_DT_OP_FIELD_SETOP):
      setop = (riack_setop_t*)va_arg(args, riack_setop_t *);
      dtop->set_op =setop;
      break; 
    }
  }
  if(flag == RIACK_DT_OP_FIELD_NONE)
    return 0;
  
  return -errno; 
}
