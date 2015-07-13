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

/** @file lib/riack/message.h
 */

#include <stdint.h>
#include <stddef.h>

/** @defgroup riack_putreq Riak riack_message
 *
 *
 * @addtogroup riack_message
 * @{
 */

#ifndef __RIACK_MESSAGE_H__
#define __RIACK_MESSAGE_H__


#ifdef __cplusplus
extern "C" {
#endif


typedef struct 
{ 
  uint32_t length;
  uint8_t message_code; 
  uint8_t data[0]; 
} riack_message_t;


#ifdef __cplusplus
} /* extern "C" */
#endif

/** @} */

#endif
