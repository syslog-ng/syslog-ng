#ifndef _SNG_PYTHON_AUTH_HEADERS_H
#define _SNG_PYTHON_AUTH_HEADERS_H

typedef struct _PythonHttpHeaderPlugin PythonHttpHeaderPlugin;

PythonHttpHeaderPlugin *python_http_header_new(void);

void python_http_header_set_loaders(PythonHttpHeaderPlugin *self, GList *loaders);
void python_http_header_set_class(PythonHttpHeaderPlugin *self, gchar *class);
void python_http_header_set_option(PythonHttpHeaderPlugin *self, gchar *key, gchar *value);

#endif
