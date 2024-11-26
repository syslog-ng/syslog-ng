/*
 * Copyright (c) 2024 One Identity LLC.
 * Copyright (c) 2024 Franco Fichtner
 * Copyright (c) 2002-2011 Balabit
 * Copyright (c) 1998-2011 Balázs Scheidler
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 */
#include "tls-verifier.h"
#include "messages.h"
#include "compat/openssl_support.h"
#include <openssl/x509v3.h>

/* TLSVerifier */

TLSVerifier *
tls_verifier_new(TLSSessionVerifyFunc verify_func, gpointer verify_data,
                 GDestroyNotify verify_data_destroy)
{
  TLSVerifier *self = g_new0(TLSVerifier, 1);

  g_atomic_counter_set(&self->ref_cnt, 1);
  self->verify_func = verify_func;
  self->verify_data = verify_data;
  self->verify_data_destroy = verify_data_destroy;
  return self;
}

TLSVerifier *
tls_verifier_ref(TLSVerifier *self)
{
  g_assert(!self || g_atomic_counter_get(&self->ref_cnt) > 0);

  if (self)
    g_atomic_counter_inc(&self->ref_cnt);

  return self;
}

static void
_tls_verifier_free(TLSVerifier *self)
{
  g_assert(self);

  if (self)
    {
      if (self->verify_data && self->verify_data_destroy)
        self->verify_data_destroy(self->verify_data);
      g_free(self);
    }
}

void
tls_verifier_unref(TLSVerifier *self)
{
  g_assert(!self || g_atomic_counter_get(&self->ref_cnt));

  if (self && (g_atomic_counter_dec_and_test(&self->ref_cnt)))
    _tls_verifier_free(self);
}

/* helper functions */

gboolean
tls_wildcard_match(const gchar *host_name, const gchar *pattern)
{
  gchar **pattern_parts, **hostname_parts;
  gboolean success = FALSE;
  gchar *lower_pattern = NULL;
  gchar *lower_hostname = NULL;
  gint i;

  pattern_parts = g_strsplit(pattern, ".", 0);
  hostname_parts = g_strsplit(host_name, ".", 0);
  if (pattern_parts[0] == NULL)
    {
      if (hostname_parts[0] == NULL)
        success = TRUE;
      else
        success = FALSE;
    }
  else
    {
      success = TRUE;
      for (i = 0; pattern_parts[i]; i++)
        {
          if (hostname_parts[i] == NULL)
            {
              /* number of dot separated entries is not the same in the hostname and the pattern spec */
              success = FALSE;
              break;
            }
          if (g_strrstr(pattern_parts[i], "?"))
            {
              /* Glib would treat any question marks as jokers */
              success = FALSE;
              break;
            }
          char *wildcard_matched = g_strrstr(pattern_parts[i], "*");
          if (wildcard_matched && (i != 0 || wildcard_matched != strstr(pattern_parts[i], "*")))
            {
              /* wildcard only on leftmost part and never as multiple wildcards as per both RFC 6125 and 9525 */
              success = FALSE;
              break;
            }

          lower_pattern = g_ascii_strdown(pattern_parts[i], -1);
          lower_hostname = g_ascii_strdown(hostname_parts[i], -1);

          if (!g_pattern_match_simple(lower_pattern, lower_hostname))
            {
              success = FALSE;
              break;
            }
        }
      if (hostname_parts[i])
        /* hostname has more parts than the pattern */
        success = FALSE;
    }

  g_free(lower_pattern);
  g_free(lower_hostname);
  g_strfreev(pattern_parts);
  g_strfreev(hostname_parts);
  return success;
}

gboolean
tls_verify_certificate_name(X509 *cert, const gchar *host_name)
{
  gchar pattern_buf[256];
  gint ext_ndx;
  gboolean found = FALSE, result = FALSE;

  ext_ndx = X509_get_ext_by_NID(cert, NID_subject_alt_name, -1);
  if (ext_ndx >= 0)
    {
      /* ok, there's a subjectAltName extension, check that */
      X509_EXTENSION *ext;
      STACK_OF(GENERAL_NAME) *alt_names;
      GENERAL_NAME *gen_name;

      ext = X509_get_ext(cert, ext_ndx);
      alt_names = X509V3_EXT_d2i(ext);
      if (alt_names)
        {
          gint num, i;

          num = sk_GENERAL_NAME_num(alt_names);

          for (i = 0; !result && i < num; i++)
            {
              gen_name = sk_GENERAL_NAME_value(alt_names, i);
              if (gen_name->type == GEN_DNS)
                {
                  const guchar *dnsname = ASN1_STRING_get0_data(gen_name->d.dNSName);
                  guint dnsname_len = ASN1_STRING_length(gen_name->d.dNSName);

                  if (dnsname_len > sizeof(pattern_buf) - 1)
                    {
                      found = TRUE;
                      result = FALSE;
                      break;
                    }

                  memcpy(pattern_buf, dnsname, dnsname_len);
                  pattern_buf[dnsname_len] = 0;
                  /* we have found a DNS name as alternative subject name */
                  found = TRUE;
                  result = tls_wildcard_match(host_name, pattern_buf);
                }
              else if (gen_name->type == GEN_IPADD)
                {
                  gchar dotted_ip[64] = {0};
                  int addr_family = AF_INET;
                  if (gen_name->d.iPAddress->length == 16)
                    addr_family = AF_INET6;

                  if (inet_ntop(addr_family, gen_name->d.iPAddress->data, dotted_ip, sizeof(dotted_ip)))
                    {
                      g_strlcpy(pattern_buf, dotted_ip, sizeof(pattern_buf));
                      found = TRUE;
                      result = strcasecmp(host_name, pattern_buf) == 0;
                    }
                }
            }
          sk_GENERAL_NAME_free(alt_names);
        }
    }
  if (!found)
    {
      /* hmm. there was no subjectAltName (this is deprecated, but still
       * widely used), look up the Subject, most specific CN */
      X509_NAME *name;

      name = X509_get_subject_name(cert);
      if (X509_NAME_get_text_by_NID(name, NID_commonName, pattern_buf, sizeof(pattern_buf)) != -1)
        {
          result = tls_wildcard_match(host_name, pattern_buf);
        }
    }
  if (!result)
    {
      msg_error("Certificate subject does not match configured hostname",
                evt_tag_str("hostname", host_name),
                evt_tag_str("certificate", pattern_buf));
    }
  else
    {
      msg_verbose("Certificate subject matches configured hostname",
                  evt_tag_str("hostname", host_name),
                  evt_tag_str("certificate", pattern_buf));
    }

  return result;
}
