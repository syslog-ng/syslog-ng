#ifndef MOCK_TRANSPORT_H_INCLUDED
#define MOCK_TRANSPORT_H_INCLUDED 1

#include "logtransport.h"

/* macro to be used when injecting an error in the I/O stream */
#define LTM_INJECT_ERROR(err)   (GUINT_TO_POINTER(err)), 0
/* macro to be used at the end of the I/O stream */
#define LTM_EOF                 NULL, 0

#define LTM_INPUT_AS_STREAM     TRUE
#define LTM_INPUT_AS_RECORDS    FALSE

LogTransport *
log_transport_mock_new(gboolean input_is_a_stream, gchar *read_buffer1, gssize read_buffer_length1, ...);

#endif
