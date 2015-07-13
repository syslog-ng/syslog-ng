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

/** @file lib/riack/client.h
 */

/** @defgroup riack_client Riak Client
 *
 *
 * @addtogroup riack_client
 * @{
 */

#include <riack/message.h>
#include <riack/proto/riak_kv.pb-c.h>
#include <riack/proto/riak_dt.pb-c.h>



#ifndef __RIACK_CLIENT_H__
#define __RIACK_CLIENT_H__

typedef RpbPutReq riack_put_req_t;
typedef DtUpdateReq riack_dt_update_req_t;



/** Riak connect options.
 *
 * When using riak_client_connect(), optional settings can be
 * specified by listing a number of key-value pairs. These are the
 * supported keys, see each of them for more information about what
 * kind of values they expect.
 *
 * The list must be terminated with RIACK_CONNECT_OPTION_NONE.
 */
typedef enum
  {
    /** The terminating option.
     * This must be used as the last option passed to
     * riack_client_connect(), to terminate the list of additional
     * options.
     *
     * It takes no value.
     */
    RIACK_CONNECT_OPTION_NONE,

    /** The host to connect to.
     * By default, the library will try to connect to 127.0.0.1. To
     * change the host, use this option.
     *
     * The option takes a string (const char *).
     */
    RIACK_CONNECT_OPTION_HOST,

    /** The port to connect to.
     * By default, the library will use port 8087. To change this, use
     * this option.
     *
     * The option takes an int.
     */
    RIACK_CONNECT_OPTION_PORT,
  } riack_connect_option_t;
  
  
typedef enum
  {
    /** The client would use this option to send a RpbPutReq requst
     * To be used as a second argument in riack_client_send function
     */
    RIACK_MESSAGE_PUTREQ
   }riack_client_send_option_t;
   

    

#ifdef __cplusplus
extern "C" {
#endif

/** The Riak client object.
 *
 * This is an opaque class, the internal state of the Riak client.
 */
typedef struct _riack_client_t
{
int fd;
int conn;
} riack_client_t;



/** Return the library version.
 *
 * @returns the compiled-in library version as a string.
 */
const char *riack_version (void);
/** Return the library name and version.
 *
 * @returns the compiled-in library name and version, as a string.
 */
const char *riack_version_string (void);

/** Allocate a new, unconnected client.
 *
 * Use riack_client_connect() to connect the newly created client to a
 * server.
 *
 * @returns a newly allocated riack_client_t object, which must be
 * freed with riack_client_free() once no longer needed.
 */
riack_client_t *riack_client_new (void);

/** Free a Riak client object.
 *
 * Disconnects and frees up the supplied client.
 *
 * @param client is the client object to free up.
 *
 * @retval -EINVAL if client is NULL.
 * @retval -ENOTCONN if the client was not connected.
 * @retval 0 on success.
 *
 * @note The object will be freed even in case of non-fatal errors.
 */
int riack_client_free (riack_client_t *client);

/** Connect a client to a server.
 *
 * @param client is the client to connect.
 * @param ... are optional settings, see #riack_connect_option_t.
 *
 * @retval 0 is returned on success.
 * @retval -EINVAL is returned if any arguments are invalid.
 * @retval -errno is returned for any errors that may arise during the
 * connect aempt.
 */
int riack_client_connect (riack_client_t *client, ...);

/** Disconnect a client from the server.
 *
 * @param client is the client to disconnect.
 *
 * @retval 0 is returned on success.
 * @retval -ENOTCONN is returned if client is NULL or disconnected.
 * @retval -errno is returned if `close()` fails.
 */
int riack_client_disconnect (riack_client_t *client);

/** Send a request from the client 
 * @param client is the client connected to the riack node
 * @param option is the request you want to send through the client
 * @param putreq is a RpbPutReq structure filled by riack_req_put_set function.
 * 
 * @retval 0 is returned on success
 * @retval -errno for internal error
 */
int riack_client_send (riack_client_t *client, riack_message_t *message);
//send data to riak

int riack_client_recv(riack_client_t *client);
//receive data from riak


#ifdef __cplusplus
} /* extern "C" */
#endif

/** @} */

#endif
