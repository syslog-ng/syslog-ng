#include <config.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <netdb.h>
#include <string.h>
#include <glib.h>

#include <openssl/crypto.h>
#include <openssl/x509.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <unistd.h>

#define MAX_MESSAGE_LENGTH 8192

#define USEC_PER_SEC      1000000

#ifndef MIN
#define MIN(a, b)    ((a) < (b) ? (a) : (b))
#endif

FILE *readfrom = NULL;
int rate = 1000;
int unix_socket = 0;
int unix_socket_i = 0;
int unix_socket_x = 0;
int sock_type_s = 0;
int sock_type_d = 0;
int sock_type = SOCK_STREAM;
int sock = -1;
int message_length = 256;
int interval = 10;
int number_of_messages = 0;
int csv = 0;
int quiet = 0;
int syslog_proto = 0;
int framing = 1;
int usessl = 0;
int skip_tokens = 3;
char *read_file = NULL;

typedef ssize_t (*send_data_t)(void *user_data, void *buf, size_t length);

static ssize_t
send_plain(void *user_data, void *buf, size_t length)
{
  return send((long)user_data, buf, length, 0);
}

#if ENABLE_SSL
static ssize_t
send_ssl(void *user_data, void *buf, size_t length)
{
  return SSL_write((SSL *)user_data, buf, length);
}
#endif

static inline unsigned long
time_val_diff_in_usec(struct timeval *t1, struct timeval *t2)
{
  return (t1->tv_sec - t2->tv_sec) * USEC_PER_SEC + (t1->tv_usec - t2->tv_usec);
}

static inline void
time_val_diff_in_timeval(struct timeval *res, const struct timeval *t1, const struct timeval *t2)
{
  res->tv_sec = (t1->tv_sec - t2->tv_sec);
  res->tv_usec = (t1->tv_usec - t2->tv_usec);
  if (res->tv_usec < 0)
    {
      res->tv_sec--;
      res->tv_usec += USEC_PER_SEC;
    }
}

static ssize_t
write_chunk(send_data_t send_func, void *send_func_ud, void *buf, size_t buf_len)
{
  ssize_t rc;
  size_t pos = 0;

  while (pos < buf_len)
    {
      rc = send_func(send_func_ud, buf + pos, buf_len - pos);

      if (rc < 0)
        return -1;

      else if (rc == 0)
        {
          errno = ECONNABORTED;
          return -1;
        }
      pos += rc;
    }
  return pos;
}

static int
parse_line(const char *line, char *host, char *program, char *pid, char **msg)
{
  const char *pos0;
  const char *pos = line;
  const char *end;
  int space = skip_tokens;
  int pid_len;

  /* Find token */
  while (space)
    {
      pos = strchr(pos, ' ');

      if (!pos)
        return -1;
      pos++;

      space --;
    }

  pos = strchr(pos, ':');
  if (!pos)
    return -1;

  /* pid */
  pos0 = pos;
  if (*(--pos) == ']')
    {
      end = pos - 1;
      while (*(--pos) != '[')
        ;

      pid_len = end - pos; /* 'end' points to the last character of the pid string (not off by one), *pos = '[' -> pid length = end - pos*/
      memcpy(pid, pos + 1, pid_len);
      pid[pid_len] = '\0';
    }
  else
    {
      pid[0] = '\0';
      ++pos; /* the 'pos' has been decreased in the condition (']'), reset it to the original position */
    }

  /* Program */
  end = pos;
  while (*(--pos) != ' ')
    ;

  memcpy(program, pos + 1, end - pos - 1);
  program[end-pos-1] = '\0';

  /* Host */
  end = pos;
  while (*(--pos) != ' ')
    ;

  memcpy(host, pos + 1, end - pos - 1);
  host[end-pos-1] = '\0';

  *msg = ((char*)pos0) + 2;

  return 1;
}

