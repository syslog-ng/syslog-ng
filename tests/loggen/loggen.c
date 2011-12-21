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
#include <signal.h>
#include <pthread.h>

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

int rate = 1000;
int unix_socket = 0;
int use_ipv6 = 0;
int unix_socket_i = 0;
int unix_socket_x = 0;
int sock_type_s = 0;
int sock_type_d = 0;
int sock_type = SOCK_STREAM;
struct sockaddr *dest_addr;
socklen_t dest_addr_len;
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
int idle_connections = 0;
int active_connections = 1;
int loop_reading = 0;
int dont_parse = 0;
static gint display_version;
char *sdata_value = NULL;
int rltp = 0;

/* results */
guint64 sum_count;
struct timeval sum_time;
gint raw_message_length;
SSL_CTX* ssl_ctx;

typedef ssize_t (*send_data_t)(void *user_data, void *buf, size_t length);

int rltp_batch_size = 100;

int *rltp_chunk_counters;

void
read_sock(int sock, char *buf, int bufsize)
{
  int rb = 0;
  int n;
  do {
    n = recv(sock, buf + rb, bufsize - 1 - rb, 0);
    if (n != -1)
      rb += n;
  } while (n == -1 && (errno == EAGAIN || errno == EWOULDBLOCK));
  buf[rb] = '\0';
}

static ssize_t
send_plain(void *user_data, void *buf, size_t length)
{
  long fd = (long)user_data;
  int id = GPOINTER_TO_INT(g_thread_self()->data);
  gsize len = 0;
  static gchar expected[100] = {0};
  int res = send(fd, buf, length, 0);
  if (rltp)
    {
      rltp_chunk_counters[id] += 1;
      if (rltp_chunk_counters[id] == rltp_batch_size)
        {
          char cbuf[256];
          send(fd, ".\n",2,0);
          read_sock(fd, cbuf, 256);
          len = g_snprintf(expected,100,"220 RECEIVED=%d\n",rltp_chunk_counters[id]);
          if (strncmp(cbuf,expected,len)!=0)
            {
              fprintf(stderr,"EXIT BAD SERVER REPLY: %s EXPECTED: %s",cbuf,expected);
              return -1;
            }
          send(fd, "DATA\n", 5, 0);
          read_sock(fd, cbuf, 256);
          if (strncmp(cbuf,"220 READY\n",strlen("220 READY\n"))!=0)
            {
              fprintf(stderr,"EXIT BAD SERVER REPLY: %s\n",cbuf);
              return -1;
            }
          rltp_chunk_counters[id] = 0;
        }
    }
  return res;
}

#if ENABLE_SSL
static ssize_t
send_ssl(void *user_data, void *buf, size_t length)
{
  return SSL_write((SSL *)user_data, buf, length);
}
#endif

static unsigned long
time_val_diff_in_usec(struct timeval *t1, struct timeval *t2)
{
  return (t1->tv_sec - t2->tv_sec) * USEC_PER_SEC + (t1->tv_usec - t2->tv_usec);
}

static void
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

