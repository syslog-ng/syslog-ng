/*
 * Copyright (c) 2025 Axoflow
 * Copyright (c) 2025 Tamas Kosztyu <tamas.kosztyu@axoflow.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 */

#ifndef AZURE_AUTH_HPP
#define AZURE_AUTH_HPP

#include "azure-auth.h"
#include "cloud-auth.hpp"

namespace syslogng {
namespace cloud_auth {
namespace azure {

class AzureMonitorAuthenticator: public syslogng::cloud_auth::Authenticator
{
public:
  AzureMonitorAuthenticator();
  ~AzureMonitorAuthenticator() {};

  void handle_http_header_request(HttpHeaderRequestSignalData *data);
};

}
}
}

#endif