static int
gen_next_message(FILE *source, char *buf, int buflen)
{
  static int lineno = 0;
  struct tm *tm;
  struct timeval now;
  char line[4096];
  char stamp[32];
  int linelen;
  int tslen;
  char *temp;

  char host[128], program[128], pid[16];
  char *msg;

  while (1)
    {
      temp = NULL;
      if (feof(source))
        return -1;
      temp = fgets(line, sizeof(line), source);
      if (!temp)
        return -1;

      if (parse_line(line, host, program, pid, &msg) > 0)
        break;

      fprintf(stderr, "\rInvalid line %d                  \n", ++lineno);
    }
  gettimeofday(&now, NULL);
  tm = localtime(&now.tv_sec);
  tslen = strftime(stamp, sizeof(stamp), "%Y-%m-%dT%H:%M:%S", tm);

  if (syslog_proto)
    {
      char tmp[11];
      linelen = snprintf(buf + 10, buflen - 10, "<38>1 %.*s %s %s %s - - \xEF\xBB\xBF%s", tslen, stamp, host, program, (pid[0] ? pid : "-"), msg);
      snprintf(tmp, sizeof(tmp), "%09d ", linelen);
      memcpy(buf, tmp, 10);
      linelen += 10;
    }
  else
    {
      if (*pid)
        linelen = snprintf(buf, buflen, "<38>%.*s %s %s[%s]: %s", tslen, stamp, host, program, pid, msg);
      else
        linelen = snprintf(buf, buflen, "<38>%.*s %s %s: %s", tslen, stamp, host, program, msg);
    }

  return linelen;
}