static void
time_val_add_time_val(struct timeval *res, const struct timeval *t1, const struct timeval *t2)
{
  res->tv_sec = (t1->tv_sec + t2->tv_sec);
  res->tv_usec = (t1->tv_usec + t2->tv_usec);
  if (res->tv_usec >= 1e6)
    {
      res->tv_sec++;
      res->tv_usec -= USEC_PER_SEC;
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
  struct tm tm;
  time_t now;
  char line[MAX_MESSAGE_LENGTH+1];
  char stamp[32];
  int linelen;
  int tslen;
  char *temp;

  char host[128], program[128], pid[16];
  char *msg = NULL;

  while (1)
    {
      temp = NULL;
      if (feof(source))
        {
          if (loop_reading)
          {
            // Restart reading from the beginning of the file
            rewind(source);
          }
          else
            return -1;
        }
      temp = fgets(line, sizeof(line), source);
      if (!temp)
        {
          if (loop_reading)
          {
            // Restart reading from the beginning of the file
            rewind(source);
            temp = fgets(line, sizeof(line), source);
          }
          else
            return -1;
        }
      if (dont_parse)
        break;

      if (parse_line(line, host, program, pid, &msg) > 0)
        break;

      fprintf(stderr, "\rInvalid line %d                  \n", ++lineno);
    }
  now = time(NULL);
  localtime_r(&now, &tm);
  tslen = strftime(stamp, sizeof(stamp), "%Y-%m-%dT%H:%M:%S", &tm);

  if (dont_parse)
    {
      linelen = snprintf(buf, buflen-1, "%s", line);
      return linelen;
    }

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
connect_server(void)
{
  int sock;

  sock = socket(dest_addr->sa_family, sock_type, 0);
  if (sock < 0)
    {
      fprintf(stderr, "Error creating socket: %s\n", g_strerror(errno));
      return -1;
    }
  if (connect(sock, dest_addr, dest_addr_len) < 0)
    {
      fprintf(stderr, "Error connecting socket: %s\n", g_strerror(errno));
      close(sock);
      return -1;
    }
  return sock;
}

static guint64
gen_messages(send_data_t send_func, void *send_func_ud, int thread_id, FILE *readfrom)
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
  gint64 sum_linelen = 0;
  char *testsdata = NULL;

  gettimeofday(&start, NULL);
  now = start;
  last_throttle_check = now;
  run_id = start.tv_sec;

  /* force reformat of the timestamp */
  last_ts_format = now;
  last_ts_format.tv_sec--;

   if (sdata_value)
     {
       testsdata = strdup(sdata_value);
     }
   else
     {
       testsdata = strdup("-");
     }

  if (!readfrom)
    {
      if (syslog_proto)
        {
          if (sock_type == SOCK_STREAM && framing)
            hdr_len = snprintf(linebuf, sizeof(linebuf), "%d ", message_length);

          linelen = snprintf(linebuf + hdr_len, sizeof(linebuf) - hdr_len, "<38>1 2007-12-24T12:28:51+02:00 localhost prg%05d 1234 - %s \xEF\xBB\xBFseq: %010d, thread: %04d, runid: %-10d, stamp: %-19s ", thread_id, testsdata, 0, thread_id, run_id, "");

          pos_timestamp1 = 6 + hdr_len;
          pos_seq = 68 + hdr_len + strlen(testsdata) - 1;
          pos_timestamp2 = 120 + hdr_len + strlen(testsdata) - 1;
        }
      else
        {
          if (sock_type == SOCK_STREAM && framing)
            hdr_len = snprintf(linebuf, sizeof(linebuf), "%d ", message_length);
          else
            hdr_len = 0;
          linelen = snprintf(linebuf + hdr_len, sizeof(linebuf) - hdr_len, "<38>2007-12-24T12:28:51 localhost prg%05d[1234]: seq: %010d, thread: %04d, runid: %-10d, stamp: %-19s ", thread_id, 0, thread_id, run_id, "");
          pos_timestamp1 = 4 + hdr_len;
          pos_seq = 55 + hdr_len;
          pos_timestamp2 = 107 + hdr_len;
        }

      if (linelen > message_length)
        {
          fprintf(stderr, "Warning: message length is too small, the minimum is %d bytes\n", linelen);
          return 0;
        }
    }
  for (i = linelen; i < message_length - 1; i++)
    {
      linebuf[i + hdr_len] = padding[(i - linelen) % (sizeof(padding) - 1)];
    }
  linebuf[hdr_len + message_length - 1] = '\n';
  linebuf[hdr_len + message_length] = 0;

  /* NOTE: all threads calculate raw_message_length. This code could use some refactorization. */
  raw_message_length = linelen = strlen(linebuf);
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
          sum_linelen = sum_linelen+rc;

        }

      if (now.tv_sec != last_ts_format.tv_sec)
        {
          /* tv_has has changed, update message timestamp & print current message rate */

          if (!readfrom)
            {
              int len;
              time_t now_time_t;
              struct tm tm;

              now_time_t = now.tv_sec;
              localtime_r(&now_time_t, &tm);
              len = strftime(stamp, sizeof(stamp), "%Y-%m-%dT%H:%M:%S", &tm);
              memcpy(&linebuf[pos_timestamp1], stamp, len);
              memcpy(&linebuf[pos_timestamp2], stamp, len);
            }

          diff_usec = time_val_diff_in_usec(&now, &last_ts_format);
          if (csv)
            {
              time_val_diff_in_timeval(&diff_tv, &now, &start);
              printf("%d;%lu.%06lu;%.2lf;%lu\n", thread_id, (long) diff_tv.tv_sec, (long) diff_tv.tv_usec, (((double) (count - last_count) * USEC_PER_SEC) / diff_usec),count);

            }
          else if (!quiet)
            {
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
  if (csv)
    printf("%d;%lu.%06lu;%.2lf;%lu\n", thread_id, (long) diff_tv.tv_sec, (long) diff_tv.tv_usec, (((double) (count - last_count) * USEC_PER_SEC) / diff_usec), count);

  if (readfrom)
    raw_message_length = sum_linelen/count;
  free(testsdata);
  return count;
}

#if ENABLE_SSL
static guint64
gen_messages_ssl(int sock, int id, FILE *readfrom)
{
  int ret = 0;
  int err;
  SSL *ssl;

  if (NULL == (ssl = SSL_new(ssl_ctx)))
    return 1;

  SSL_set_fd (ssl, sock);
  if (-1 == (err = SSL_connect(ssl)))
    {
      fprintf(stderr, "SSL connect failed\n");
      ERR_print_errors_fp(stderr);
      return 2;
    }

  ret = gen_messages(send_ssl, ssl, id, readfrom);

  SSL_shutdown (ssl);
  /* Clean up. */
  SSL_free (ssl);

  return ret;
}
#else
#define gen_messages_ssl gen_messages_plain
#endif

static guint64
gen_messages_plain(int sock, int id, FILE *readfrom)
{
  return gen_messages(send_plain, GINT_TO_POINTER(sock), id, readfrom);
}

GMutex *thread_lock;
GCond *thread_cond;
GCond *thread_finished;
GCond *thread_connected;
gboolean threads_start;
gboolean threads_stop;
gint active_finished;
gint connect_finished;
guint64 sum_count;

/* Multithreaded OPENSSL */
GMutex **ssl_lock_cs;
long *ssl_lock_count;

void ssl_locking_callback(int mode, int type, char *file, int line)
{
  if (mode & CRYPTO_LOCK)
    {
      g_mutex_lock(ssl_lock_cs[type]);
      ssl_lock_count[type]++;
    }
  else
    {
      g_mutex_unlock(ssl_lock_cs[type]);
    }
}

unsigned long ssl_thread_id(void)
{
  unsigned long ret;

  ret=(unsigned long)pthread_self();
  return(ret);
}

void thread_setup(void)
  {
  int i;

  ssl_lock_cs=OPENSSL_malloc(CRYPTO_num_locks() * sizeof(void *));
  ssl_lock_count=OPENSSL_malloc(CRYPTO_num_locks() * sizeof(long));
  for (i=0; i<CRYPTO_num_locks(); i++)
    {
      ssl_lock_cs[i]=g_mutex_new();
    }
  CRYPTO_set_id_callback((unsigned long (*)())ssl_thread_id);
  CRYPTO_set_locking_callback((void (*)())ssl_locking_callback);
}

gpointer
idle_thread(gpointer st)
{
  int sock;

  sock = connect_server();
  if (sock < 0)
    goto error;
  g_mutex_lock(thread_lock);
  connect_finished++;
  if (connect_finished == active_connections + idle_connections)
    g_cond_signal(thread_connected);

  while (!threads_start)
    g_cond_wait(thread_cond, thread_lock);


  while (!threads_stop)
    g_cond_wait(thread_cond, thread_lock);
  g_mutex_unlock(thread_lock);
  close(sock);
  return NULL;
error:
  g_mutex_lock(thread_lock);
  connect_finished++;
  g_cond_signal(thread_connected);
  g_cond_signal(thread_finished);
  g_mutex_unlock(thread_lock);
  return NULL;
}

void
read_rltp_options(int sock,gint *compression_level,gint *require_tls,gint *serialization)
{
  char cbuf[256];
  char *options = NULL;
  read_sock(sock, cbuf, 256);
  options = strstr(cbuf,"250");
  while(options)
    {
      options+=4;
      if (strncmp(options,"compress_level",strlen("compress_level")) == 0)
        {
          options+=strlen("compress_level") + 1;
          *compression_level = strtod(options,NULL);
        }
      else if (strncmp(options,"tls_required",strlen("tls_required")) == 0)
        {
          options+=strlen("tls_required") + 1;
          *require_tls = strtod(options,NULL);
        }
      else if (strncmp(options,"serialized",strlen("serialized")) == 0)
        {
          options+=strlen("serialized") + 1;
          *serialization = strtod(options,NULL);
        }
      options = strstr(options,"250");
    }
}

gpointer
active_thread(gpointer st)
{

  int id = GPOINTER_TO_INT(st);
  gint sock;
  guint64 count;
  struct timeval start, end, diff_tv;
  FILE *readfrom = NULL;


  sock = connect_server();
  if (sock < 0)
    goto error;
  g_mutex_lock(thread_lock);
  connect_finished++;
  if (connect_finished == active_connections + idle_connections)
    g_cond_signal(thread_connected);

  while (!threads_start)
    g_cond_wait(thread_cond, thread_lock);
  g_mutex_unlock(thread_lock);

  int compression_level = 0,
      require_tls = 0,
      serialization = 0;

  if (rltp)
    {
      char cbuf[256];

      /* session id */
      read_sock(sock, cbuf ,256);
      send(sock, "EHLO TID=01234567890123456789012345678901\n", strlen("EHLO TID=01234567890123456789012345678901\n"), 0);
      read_rltp_options(sock,&compression_level,&require_tls,&serialization);
      send(sock, "DATA\n", 5, 0);
      read_sock(sock, cbuf ,256);
      if (strncmp(cbuf,"220 READY\n",strlen("220 READY\n"))!=0)
        {
          fprintf(stderr,"EXIT BAD SERVER REPLY: %s\n",cbuf);
        }
    }

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
              return NULL;
            }
        }
    }

  gettimeofday(&start, NULL);
  count = (usessl ? gen_messages_ssl : gen_messages_plain)(sock, id, readfrom);
  if (rltp && rltp_chunk_counters[id])
    {
      char cbuf[256];
      send(sock, ".\n", 2, 0);
      read_sock(sock, cbuf, 256);
      rltp_chunk_counters[id] = 0;
    }
  shutdown(sock, SHUT_RDWR);
  gettimeofday(&end, NULL);
  time_val_diff_in_timeval(&diff_tv, &end, &start);

  g_mutex_lock(thread_lock);
  sum_count += count;
  time_val_add_time_val(&sum_time, &sum_time, &diff_tv);
  active_finished++;
  if (active_finished == active_connections)
    g_cond_signal(thread_finished);
  g_mutex_unlock(thread_lock);
  close(sock);
  if (readfrom && readfrom != stdin)
    fclose(readfrom);
  return NULL;
