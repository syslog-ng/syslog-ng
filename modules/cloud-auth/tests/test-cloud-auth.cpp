/*
 * Copyright (c) 2026 syslog-ng contributors
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
 */

#include "azure-auth.hpp"

#include "compat/cpp-start.h"
#include "apphook.h"
#include "compat/cpp-end.h"

#include <criterion/criterion.h>
#include <criterion/parameterized.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/x509.h>

#include <map>

#define FIXTURE_DIR TOP_SRCDIR "/modules/cloud-auth/tests/fixtures/"

static const char *certificate_path = FIXTURE_DIR "test-only-certificate.pem";
static const char *private_key_path = FIXTURE_DIR "test-only-private-key.pem";
static const char *encrypted_private_key_path = FIXTURE_DIR "test-only-encrypted-private-key.pem";
static const char *missing_path = FIXTURE_DIR "does-not-exist.pem";
static const char *token_url = "https://login.microsoftonline.com/test-tenant/oauth2/v2.0/token";
static const char *scope = "https://monitor.azure.com//.default";

using syslogng::cloud_auth::azure::AzureMonitorAuthenticator;
using syslogng::cloud_auth::oauth2::OAuth2Authenticator;

/*
 * Criterion runs each parameterized case in a separate process and copies
 * the parameter array by value. Pointers to the parent's static strings are
 * not valid there (e.g. on macOS), so parameters identify fixtures with an
 * enum that is resolved inside the test process.
 */
typedef enum
{
  FX_NULL,
  FX_EMPTY,
  FX_CERT,
  FX_KEY,
  FX_ENCRYPTED_KEY,
  FX_MISSING,
  FX_SECRET,
  FX_WRONG_PASSPHRASE,
} Fixture;

static const char *
_fixture(Fixture fixture)
{
  switch (fixture)
    {
    case FX_NULL:
      return nullptr;
    case FX_EMPTY:
      return "";
    case FX_CERT:
      return certificate_path;
    case FX_KEY:
      return private_key_path;
    case FX_ENCRYPTED_KEY:
      return encrypted_private_key_path;
    case FX_MISSING:
      return missing_path;
    case FX_SECRET:
      return "secret";
    case FX_WRONG_PASSPHRASE:
      return "wrong-passphrase";
    default:
      g_assert_not_reached();
    }
}

/* Exercise credential construction without performing an HTTP request. */
class TestAzureAuthenticator : public AzureMonitorAuthenticator
{
public:
  using AzureMonitorAuthenticator::AzureMonitorAuthenticator;
  using OAuth2Authenticator::build_client_assertion_jwt;
  using OAuth2Authenticator::build_post_body;
  using OAuth2Authenticator::prepare_auth_credentials;
};

class TestOAuth2Authenticator : public OAuth2Authenticator
{
public:
  using OAuth2Authenticator::OAuth2Authenticator;
  using OAuth2Authenticator::build_client_assertion_jwt;
  using OAuth2Authenticator::build_post_body;
  using OAuth2Authenticator::prepare_auth_credentials;
};

static void
_setup(void)
{
  app_startup();
  cr_assert_eq(curl_global_init(CURL_GLOBAL_DEFAULT), CURLE_OK);
}

static void
_teardown(void)
{
  curl_global_cleanup();
  app_shutdown();
}

TestSuite(cloud_auth, .init = _setup, .fini = _teardown);

static std::map<std::string, std::string>
_decode_post_body(const std::string &body)
{
  std::map<std::string, std::string> fields;
  CURL *hnd = curl_easy_init();
  cr_assert_not_null(hnd);
  size_t start = 0;
  while (start < body.size())
    {
      size_t end = body.find('&', start);
      std::string field = body.substr(start, end == std::string::npos ? end : end - start);
      size_t separator = field.find('=');
      cr_assert_neq(separator, std::string::npos);
      std::string name = field.substr(0, separator);
      std::string encoded = field.substr(separator + 1);
      int length = 0;
      char *decoded = curl_easy_unescape(hnd, encoded.c_str(), encoded.size(), &length);
      cr_assert_not_null(decoded);
      cr_assert(fields.emplace(name, std::string(decoded, length)).second, "Duplicate POST field: %s", name.c_str());
      curl_free(decoded);
      if (end == std::string::npos)
        break;
      start = end + 1;
    }
  curl_easy_cleanup(hnd);
  return fields;
}