static int
gen_messages(send_data_t send_func, void *send_func_ud)
{
  struct timeval now, start, last_ts_format, last_throttle_check;
  char linebuf[MAX_MESSAGE_LENGTH + 1];
  char stamp[32];
  char intbuf[16];
  int linelen = 0;
  int i, run_id;
  unsigned long count = 0, last_count = 0;
  char padding[] = "PADD";
  long buckets = rate - (rate / 10);
  double diff_usec;
  struct timeval diff_tv;
  int pos_timestamp1 = 0, pos_timestamp2 = 0, pos_seq = 0;
  int rc, hdr_len = 0;
  unsigned int counter = 0;
  uint64_t sum_len = 0;

  gettimeofday(&start, NULL);
  now = start;
  last_throttle_check = now;
  run_id = start.tv_sec;

  /* force reformat of the timestamp */
  last_ts_format = now;
  last_ts_format.tv_sec--;

  if (!readfrom)
    {
      if (syslog_proto)
        {
          if (sock_type == SOCK_STREAM && framing)
            hdr_len = snprintf(linebuf, sizeof(linebuf), "%d ", message_length);

          linelen = snprintf(linebuf + hdr_len, sizeof(linebuf) - hdr_len, "<38>1 2007-12-24T12:28:51+02:00 localhost localprg 1234 - - \xEF\xBB\xBFseq: %010d, runid: %-10d, stamp: %-19s ", 0, run_id, "");
          pos_timestamp1 = 6 + hdr_len;
          pos_seq = 68 + hdr_len;
          pos_timestamp2 = 106 + hdr_len;
        }
      else
        {
          linelen = snprintf(linebuf, sizeof(linebuf), "<38>2007-12-24T12:28:51 localhost localprg[1234]: seq: %010d, runid: %-10d, stamp: %-19s ", 0, run_id, "");
          pos_timestamp1 = 4;
          pos_seq = 55;
          pos_timestamp2 = 93;
        }

      if (linelen > message_length)
        {
          fprintf(stderr, "Warning: message length is too small, the minimum is %d bytes\n", linelen);
          return 1;
        }
    }

  for (i = linelen; i < message_length - 1; i++)
    {
      linebuf[i + hdr_len] = padding[(i - linelen) % (sizeof(padding) - 1)];
    }
  linebuf[hdr_len + message_length - 1] = '\n';
  linebuf[hdr_len + message_length] = 0;
  linelen = strlen(linebuf);
  counter = 0;
  while (time_val_diff_in_usec(&now, &start) < ((int64_t)interval) * USEC_PER_SEC)
    {
      if(number_of_messages != 0 && count >= number_of_messages)
        {
          break;
        }
      gettimeofday(&now, NULL);

      diff_usec = time_val_diff_in_usec(&now, &last_throttle_check);
      if (buckets == 0 || diff_usec > 1e5)
        {
          /* check rate every 0.1sec */
          long new_buckets;

          new_buckets = (rate * diff_usec) / USEC_PER_SEC;
          if (new_buckets)
            {
              buckets = MIN(rate, buckets + new_buckets);
              last_throttle_check = now;
            }
        }

      if (buckets == 0)
        {
          struct timespec tspec, trem;
          long msec = (1000 / rate) + 1;

          tspec.tv_sec = msec / 1000;
          tspec.tv_nsec = (msec % 1000) * 1e6;
          while (nanosleep(&tspec, &trem) < 0 && errno == EINTR)
            {
              tspec = trem;
            }
          continue;
        }

      if (readfrom)
        {
          rc = gen_next_message(readfrom, linebuf, sizeof(linebuf));
          if (rc == -1)
            break;

          linelen = rc;
        }

      if (now.tv_sec != last_ts_format.tv_sec)
        {
          /* tv_has has changed, update message timestamp & print current message rate */

          if (!readfrom)
            {
              int len;
              struct tm *tm;

              tm = localtime(&now.tv_sec);
              len = strftime(stamp, sizeof(stamp), "%Y-%m-%dT%H:%M:%S", tm);
              memcpy(&linebuf[pos_timestamp1], stamp, len);
              memcpy(&linebuf[pos_timestamp2], stamp, len);
            }

          if (csv)
            {
              time_val_diff_in_timeval(&diff_tv, &now, &start);
              printf("%lu.%06lu;%lu\n", (long) diff_tv.tv_sec, (long) diff_tv.tv_usec, count);
            }
          else if (!quiet)
            {
              diff_usec = time_val_diff_in_usec(&now, &last_ts_format);
              fprintf(stderr, "count=%ld, rate = %.2lf msg/sec                 \r", count, ((double) (count - last_count) * USEC_PER_SEC) / diff_usec);
            }
          last_ts_format = now;
          last_count = count;
        }

      if (!readfrom)
        {
          /* add sequence number */
          snprintf(intbuf, sizeof(intbuf), "%010ld", count);
          memcpy(&linebuf[pos_seq], intbuf, 10);
        }

      sum_len += linelen;
      rc = write_chunk(send_func, send_func_ud, linebuf, linelen);
      if (rc < 0)
        {
          fprintf(stderr, "Send error %s, results may be skewed.\n", strerror(errno));
          break;
        }
      buckets--;
      count++;
    }

  gettimeofday(&now, NULL);
  diff_usec = time_val_diff_in_usec(&now, &start);
  time_val_diff_in_timeval(&diff_tv, &now, &start);
  printf("%lu.%06lu;%lu\n", (long) diff_tv.tv_sec, (long) diff_tv.tv_usec, count);
  fprintf(stderr, "average rate = %.2lf msg/sec, count=%ld, time=%ld.%03ld, (last) msg size=%d, bandwidth=%.2lf kB/sec\n",
    (double) count * USEC_PER_SEC / diff_usec, count, diff_tv.tv_sec, diff_tv.tv_usec / 1000, linelen,
    (double) sum_len * (USEC_PER_SEC / 1024) / diff_usec);

  return 0;
}