error:
  g_mutex_lock(thread_lock);
  connect_finished++;
  active_finished++;
  g_cond_signal(thread_connected);
  g_cond_signal(thread_finished);
  g_mutex_unlock(thread_lock);
  return NULL;
}

static GOptionEntry loggen_options[] = {
  { "rate", 'r', 0, G_OPTION_ARG_INT, &rate, "Number of messages to generate per second", "<msg/sec/active connection>" },
  { "inet", 'i', 0, G_OPTION_ARG_NONE, &unix_socket_i, "Use IP-based transport (TCP, UDP)", NULL },
  { "unix", 'x', 0, G_OPTION_ARG_NONE, &unix_socket_x, "Use UNIX domain socket transport", NULL },
  { "stream", 'S', 0, G_OPTION_ARG_NONE, &sock_type_s, "Use stream socket (TCP and unix-stream)", NULL },
  { "ipv6", '6', 0, G_OPTION_ARG_NONE, &use_ipv6, "Use AF_INET6 sockets instead of AF_INET (can use both IPv4 & IPv6)", NULL },
  { "dgram", 'D', 0, G_OPTION_ARG_NONE, &sock_type_d, "Use datagram socket (UDP and unix-dgram)", NULL },
  { "size", 's', 0, G_OPTION_ARG_INT, &message_length, "Specify the size of the syslog message", "<size>" },
  { "interval", 'I', 0, G_OPTION_ARG_INT, &interval, "Number of seconds to run the test for", "<sec>" },
  { "syslog-proto", 'P', 0, G_OPTION_ARG_NONE, &syslog_proto, "Use the new syslog-protocol message format (see also framing)", NULL },
  { "sdata", 'p', 0, G_OPTION_ARG_STRING, &sdata_value, "Send the given sdata (e.g. \"[test name=\\\"value\\\"]) in case of syslog-proto", NULL },
  { "no-framing", 'F', G_OPTION_FLAG_REVERSE, G_OPTION_ARG_NONE, &framing, "Don't use syslog-protocol style framing, even if syslog-proto is set", NULL },
  { "active-connections", 0, 0, G_OPTION_ARG_INT, &active_connections, "Number of active connections to the server (default = 1)", "<number>" },
  { "idle-connections", 0, 0, G_OPTION_ARG_INT, &idle_connections, "Number of inactive connections to the server (default = 0)", "<number>" },
#if ENABLE_SSL
  { "use-ssl", 'U', 0, G_OPTION_ARG_NONE, &usessl, "Use ssl layer", NULL },
#endif
  { "read-file", 'R', 0, G_OPTION_ARG_STRING, &read_file, "Read log messages from file", "<filename>" },
  { "loop-reading", 'l', 0, G_OPTION_ARG_NONE, &loop_reading, "Read the file specified in read-file option in loop (it will restart the reading if reached the end of the file)", NULL },
  { "skip-tokens", 0, 0, G_OPTION_ARG_INT, &skip_tokens, "Skip the given number of tokens (delimined by a space) at the beginning of each line (default value: 3)", "<number>" },
  { "csv", 'C', 0, G_OPTION_ARG_NONE, &csv, "Produce CSV output", NULL },
  { "number", 'n', 0, G_OPTION_ARG_INT, &number_of_messages, "Number of messages to generate", "<number>" },
  { "quiet", 'Q', 0, G_OPTION_ARG_NONE, &quiet, "Don't print the msg/sec data", NULL },
  { "version",   'V', 0, G_OPTION_ARG_NONE, &display_version, "Display version number (" PACKAGE " " COMBINED_VERSION ")", NULL },
  { "rltp",   'L', 0, G_OPTION_ARG_NONE, &rltp, "RLTP transport", NULL },
  { "rltp-batch-size", 'b', 0, G_OPTION_ARG_INT, &rltp_batch_size, "Number of messages send in one rltp package (default value: 100)", NULL },
  { NULL }
};

