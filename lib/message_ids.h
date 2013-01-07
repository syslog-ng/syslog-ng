#ifndef MESSAGE_IDS_H
#define MESSAGE_IDS_H 1

#define MSG_APPLICATION_STARTED               1
#define MSG_APPLICATION_TERMINATED            2
#define MSG_APPLICATION_RELOAD                3
#define MSG_SERVICE_INSTALLED                 16
#define MSG_SERVICE_REMOVED                   17
#define MSG_SOCKETS_FAIL                      23
#define MSG_LOGFILE_OPEN_FAIL                 24
#define MSG_CONFLOAD_FAIL                     25
#define MSG_FINDCERT_FAIL                     32
#define MSG_THREAD_FAIL                       33
#define MSG_CRYPTO_FAIL                       34
#define MSG_SYSLOG_SERVER_CONNECTION_FAILED   35
#define MSG_SYSLOG_SERVER_RECONNECTED         36
#define MSG_INVALID_EVENT_CONTAINER           37
#define MSG_SYSLOG_SERVER_XML_UPGRADE_FAILED  38
#define MSG_SYSLOG_SERVER_XML_UPGRADE_WARNING 39
#define MSG_SYSLOG_BAD_CONFIG_VERSION         40
#define MSG_SYSLOG_TRIAL_EXPIRED              41
#define MSG_LICENSE_NOT_FOUND                 56
#define MSG_SYSLOG_SERVER_TRY_TO_RECONNECT    57

#define MSG_TOO_MUCH_SDATA_ITEM               100
#define MSG_TOO_MUCH_TAGS                     101
#define MSG_INVALID_TAG                       102
#define MSG_UNKNOWN_RCPTID_STATE_VERSION      103

#define MSG_MESSAGE_IS_DROPPED                104

#define MSG_LOGREADER_SEEK_ERROR              105
#define MSG_LOGREADER_FSTAT_ERROR             106
#define MSG_LOGREADER_RESPONSE_TIMEOUT        107
#define MSG_READER_CANT_POLL                  108
#define MSG_LOGREADER_UNKNOWN_ENCODING        109
#define MSG_LOGREADER_UNKNOWN_FORMAT          110

#define MSG_LOGWRITER_EOF_OCCURED             111
#define MSG_LOGWRITER_RESPONSE_TIMEOUT        112
#define MSG_LOGWRITER_UNKNOWN_FLAG            113
#define MSG_LOGWRITER_WRONG_MARKMODE          114

#define MSG_CANT_RESOLVE_HOSTNAME             115
#define MSG_CHARACTER_CONVERSION_FAILED       116

#define MSG_CANT_ALLOC_NV_HANDLE              117

#define MSG_CANT_CREATE_PERSIST_FILE          118
#define MSG_INVALID_PERSIST_HANDLE            119
#define MSG_CORRUPTED_PERSIST_HEADER          120
#define MSG_CANT_ALLOCATE_KEY_IN_PERSIST      121
#define MSG_PERSIST_KEY_IS_TOO_LARGE          122
#define MSG_PERSIST_FILE_TOO_LARGE            123
#define MSG_CANT_MAPPING_PERSIST_FILE         124
#define MSG_PERSIST_FILE_FORMAT_ERROR         125
#define MSG_CORRUPTED_PERSIST_HANDLE          126
#define MSG_CANT_RENAME_PERSIST_FILE          127

#define MSG_CANT_FIND_PLUGIN                  128
#define MSG_CANT_OPEN_PLUGIN                  129
#define MSG_CANT_FIND_PLUGIN_INIT_FUNCTION    130

#define MSG_CANT_SETTING_TLS_SESSION_CONTEXT  131
#define MSG_CERTIFICATE_VALIDATION_FAILED     132

#define MSG_SSL_READING_ERROR                 133
#define MSG_SSL_WRITINTG_ERROR                134