#if ENABLE_SSL
static inline int
gen_messages_ssl(int sock)
{
  int ret = 0;
  int err;
  SSL_CTX* ctx;
  SSL *ssl;

  /* Initialize SSL library */
  OpenSSL_add_ssl_algorithms();

  if (NULL == (ctx = SSL_CTX_new(SSLv23_client_method())))
    return 1;

  if (NULL == (ssl = SSL_new(ctx)))
    return 1;

  SSL_load_error_strings();
  ERR_load_crypto_strings();

  SSL_set_fd (ssl, sock);
  if (-1 == (err = SSL_connect(ssl)))
    {
      fprintf(stderr, "SSL connect failed\n");
      ERR_print_errors_fp(stderr);
      return 2;
    }

  ret = gen_messages(send_ssl, ssl);

  SSL_shutdown (ssl);

  /* Clean up. */
  SSL_free (ssl);
  SSL_CTX_free (ctx);

  return ret;
}
#else
#define gen_messages_ssl gen_messages_plain
#endif

static inline int
gen_messages_plain(int sock)
{
  return gen_messages(send_plain, (void *)((long)sock));
}

static GOptionEntry loggen_options[] = {
  { "rate", 'r', 0, G_OPTION_ARG_INT, &rate, "Number of messages to generate per second", "<msg/sec>" },
  { "inet", 'i', 0, G_OPTION_ARG_NONE, &unix_socket_i, "Use IP-based transport (TCP, UDP)", NULL },
  { "unix", 'x', 0, G_OPTION_ARG_NONE, &unix_socket_x, "Use UNIX domain socket transport", NULL },
  { "stream", 'S', 0, G_OPTION_ARG_NONE, &sock_type_s, "Use stream socket (TCP and unix-stream)", NULL },
  { "dgram", 'D', 0, G_OPTION_ARG_NONE, &sock_type_d, "Use datagram socket (UDP and unix-dgram)", NULL },
  { "size", 's', 0, G_OPTION_ARG_INT, &message_length, "Specify the size of the syslog message", "<size>" },
  { "interval", 'I', 0, G_OPTION_ARG_INT, &interval, "Number of seconds to run the test for", "<sec>" },
  { "syslog-proto", 'P', 0, G_OPTION_ARG_NONE, &syslog_proto, "Use the new syslog-protocol message format (see also framing)", NULL },
  { "no-framing", 'F', G_OPTION_FLAG_REVERSE, G_OPTION_ARG_NONE, &framing, "Don't use syslog-protocol style framing, even if syslog-proto is set", NULL },
#if ENABLE_SSL
  { "use-ssl", 'U', 0, G_OPTION_ARG_NONE, &usessl, "Use ssl layer", NULL },
#endif
  { "read-file", 'R', 0, G_OPTION_ARG_STRING, &read_file, "Read log messages from file", "<filename>" },
  { "skip-tokens", 0, 0, G_OPTION_ARG_INT, &skip_tokens, "Skip the given number of tokens (delimined by a space) at the beginning of each line (default value: 3)", "<number>" },
  { "csv", 'C', 0, G_OPTION_ARG_NONE, &csv, "Produce CSV output", NULL },
  { "number", 'n', 0, G_OPTION_ARG_INT, &number_of_messages, "Number of messages to generate", "<number>" },
  { "quiet", 'Q', 0, G_OPTION_ARG_NONE, &quiet, "Don't print the msg/sec data", NULL },
  { NULL }
};