static GOptionEntry file_option_entries[] =
{
  { "read-file", 'R', 0, G_OPTION_ARG_STRING, &read_file, "Read log messages from file", "<filename>" },
  { "loop-reading",'l',0, G_OPTION_ARG_NONE, &loop_reading, "Read the file specified in read-file option in loop (it will restart the reading if reached the end of the file)", NULL },
  { "dont-parse", 'd', 0, G_OPTION_ARG_NONE, &dont_parse, "Don't parse the lines coming from the readed files. Loggen will send the whole lines as it is in the readed file", NULL },
  { "skip-tokens", 0, 0, G_OPTION_ARG_INT, &skip_tokens, "Skip the given number of tokens (delimined by a space) at the beginning of each line (default value: 3)", "<number>" },
  { NULL }
};

void
version(void)
{
  printf(PACKAGE " " COMBINED_VERSION "\n");
}

int
main(int argc, char *argv[])
{
  int ret = 0;
  GError *error = NULL;
  GOptionContext *ctx = NULL;
  int i;
  guint64 diff_usec;
  GOptionGroup *group;

  g_thread_init(NULL);
  tzset();

  signal(SIGPIPE, SIG_IGN);

  ctx = g_option_context_new(" target port");
  g_option_context_add_main_entries(ctx, loggen_options, 0);


  group = g_option_group_new("file", "File options", "Show file options", NULL, NULL);
  g_option_group_add_entries(group, file_option_entries);
  g_option_context_add_group(ctx, group);

  if (!g_option_context_parse(ctx, &argc, &argv, &error))
    {
      fprintf(stderr, "Option parsing failed: %s\n", error->message);
      return 1;
    }

  if (display_version)
    {
      version();
      return 0;
    }

  if (active_connections <= 0)
    {
      fprintf(stderr, "Minimum value of active-connections must be greater than 0\n");
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

#if ENABLE_SSL
  if (usessl)
    {
      /* Initialize SSL library */
      OpenSSL_add_ssl_algorithms();
      if (NULL == (ssl_ctx = SSL_CTX_new(SSLv23_client_method())))
        return 1;
    }
    SSL_load_error_strings();
    ERR_load_crypto_strings();
#endif
  if (syslog_proto || rltp)
    framing = 1;

  if (read_file != NULL)
    {
      if (read_file[0] == '-' && read_file[1] == '\0')
        {
          if (active_connections > 1)
            {
              fprintf(stderr, "Warning: more than one active connection is not allowed if reading from stdin was specified. active-connections = '%d', new active-connections = '1'\n", active_connections);
              active_connections = 1;
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
          struct addrinfo *res;

          memset(&hints, 0, sizeof(hints));
          hints.ai_family = use_ipv6 ? AF_INET6 : AF_INET;
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

          dest_addr = res->ai_addr;
          dest_addr_len = res->ai_addrlen;
#else
          struct hostent *he;
          struct servent *se;
          static struct sockaddr_in s_in;

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

          dest_addr = (struct sockaddr *) &s_in;
          dest_addr_len = sizeof(s_in);
#endif
        }
    }
  else
    {
      static struct sockaddr_un saun;

      saun.sun_family = AF_UNIX;
      strncpy(saun.sun_path, argv[0], sizeof(saun.sun_path));

      dest_addr = (struct sockaddr *) &saun;
      dest_addr_len = sizeof(saun);
    }
  if (active_connections + idle_connections > 10000)
    {
      fprintf(stderr, "Loggen doesn't support more than 10k threads.\n");
      return 2;
    }
  thread_setup();
  /* used for startup & to signal inactive threads to exit */
  thread_cond = g_cond_new();
  /* active threads signal when they are ready */
  thread_finished = g_cond_new();
  thread_connected = g_cond_new();
  /* mutex used for both cond vars */
  thread_lock = g_mutex_new();
  if(csv)
    {
      printf("ThreadId;Time;Rate;Count\n");
    }

  for (i = 0; i < idle_connections; i++)
    {
      if (!g_thread_create_full(idle_thread, NULL, 1024 * 64, FALSE, FALSE, G_THREAD_PRIORITY_NORMAL, NULL))
        goto stop_and_exit;
    }
  rltp_chunk_counters = g_malloc0(active_connections * sizeof(int));
  for (i = 0; i < active_connections; i++)
    {
      if (!g_thread_create_full(active_thread, GINT_TO_POINTER(i), 1024 * 64, FALSE, FALSE, G_THREAD_PRIORITY_NORMAL, NULL))
        goto stop_and_exit;
    }

  g_mutex_lock(thread_lock);
  while (connect_finished < active_connections + idle_connections)
    g_cond_wait(thread_connected, thread_lock);

  /* tell everyone to start */
  threads_start = TRUE;
  g_cond_broadcast(thread_cond);

  /* wait until active ones finish */
  while (active_finished < active_connections)
    g_cond_wait(thread_finished, thread_lock);

  /* tell inactive ones to exit (active ones exit automatically) */
  threads_stop = TRUE;
  g_cond_broadcast(thread_cond);
  g_mutex_unlock(thread_lock);

  sum_time.tv_sec /= active_connections;
  sum_time.tv_usec /= active_connections;
  diff_usec = sum_time.tv_sec * USEC_PER_SEC + sum_time.tv_usec;

  if (diff_usec == 0)
    {
      diff_usec = 1;
    }
  fprintf(stderr, "average rate = %.2lf msg/sec, count=%"G_GUINT64_FORMAT", time=%ld.%03ld, (average) msg size=%ld, bandwidth=%.2lf kB/sec\n",
        (double)sum_count * USEC_PER_SEC / diff_usec, sum_count,
        (long int)sum_time.tv_sec, (long int)sum_time.tv_usec / 1000,
        (long int)raw_message_length, (double)sum_count * raw_message_length * (USEC_PER_SEC / 1024) / diff_usec);

stop_and_exit:
  SSL_CTX_free (ssl_ctx);
  threads_start = TRUE;
  threads_stop = TRUE;
  g_mutex_lock(thread_lock);
  g_cond_broadcast(thread_cond);
  g_mutex_unlock(thread_lock);

  return ret;
}
