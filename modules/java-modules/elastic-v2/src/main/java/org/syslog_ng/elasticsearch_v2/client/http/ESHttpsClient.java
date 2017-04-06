/*
 * Copyright (c) 2017 Balabit
 * Copyright (c) 2017 Fabien Wernli <git@faxmodem.org>
 * Copyright (c) 2017 Laszlo Budai <laszlo.budai@balabit.com>
 *
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

package org.syslog_ng.elasticsearch_v2.client.http;

import org.syslog_ng.elasticsearch_v2.ElasticSearchOptions;
import io.searchbox.client.config.HttpClientConfig;
import javax.net.ssl.SSLContext;
import javax.net.ssl.HostnameVerifier;
import org.apache.http.conn.ssl.DefaultHostnameVerifier;
import org.apache.http.conn.ssl.NoopHostnameVerifier;
import org.apache.http.conn.ssl.TrustStrategy;
import org.apache.http.ssl.SSLContextBuilder;
import org.apache.http.conn.ssl.SSLConnectionSocketFactory;
import org.apache.http.nio.conn.SchemeIOSessionStrategy;
import org.apache.http.nio.conn.ssl.SSLIOSessionStrategy;
import java.security.cert.CertificateException;
import java.security.cert.X509Certificate;
import java.security.UnrecoverableKeyException;
import java.security.NoSuchAlgorithmException;
import java.security.KeyStore;
import java.security.KeyStoreException;
import java.io.FileInputStream;
import java.io.IOException;

public class ESHttpsClient extends ESHttpClient {
    public ESHttpsClient(ElasticSearchOptions options) {
        super(options);
    }

    private TrustStrategy trustStrategy;

    {
        trustStrategy = new TrustStrategy() {
            public boolean isTrusted(X509Certificate[] chain, String authType) throws CertificateException {
                return true;
            }
        };
    }

    private boolean isSSLInsecure(ElasticSearchOptions options) {
        return options.getJavaTrustStoreFilepath().isEmpty();
    }

    private HostnameVerifier getNoopHostnameVerifier() {
        return NoopHostnameVerifier.INSTANCE;
    }

    private HostnameVerifier getDefaultHostnameVerifier() {
        return new DefaultHostnameVerifier();
    }

    private KeyStore createKeyStore() {
        KeyStore keyStore;
        try {
            keyStore = KeyStore.getInstance(KeyStore.getDefaultType());
        } catch (KeyStoreException e) {
            throw new ESHttpsClient.HttpClientBuilderException("Error initializing keyStore", e);
        }
        return keyStore;
    }

    private void loadKeyStore(KeyStore keyStore, String path, String password) {
        try {
            keyStore.load(new FileInputStream(path), password.toCharArray());
        } catch (IOException | NoSuchAlgorithmException | CertificateException e) {
            throw new ESHttpClient.HttpClientBuilderException("Failed to load KeyStore", e);
        }
    }

    private KeyStore setupKeyStore(String path, String password) {
        KeyStore keyStore = createKeyStore();
        loadKeyStore(keyStore, path, password);
        return keyStore;
    }

    private void loadTrustMaterial(SSLContextBuilder sslContextBuilder, KeyStore trustStore) {
        try {
            sslContextBuilder.loadTrustMaterial(trustStore, trustStrategy);
        } catch (NoSuchAlgorithmException | KeyStoreException e) {
            throw new ESHttpClient.HttpClientBuilderException("Error loading truststore material", e);
        }
    }

    private void loadKeyMaterial(SSLContextBuilder sslContextBuilder, KeyStore keyStore, String password) {
        try {
            sslContextBuilder.loadKeyMaterial(keyStore, password.toCharArray());
        } catch (NoSuchAlgorithmException | KeyStoreException | UnrecoverableKeyException e) {
            throw new ESHttpClient.HttpClientBuilderException("Error loading keyStore material", e);
        }
    }

    private SSLContextBuilder setupSSLContextBuilder(ElasticSearchOptions options) {
        SSLContextBuilder sslContextBuilder = new SSLContextBuilder();
        KeyStore trustStore = null;

        if (isSSLInsecure(options)) {
            logger.warn("Using insecure options for HTTPS client");
        } else {
            trustStore = setupKeyStore(options.getJavaTrustStoreFilepath(), options.getJavaTrustStorePassword());
        }
        loadTrustMaterial(sslContextBuilder, trustStore);

        if (options.getHttpAuthType().equals("clientcert")) {
            KeyStore keyStore = setupKeyStore(options.getJavaKeyStoreFilepath(), options.getJavaKeyStorePassword());
            loadKeyMaterial(sslContextBuilder, keyStore, options.getJavaKeyStorePassword());
        }
        return sslContextBuilder;
    }

    private HostnameVerifier setupHostnameVerifier(ElasticSearchOptions options) {
        if (isSSLInsecure(options)) {
            return NoopHostnameVerifier.INSTANCE;
        } else {
            return new DefaultHostnameVerifier();
        }
    }

    private SSLContext buildSSLContext(SSLContextBuilder sslContextBuilder) {
        SSLContext sslContext;
        try {
            sslContext = sslContextBuilder.build();
        } catch (Exception e) {
            throw new ESHttpClient.HttpClientBuilderException("Error initializing SSL context",e);
        }
        return sslContext;
    }

    @Override
    protected void setupHttpClientBuilder(HttpClientConfig.Builder httpClientConfigBuilder, ElasticSearchOptions options) {
        SSLContextBuilder sslContextBuilder = setupSSLContextBuilder(options);
        SSLContext sslContext = buildSSLContext(sslContextBuilder);
        HostnameVerifier hostnameVerifier = setupHostnameVerifier(options);
        SSLConnectionSocketFactory sslSocketFactory = new SSLConnectionSocketFactory(sslContext, hostnameVerifier);
        SchemeIOSessionStrategy httpsIOSessionStrategy = new SSLIOSessionStrategy(sslContext, hostnameVerifier);

        httpClientConfigBuilder.sslSocketFactory(sslSocketFactory).httpsIOSessionStrategy(httpsIOSessionStrategy);
    }

}
