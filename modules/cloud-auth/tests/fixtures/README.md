# Cloud-auth test fixtures

These self-signed RSA certificate and private keys are **public test data only**.
They identify no real application or tenant. Never deploy, trust, or register them
with Azure or any other service. The encrypted key's public test passphrase is
`test-only-passphrase`.

The certificate has a deliberately long validity period; tests do not depend on
certificate expiration or contact a network endpoint. Its SHA-256 DER thumbprint
is `73DUJ-amUKdBFa75-WkqLKYH1lv5x36Tkn4LHao_E98` (unpadded base64url).
The expected thumbprint is fixed independently of the implementation under test.

To regenerate from the repository root using OpenSSL:

```sh
openssl req -x509 -newkey rsa:2048 -sha256 -nodes \
  -keyout modules/cloud-auth/tests/fixtures/test-only-private-key.pem \
  -out modules/cloud-auth/tests/fixtures/test-only-certificate.pem \
  -days 36500 -subj '/CN=syslog-ng cloud-auth TEST ONLY - never use in production'
openssl pkey -in modules/cloud-auth/tests/fixtures/test-only-private-key.pem \
  -aes-256-cbc -passout pass:test-only-passphrase \
  -out modules/cloud-auth/tests/fixtures/test-only-encrypted-private-key.pem
openssl x509 -in modules/cloud-auth/tests/fixtures/test-only-certificate.pem \
  -noout -fingerprint -sha256
```

Regeneration changes the thumbprint; update the expected value in
`test-cloud-auth.cpp` and this document accordingly.

Run the network-free Criterion tests in an already configured build:

```sh
# CMake (BUILD_TESTING and ENABLE_CLOUD_AUTH enabled)
cmake --build build --target test_cloud_auth
ctest --test-dir build -R '^test_cloud_auth$' --output-on-failure
# Autotools (cloud-auth and Criterion enabled)
make check check_PROGRAMS=modules/cloud-auth/tests/test_cloud_auth \
  TESTS=modules/cloud-auth/tests/test_cloud_auth
```