int
main(int argc, char *argv[])
{
  int ret = 0;
  GError *error = NULL;
  GOptionContext *ctx = NULL;

  ctx = g_option_context_new(" target port");
  g_option_context_add_main_entries(ctx, loggen_options, 0);
  if (!g_option_context_parse(ctx, &argc, &argv, &error))
    {
      fprintf(stderr, "Option parsing failed: %s\n", error->message);
      return 1;
    }

  if (unix_socket_i)
    unix_socket = 0;
  if (unix_socket_x)
    unix_socket = 1;

  if (sock_type_d)
    sock_type = SOCK_DGRAM;
  if (sock_type_s)
    sock_type = SOCK_STREAM;

  if (message_length > MAX_MESSAGE_LENGTH)
    {
      fprintf(stderr, "Message size too large, limiting to %d\n", MAX_MESSAGE_LENGTH);
      message_length = MAX_MESSAGE_LENGTH;
    }

  if (syslog_proto)
    framing = 1;

  if (read_file != NULL)
    {
      if (read_file[0] == '-' && read_file[1] == '\0')
        {
          readfrom = stdin;
        }
      else
        {
          readfrom = fopen(read_file, "r");
          if (!readfrom)
            {
              const int bufsize = 1024;
              char cbuf[bufsize];
              snprintf(cbuf, bufsize, "fopen: %s", read_file);
              perror(cbuf);
              return 1;
            }
        }
    }

  /* skip the program name after parsing - argc is at least 1 with argv containing the program name
     we will only have the parameters after the '--' (if any) */
  --argc;
  ++argv;

  if (unix_socket && usessl)
    {
      fprintf(stderr, "Error: trying to use SSL on a Unix Domain Socket\n");
      return 1;
    }
  if (!unix_socket)
    {

      if (argc < 2)
        {
          fprintf(stderr, "No target server or port specified\n");
          return 1;
        }

      if (1)
        {
#if HAVE_GETADDRINFO
          struct addrinfo hints;
          struct addrinfo *res, *rp;

          memset(&hints, 0, sizeof(hints));
          hints.ai_family = AF_UNSPEC;
          hints.ai_socktype = sock_type;
#ifdef AI_ADDRCONFIG
          hints.ai_flags = AI_ADDRCONFIG;
#endif
          hints.ai_protocol = 0;
          if (getaddrinfo(argv[0], argv[1], &hints, &res) != 0)
            {
              fprintf(stderr, "Name lookup error\n");
              return 2;
            }

          for (rp = res; rp != NULL; rp = rp->ai_next)
            {
              sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
              if (sock == -1)
                continue;

              if (connect(sock, rp->ai_addr, rp->ai_addrlen) != -1)
                break;

              close(sock);
              sock = -1;
            }
          freeaddrinfo(res);
#else
          struct hostent *he;
          struct servent *se;
          struct sockaddr_in s_in;

          he = gethostbyname(argv[0]);
          if (!he)
            {
              fprintf(stderr, "Name lookup error\n");
              return 2;
            }
          s_in.sin_family = AF_INET;
          s_in.sin_addr = *(struct in_addr *) he->h_addr;

          se = getservbyname(argv[1], sock_type == SOCK_STREAM ? "tcp" : "udp");
          if (se)
            s_in.sin_port = se->s_port;
          else
            s_in.sin_port = htons(atoi(argv[1]));


          sock = socket(AF_INET, sock_type, 0);
          if (sock < 0)
            {
              fprintf(stderr, "Error creating socket: %s\n", strerror(errno));
              return 2;
            }

          if (connect(sock, (struct sockaddr *) &s_in, sizeof(s_in)) < 0)
            {
              close(sock);
              sock = -1;
            }
#endif
        }

      if (sock == -1)
        {
          fprintf(stderr, "Error connecting to target server: %s\n", strerror(errno));
          return 2;
        }
    }
  else
    {
      struct sockaddr_un saun;

      sock = socket(AF_UNIX, sock_type, 0);
      saun.sun_family = AF_UNIX;
      strncpy(saun.sun_path, argv[0], sizeof(saun.sun_path));
      if (connect(sock, (struct sockaddr *) &saun, sizeof(saun)) < 0)
        {
          fprintf(stderr, "Error connecting to target server: %s\n", strerror(errno));
          return 2;
        }
    }

  ret = (usessl ? gen_messages_ssl : gen_messages_plain)(sock);

  shutdown(sock, SHUT_RDWR);

  if (readfrom && readfrom != stdin)
    fclose(readfrom);
  return ret;
}
