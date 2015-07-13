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

/** @file lib/riack/content.h
 */

/** @defgroup riack_content Riak RpbContent
 *
 *
 * @addtogroup riack_content
 * @{
 */

#ifndef __RIACK_CONTENT_H__
#define __RIACK_CONTENT_H__

#include <riack/proto/riak_kv.pb-c.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef RpbContent riack_content_t;

typedef enum
  {
    RIACK_CONTENT_FIELD_NONE,
    RIACK_CONTENT_FIELD_VALUE,
    RIACK_CONTENT_FIELD_CONTENT_TYPE,
    RIACK_CONTENT_FIELD_CONTENT_ENCODING,
    RIACK_CONTENT_FIELD_CHARSET
  } riack_content_field_t;

riack_content_t *riack_content_new (void);
void riack_content_free (riack_content_t *content);

int riack_content_set (riack_content_t *content, ...);

#ifdef __cplusplus
} /* extern "C" */
#endif

/** @} */

#endif
