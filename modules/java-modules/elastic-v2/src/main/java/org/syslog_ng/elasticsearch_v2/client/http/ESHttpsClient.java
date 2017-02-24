/*
 * Copyright (c) 2016 Balabit
 * Copyright (c) 2016 Laszlo Budai <laszlo.budai@balabit.com>
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

  @Override
  protected void setupHttpClientBuilder(HttpClientConfig.Builder httpClientConfigBuilder, ElasticSearchOptions options) {
		/* HTTPS */
			/* Initialize common objects */
			SSLContextBuilder sslContextBuilder = new SSLContextBuilder();
			SSLContext sslContext = null;
			SSLConnectionSocketFactory sslSocketFactory = null;
			SchemeIOSessionStrategy httpsIOSessionStrategy = null;
			HostnameVerifier hostnameVerifier = null;
			TrustStrategy trustStrategy  = null;
			KeyStore trustStore = null;
			/* Initialize trust strategy */
			// trust any server certificate
			// TODO: configuration option to control this?
			trustStrategy = new TrustStrategy() {
				public boolean isTrusted(X509Certificate[] chain, String authType) throws CertificateException {
					return true;
				}
			};
			if (options.getJavaSSLInsecure()) {
				logger.warn("Using insecure options for HTTPS client");
				// don't load trusted CA bundle
				trustStore = null;
				// skip hostname checks
				hostnameVerifier = NoopHostnameVerifier.INSTANCE;
			} else {
				// ensure server certificate matches its hostname
				// TODO: configuration option to control this?
				logger.warn("Using default hostname verifier for HTTPS client");
				hostnameVerifier = new DefaultHostnameVerifier();
				// load trust store CA bundle
				try {
					trustStore = KeyStore.getInstance(KeyStore.getDefaultType());
				} catch (Exception e) {
					logger.error("Error initializing truststore: " + e.getMessage());
				}
				try {
					trustStore.load(new FileInputStream(options.getJavaTrustStoreFilepath()), options.getJavaTrustStorePassword().toCharArray());
				} catch (IOException | NoSuchAlgorithmException | CertificateException e) {
					logger.error("Error loading truststore: " + e.getMessage());
				}
			}
			try {
				sslContextBuilder.loadTrustMaterial(trustStore, trustStrategy);
			} catch (NoSuchAlgorithmException | KeyStoreException e) {
				logger.error("Error loading truststore material: " + e.getMessage());
			}
			if (options.getHttpAuthType().equals("clientcert")) {
				KeyStore keyStore = null;
				try {
					// TODO: maybe we should support other filetypes
					keyStore = KeyStore.getInstance(KeyStore.getDefaultType());
				} catch (KeyStoreException e) {
					logger.error("Error initializing keystore: " + e.getMessage());
				}
				try {
					keyStore.load(new FileInputStream(options.getJavaKeyStoreFilepath()), options.getJavaKeyStorePassword().toCharArray());
				} catch (IOException | NoSuchAlgorithmException | CertificateException  e) {
					logger.error("Error loading keystore: " + e.getMessage());
				}

				// not sure why we need to give keystore password again
				try {
					sslContextBuilder.loadKeyMaterial(keyStore,options.getJavaKeyStorePassword().toCharArray());
				} catch (NoSuchAlgorithmException | KeyStoreException | UnrecoverableKeyException e) {
					logger.error("Error loading keystore material: " + e.getMessage());
				}
			}
			// Initialize ssl context
			try {
				sslContext = sslContextBuilder.build();
			} catch (Exception e) {
				logger.error("Error initializing SSL context: " + e.getMessage());
			}

			// configure httpclient to use the ssl context
			sslSocketFactory = new SSLConnectionSocketFactory(sslContext, hostnameVerifier);
			httpsIOSessionStrategy = new SSLIOSessionStrategy(sslContext, hostnameVerifier);
			httpClientConfigBuilder.sslSocketFactory(sslSocketFactory).httpsIOSessionStrategy(httpsIOSessionStrategy);
	}

}