#define MSG_CANT_FIND_CERTIFICATE             135
#define MSG_CANT_LOAD_DYNAMIC_ENGINE          136
#define MSG_CANT_FIND_CAPI_ENGINE             137
#define MSG_CANT_LOAD_CAPI_ENGINE             138
#define MSG_CANT_FIND_PRIVATE_KEY             139

#define MSG_CANT_OPEN_FILE                    140
#define MSG_CANT_RECOVER_FILE_STATE           141
#define MSG_CANT_FIND_PROTO                   142
#define MSG_CANT_INITIALIZE_READER            143
#define MSG_CANT_START_FILEMONITOR            144

#define MSG_DIRECTORY_DOESNT_EXIST            145
#define MSG_CANT_MONITOR_DIRECTORY            146
#define MSG_CANT_FIND_PORT_NUMBER             147
#define MSG_CANT_FIND_TRANSPORT_FOR_PLUGIN    148
#define MSG_TLS_OPTION_MISSING                149

#define MSG_ERROR_BINDING_SOCKET              150
#define MSG_ERROR_CREATING_SOCKET             151
#define MSG_REJECT_CONNECTION                 152
#define MSG_BAD_REGEXP                        153

#define MSG_IO_ERROR_WRITING                  154
#define MSG_CHAR_ENCODING_FAILED              155
#define MSG_CANT_LOAD_READER_STATE            156
#define MSG_IO_ERROR_READING                  157
#define MSG_EOF_OCCURED                       158
#define MSG_REVERSE_CONVERTING_FAILED         159

#define MSG_CANT_CREATE_EVENTLOG_READER       160
#define MSG_CANT_INIT_EVENTLOG_READER         161
#define MSG_CHANNEL_OPTION_MISSING            162
#define MSG_CANT_READ_EVENTLOG                163
#define MSG_CANT_SEEK_IN_EVENTLOG             164
#define MSG_CANT_OPEN_PUBLISHER               165
#define MSG_EVENTLOG_SUBSCRIPTION_FAILED      166
#define MSG_CANT_RENDER_EVENT                 167
#define MSG_CANT_PARSE_EVENT_XML              168

#define MSG_RLTP_SERVER_CLOSED_CONNECTION     169
#define MSG_RLTP_SERVER_DOESNT_SUPPORT_TLS    170
#define MSG_RLTP_CANT_INITIALIZE_TLS          171

#define MSG_THREADED_ISNT_AVAILABLE           200
#define MSG_RESERVED_WORD_USED                201
#define MSG_OBSOLATED_KEYWORD_USED            202
#define MSG_SAME_PREFIX_GARBAGE               203
#define MSG_CERTIFICATE_PUPOSE_INVALID        204
#define MSG_TLS_CANT_COMPRESS                 205
#define MSG_SERVER_UNACCESSIBLE               206
#define MSG_EVENT_CONTAINER_CHANGED           207
#define MSG_EVENT_RECORD_CONTINUITY_BROKEN    208
#define MSG_RLTP_MESSAGE_LENGTH_TOO_LARGE     209

#define MSG_NONBLOCKING_READ_BLOCKED          300
#define MSG_NONBLOCKING_WRITE_BLOCKED         301
#define MSG_WRITE_SUSPENDING_IO_ERROR         302
#define MSG_WRITE_SUSPEND_TIMEOUT_ELAPSED     303

#define MSG_TLS_CERT_ACCEPTED                 310
#define MSG_TLS_CERT_INVALID                  311
#define MSG_TLS_CERT_UNTRUSTED                312
#define MSG_TLS_CLRS_NOT_FOUND                313
#define MSG_TLS_CERT_VALIDATION_ERROR         314

#define MSG_SERVER_CONNECTION_ESTABLISHED     320
#define MSG_SERVER_CONNECTION_BROKEN          321
#define MSG_SERVER_TRY_RECONNECT              322

#define MSG_INVALID_ENCODING_SEQ              330

#define MSG_MAXIMUM_SIZE_REACHED              500

#define MSG_FILE_SOURCE_NOT_FOUND             501
#define MSG_FILE_OLDER_OVERWRITE              502

#define MSG_CUSTOM                            1000

#endif
