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

import io.searchbox.client.JestClient;
import io.searchbox.client.JestClientFactory;
import io.searchbox.client.JestResult;
import io.searchbox.client.config.HttpClientConfig;
import io.searchbox.cluster.NodesInfo;
import org.apache.log4j.Logger;
import org.syslog_ng.elasticsearch_v2.ElasticSearchOptions;
import org.syslog_ng.elasticsearch_v2.client.ESClient;
import org.syslog_ng.elasticsearch_v2.messageprocessor.ESIndex;
import org.syslog_ng.elasticsearch_v2.messageprocessor.ESMessageProcessorFactory;
import org.syslog_ng.elasticsearch_v2.messageprocessor.http.HttpMessageProcessor;
import java.util.Set;
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

public class ESHttpClient implements ESClient {
	private ElasticSearchOptions options;
	private String authtype;
	private JestClient client;
	private HttpMessageProcessor messageProcessor;
	private Logger logger;

	public ESHttpClient(ElasticSearchOptions options) {
		this.options = options;
		logger = Logger.getRootLogger();
		messageProcessor = ESMessageProcessorFactory.getMessageProcessor(options, this);
	}

	private JestClient createClient() {
		Set<String> connectionUrls = options.getClusterUrls();
		JestClientFactory clientFactory = new JestClientFactory();
		/* instanciate the HttpClientConfig builder with common options */
		HttpClientConfig.Builder httpClientConfigBuilder = new HttpClientConfig
			.Builder(connectionUrls)
			.multiThreaded(true)
			.defaultSchemeForDiscoveredNodes(options.getClientMode());

		/* HTTP Basic authentication requested */
		if (options.getHttpAuthType().equals("basic")) {
			httpClientConfigBuilder.defaultCredentials(options.getHttpAuthTypeBasicUsername(), options.getHttpAuthTypeBasicPassword());
		}
		/* HTTPS */
		if (options.getClientMode().equals("https")) {
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
				public boolean isTrusted(X509Certificate[] arg0, String arg1) throws CertificateException {
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
					return null;
				}
				try {
					trustStore.load(new FileInputStream(options.getJavaTrustStoreFilepath()), options.getJavaTrustStorePassword().toCharArray());
				} catch (IOException | NoSuchAlgorithmException | CertificateException e) {
					logger.error("Error loading truststore: " + e.getMessage());
					return null;
				}
			}
			try {
				sslContextBuilder.loadTrustMaterial(trustStore, trustStrategy);
			} catch (NoSuchAlgorithmException | KeyStoreException e) {
				logger.error("Error loading truststore material: " + e.getMessage());
				return null;
			}
			if (options.getHttpAuthType().equals("clientcert")) {
				KeyStore keyStore = null;
				try {
					// TODO: maybe we should support other filetypes
					keyStore = KeyStore.getInstance(KeyStore.getDefaultType());
				} catch (KeyStoreException e) {
					logger.error("Error initializing keystore: " + e.getMessage());
					return null;
				}
				try {
					keyStore.load(new FileInputStream(options.getJavaKeyStoreFilepath()), options.getJavaKeyStorePassword().toCharArray());
				} catch (IOException | NoSuchAlgorithmException | CertificateException  e) {
					logger.error("Error loading keystore: " + e.getMessage());
					return null;
				}

				// not sure why we need to give keystore password again
				try {
					sslContextBuilder.loadKeyMaterial(keyStore,options.getJavaKeyStorePassword().toCharArray());
				} catch (NoSuchAlgorithmException | KeyStoreException | UnrecoverableKeyException e) {
					logger.error("Error loading keystore material: " + e.getMessage());
					return null;
				}
			}
			// Initialize ssl context
			try {
				sslContext = sslContextBuilder.build();
			} catch (Exception e) {
				logger.error("Error initializing SSL context: " + e.getMessage());
				return null;
			}

			// configure httpclient to use the ssl context
			sslSocketFactory = new SSLConnectionSocketFactory(sslContext, hostnameVerifier);
			httpsIOSessionStrategy = new SSLIOSessionStrategy(sslContext, hostnameVerifier);
			httpClientConfigBuilder.sslSocketFactory(sslSocketFactory).httpsIOSessionStrategy(httpsIOSessionStrategy);
		}

		clientFactory.setHttpClientConfig(httpClientConfigBuilder.build());
		return clientFactory.getObject();
	}

	public JestClient getClient() {
		return client;
	}

	@Override
	public boolean open() {
		if (client == null) {
			client = createClient();
		}
		messageProcessor.init();
		return true;
	}

	@Override
	public void close() {
		messageProcessor.flush();
	}

	@Override
	public boolean isOpened() {
		return true;
	}

	@Override
	public void init() {
		this.client = createClient();
	}

	@Override
	public void deinit() {
		client.shutdownClient();
		options.deinit();
	}

	@Override
	public boolean send(ESIndex index) {
		return messageProcessor.send(index);
	}

	@Override
	public String getClusterName() {
		NodesInfo nodesinfo = new NodesInfo.Builder().build();
		String clusterName;
		try {
			JestResult result = client.execute(nodesinfo);
			clusterName = result.getValue("cluster_name").toString();
		} catch (IOException | NullPointerException e) {
			clusterName = new String();
		}
		return clusterName;
	}
}