template<typename AuthenticatorT>
static std::map<std::string, std::string>
_prepare_post_body(AuthenticatorT &authenticator)
{
  std::string body = authenticator.build_post_body();
  CURL *hnd = curl_easy_init();
  cr_assert_not_null(hnd);
  authenticator.prepare_auth_credentials(hnd, body);
  curl_easy_cleanup(hnd);
  return _decode_post_body(body);
}

static bool
_verify_signature(const std::string &token)
{
  /* Verify with the certificate public key, independently of jwt-cpp's signer. */
  BIO *bio = BIO_new_file(certificate_path, "r");
  cr_assert_not_null(bio);
  X509 *cert = PEM_read_bio_X509(bio, nullptr, nullptr, nullptr);
  BIO_free(bio);
  cr_assert_not_null(cert);
  EVP_PKEY *key = X509_get_pubkey(cert);
  X509_free(cert);
  cr_assert_not_null(key);
  EVP_MD_CTX *ctx = EVP_MD_CTX_new();
  cr_assert_not_null(ctx);
  cr_assert_eq(EVP_DigestVerifyInit(ctx, nullptr, EVP_sha256(), nullptr, key), 1);
  size_t separator = token.rfind('.');
  cr_assert_neq(separator, std::string::npos);
  std::string signature = jwt::decode(token).get_signature();
  int result = EVP_DigestVerify(ctx, reinterpret_cast<const unsigned char *>(signature.data()), signature.size(),
                                reinterpret_cast<const unsigned char *>(token.data()), separator);
  EVP_MD_CTX_free(ctx);
  EVP_PKEY_free(key);
  return result == 1;
}

Test(cloud_auth, certificate_assertion_claims_thumbprint_and_signature)
{
  TestAzureAuthenticator authenticator("test-tenant", "test-app", nullptr, scope, certificate_path, private_key_path);
  auto before = std::chrono::system_clock::now();
  std::string token = authenticator.build_client_assertion_jwt();
  auto after = std::chrono::system_clock::now();
  cr_assert_not(token.empty());
  auto decoded = jwt::decode(token);
  cr_assert(decoded.get_algorithm() == "RS256");
  cr_assert(decoded.get_header_claim("x5t#S256").as_string() ==
            "73DUJ-amUKdBFa75-WkqLKYH1lv5x36Tkn4LHao_E98");
  cr_assert(decoded.get_issuer() == "test-app");
  cr_assert(decoded.get_subject() == "test-app");
  cr_assert_eq(decoded.get_audience().size(), 1);
  cr_assert_eq(decoded.get_audience().count(token_url), 1);
  cr_assert(decoded.get_issued_at() >= before - std::chrono::seconds(1));
  cr_assert(decoded.get_issued_at() <= after);
  cr_assert(decoded.get_not_before() == decoded.get_issued_at());
  cr_assert(decoded.get_expires_at() - decoded.get_issued_at() == std::chrono::minutes(5));
  cr_assert_not(decoded.get_id().empty());
  cr_assert(decoded.get_id() != jwt::decode(authenticator.build_client_assertion_jwt()).get_id());
  cr_assert(_verify_signature(token));
  std::string tampered = token;
  size_t signature_start = token.rfind('.') + 1;
  tampered[signature_start] = tampered[signature_start] == 'a' ? 'b' : 'a';
  cr_assert_not(_verify_signature(tampered));
}

Test(cloud_auth, certificate_post_credentials)
{
  TestAzureAuthenticator authenticator("test-tenant", "app +&=%", nullptr, scope, certificate_path, private_key_path);
  auto fields = _prepare_post_body(authenticator);
  cr_assert_eq(fields.size(), 5);
  cr_assert_str_eq(fields.at("grant_type").c_str(), "client_credentials");
  cr_assert_str_eq(fields.at("scope").c_str(), scope);
  cr_assert_str_eq(fields.at("client_id").c_str(), "app +&=%");
  cr_assert_str_eq(fields.at("client_assertion_type").c_str(), "urn:ietf:params:oauth:client-assertion-type:jwt-bearer");
  cr_assert_eq(fields.count("client_secret"), 0);
  cr_assert(_verify_signature(fields.at("client_assertion")));
  auto decoded = jwt::decode(fields.at("client_assertion"));
  cr_assert(decoded.get_issuer() == fields.at("client_id"));
  cr_assert(decoded.get_subject() == fields.at("client_id"));
}

