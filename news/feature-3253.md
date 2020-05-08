`http`: add support for proxy option

log {
   source { system(); };
   destination { http( url("SYSLOG_SERVER_IP:PORT") proxy("PROXY_IP:PORT") method("POST") ); };
};