Test(cloud_auth, app_secret_post_credentials_are_preserved)
{
  TestAzureAuthenticator authenticator("test-tenant", "app +&=%", "secret +&=%", scope);
  auto fields = _prepare_post_body(authenticator);
  cr_assert_eq(fields.size(), 4);
  cr_assert_str_eq(fields.at("grant_type").c_str(), "client_credentials");
  cr_assert_str_eq(fields.at("scope").c_str(), scope);
  cr_assert_str_eq(fields.at("client_id").c_str(), "app +&=%");
  cr_assert_str_eq(fields.at("client_secret").c_str(), "secret +&=%");
  cr_assert_eq(fields.count("client_assertion"), 0);
  cr_assert_eq(fields.count("client_assertion_type"), 0);
}

Test(cloud_auth, encrypted_private_key_with_passphrase)
{
  TestAzureAuthenticator authenticator("test-tenant", "test-app", nullptr, scope, certificate_path,
                                       encrypted_private_key_path, "test-only-passphrase");
  std::string token = authenticator.build_client_assertion_jwt();
  cr_assert_not(token.empty());
  cr_assert(_verify_signature(token));
}

struct InvalidAssertion
{
  Fixture cert;
  Fixture key;
  Fixture passphrase;
};

ParameterizedTestParameters(cloud_auth, invalid_assertion_credentials)
{
  static InvalidAssertion cases[] =
  {
    {FX_CERT, FX_MISSING, FX_NULL},
    {FX_MISSING, FX_KEY, FX_NULL},
    {FX_KEY, FX_KEY, FX_NULL},
    {FX_CERT, FX_CERT, FX_NULL},
    {FX_CERT, FX_ENCRYPTED_KEY, FX_WRONG_PASSPHRASE},
  };
  return cr_make_param_array(InvalidAssertion, cases, G_N_ELEMENTS(cases));
}

ParameterizedTest(InvalidAssertion *test_case, cloud_auth, invalid_assertion_credentials)
{
  TestAzureAuthenticator authenticator("test-tenant", "test-app", nullptr, scope, _fixture(test_case->cert),
                                       _fixture(test_case->key), _fixture(test_case->passphrase));
  cr_assert(authenticator.build_client_assertion_jwt().empty());
}

struct AzureConfiguration
{
  Fixture secret;
  Fixture cert;
  Fixture key;
  gboolean valid;
};

ParameterizedTestParameters(cloud_auth, azure_credential_configuration)
{
  static AzureConfiguration cases[] =
  {
    {FX_NULL, FX_NULL, FX_NULL, FALSE},
    {FX_EMPTY, FX_EMPTY, FX_EMPTY, FALSE},
    {FX_NULL, FX_CERT, FX_NULL, FALSE},
    {FX_NULL, FX_NULL, FX_KEY, FALSE},
    {FX_SECRET, FX_CERT, FX_NULL, FALSE},
    {FX_SECRET, FX_NULL, FX_KEY, FALSE},
    {FX_SECRET, FX_CERT, FX_KEY, FALSE},
    {FX_SECRET, FX_NULL, FX_NULL, TRUE},
    {FX_SECRET, FX_EMPTY, FX_EMPTY, TRUE},
    {FX_NULL, FX_CERT, FX_KEY, TRUE},
    {FX_EMPTY, FX_CERT, FX_KEY, TRUE},
  };
  return cr_make_param_array(AzureConfiguration, cases, G_N_ELEMENTS(cases));
}

ParameterizedTest(AzureConfiguration *test_case, cloud_auth, azure_credential_configuration)
{
  CloudAuthenticator *authenticator = azure_authenticator_new();
  azure_authenticator_set_auth_mode(authenticator, AAAM_MONITOR);
  azure_authenticator_set_tenant_id(authenticator, "test-tenant");
  azure_authenticator_set_app_id(authenticator, "test-app");
  azure_authenticator_set_app_secret(authenticator, _fixture(test_case->secret));
  azure_authenticator_set_cert_path(authenticator, _fixture(test_case->cert));
  azure_authenticator_set_private_key_path(authenticator, _fixture(test_case->key));
  cr_assert_eq(cloud_authenticator_init(authenticator), test_case->valid);
  if (test_case->valid)
    cloud_authenticator_deinit(authenticator);
  cloud_authenticator_free(authenticator);
}

Test(cloud_auth, azure_requires_auth_mode)
{
  CloudAuthenticator *authenticator = azure_authenticator_new();
  azure_authenticator_set_tenant_id(authenticator, "test-tenant");
  azure_authenticator_set_app_id(authenticator, "test-app");
  azure_authenticator_set_app_secret(authenticator, "secret");
  cr_assert_not(cloud_authenticator_init(authenticator));
  cloud_authenticator_free(authenticator);
}

Test(cloud_auth, oauth2_private_key_jwt_post_credentials)
{
  const char *generic_token_url = "https://idp.example.com/oauth2/token";
  TestOAuth2Authenticator authenticator("generic-client", nullptr, generic_token_url, "api.read", nullptr, nullptr, 60,
                                        OAUTH2_AUTH_METHOD_PRIVATE_KEY_JWT, certificate_path, private_key_path);
  auto fields = _prepare_post_body(authenticator);
  cr_assert_eq(fields.size(), 5);
  cr_assert_str_eq(fields.at("grant_type").c_str(), "client_credentials");
  cr_assert_str_eq(fields.at("scope").c_str(), "api.read");
  cr_assert_str_eq(fields.at("client_id").c_str(), "generic-client");
  cr_assert_str_eq(fields.at("client_assertion_type").c_str(), "urn:ietf:params:oauth:client-assertion-type:jwt-bearer");
  cr_assert_eq(fields.count("client_secret"), 0);
  cr_assert(_verify_signature(fields.at("client_assertion")));
  auto decoded = jwt::decode(fields.at("client_assertion"));
  cr_assert(decoded.get_issuer() == "generic-client");
  cr_assert_eq(decoded.get_audience().count(generic_token_url), 1);
}

struct OAuth2Configuration
{
  OAuth2AuthMethod auth_method;
  Fixture cert;
  Fixture key;
  gboolean valid;
};

ParameterizedTestParameters(cloud_auth, oauth2_client_assertion_configuration)
{
  static OAuth2Configuration cases[] =
  {
    {OAUTH2_AUTH_METHOD_PRIVATE_KEY_JWT, FX_NULL, FX_NULL, FALSE},
    {OAUTH2_AUTH_METHOD_PRIVATE_KEY_JWT, FX_EMPTY, FX_EMPTY, FALSE},
    {OAUTH2_AUTH_METHOD_PRIVATE_KEY_JWT, FX_CERT, FX_NULL, FALSE},
    {OAUTH2_AUTH_METHOD_PRIVATE_KEY_JWT, FX_NULL, FX_KEY, FALSE},
    {OAUTH2_AUTH_METHOD_BASIC, FX_CERT, FX_KEY, FALSE},
    {OAUTH2_AUTH_METHOD_POST_BODY, FX_CERT, FX_NULL, FALSE},
    {OAUTH2_AUTH_METHOD_POST_BODY, FX_NULL, FX_KEY, FALSE},
    {OAUTH2_AUTH_METHOD_PRIVATE_KEY_JWT, FX_CERT, FX_KEY, TRUE},
    {OAUTH2_AUTH_METHOD_BASIC, FX_NULL, FX_NULL, TRUE},
    {OAUTH2_AUTH_METHOD_POST_BODY, FX_EMPTY, FX_EMPTY, TRUE},
  };
  return cr_make_param_array(OAuth2Configuration, cases, G_N_ELEMENTS(cases));
}

ParameterizedTest(OAuth2Configuration *test_case, cloud_auth, oauth2_client_assertion_configuration)
{
  CloudAuthenticator *authenticator = oauth2_authenticator_new();
  oauth2_authenticator_set_client_id(authenticator, "generic-client");
  oauth2_authenticator_set_client_secret(authenticator, "secret");
  oauth2_authenticator_set_token_url(authenticator, "https://idp.example.com/oauth2/token");
  oauth2_authenticator_set_auth_method(authenticator, test_case->auth_method);
  oauth2_authenticator_set_cert_path(authenticator, _fixture(test_case->cert));
  oauth2_authenticator_set_private_key_path(authenticator, _fixture(test_case->key));
  cr_assert_eq(cloud_authenticator_init(authenticator), test_case->valid);
  if (test_case->valid)
    cloud_authenticator_deinit(authenticator);
  cloud_authenticator_free(authenticator);
}
