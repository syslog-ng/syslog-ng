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

#if HAVE_GETOPT_H
#include <getopt.h>
#endif

#define POS_TIMESTAMP1   4
#define POS_SEQ          49
#define POS_TIMESTAMP2   87

#define MAX_MESSAGE_LENGTH 8192

#define USEC_PER_SEC     10e6
#define MIN(a, b)    ((a) < (b) ? (a) : (b))

static inline unsigned long
time_val_diff(struct timeval *t1, struct timeval *t2)
{
  return (t1->tv_sec - t2->tv_sec) * USEC_PER_SEC + (t1->tv_usec - t2->tv_usec);
}

void
gen_messages(int sock, int rate, int message_length, int interval)
{
  struct timeval now, start, last_ts_format, last_throttle_check;
  char linebuf[MAX_MESSAGE_LENGTH + 1];
  char stamp[32];
  char intbuf[16];
  int linelen;
  int i, run_id;
  unsigned long count = 0, last_count = 0;
  char padding[] = "PADD";
  long buckets = rate - (rate / 10);
  double diff;
  
  gettimeofday(&start, NULL);
  now = start;
  last_throttle_check = now;
  run_id = start.tv_sec;

  /* force reformat of the timestamp */
  last_ts_format = now;
  last_ts_format.tv_sec--;
  
  linelen = snprintf(linebuf, sizeof(linebuf), "<38>2007-12-24T12:28:51 localhost localprg: seq: %010d, runid: %-10d, stamp: %-19s ", 0, run_id, "");
  
  for (i = linelen; i < message_length - 1; i++)
    {
      linebuf[i] = padding[(i - linelen) % (sizeof(padding) - 1)];
    }
  linebuf[message_length - 1] = '\n';
  linebuf[message_length] = 0;
  linelen = strlen(linebuf);
  
  while (now.tv_sec - start.tv_sec < interval)
    {
      gettimeofday(&now, NULL);
      
      if (now.tv_sec != last_ts_format.tv_sec)
        {
          int len;
          struct tm *tm;
          
          tm = localtime(&now.tv_sec);
          len = strftime(stamp, sizeof(stamp), "%Y-%m-%dT%H:%M:%S", tm);
          memcpy(&linebuf[POS_TIMESTAMP1], stamp, len);
          memcpy(&linebuf[POS_TIMESTAMP2], stamp, len);

          diff = time_val_diff(&now, &last_ts_format);
          
          fprintf(stderr, "rate = %.2lf msg/sec              \r", ((double) (count - last_count) * USEC_PER_SEC) / diff);
          last_ts_format = now;
          last_count = count;

        }

      diff = time_val_diff(&now, &last_throttle_check);
      if (buckets == 0 || diff > 10e5)
        {
          /* check rate every 0.1sec */
          long new_buckets;
          
          new_buckets = (rate * diff) / USEC_PER_SEC;
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
          tspec.tv_nsec = (msec % 1000) * 10e6;
          while (nanosleep(&tspec, &trem) < 0 && errno == EINTR)
            {
              tspec = trem;
            }
          /* recalculate buckets */
          continue;
        }

      /* add sequence number */
      snprintf(intbuf, sizeof(intbuf), "%010ld", count);
      memcpy(&linebuf[POS_SEQ], intbuf, 10);
          
      if (write(sock, linebuf, linelen) < 0)
        {
          fprintf(stderr, "Send error %s\n", strerror(errno));
          break;
        }
      buckets--;
      count++;
    }
  close(sock);
  
  gettimeofday(&now, NULL);
  diff = (((double) now.tv_sec) + ((double) now.tv_usec / 10e6)) - ((double) start.tv_sec + ((double) now.tv_usec / 10e6));
  fprintf(stderr, "average rate = %.2lf msg/sec, count=%ld\n", (double) count / diff, count);
}

void
usage()
{
  printf("loggen [options] target [port]\n"
         "Generate syslog messages at the specified rate\n\n"
         "Options:\n"
         "  --rate, or -r <msg/sec> Number of messages to generate per second\n"
         "  --inet, or -i           Use IP based transport (TCP, UDP)\n"
         "  --unix, or -x           Use UNIX domain socket transport\n"
         "  --stream, or -S         Use stream socket (TCP and unix-stream)\n"
         "  --dgram, or -D          Use datagram socket (UDP and unix-dgram)\n"
         "  --size, or -s <size>    Specify the size of the syslog message\n"
         "  --interval, or -I <sec> Number of seconds to run the test for\n");
  exit(0);
}

int 
main(int argc, char *argv[])
{
#if HAVE_GETOPT_LONG
  struct option syslog_ng_options[] = 
    {
      { "rate", required_argument, NULL, 'r' },
      { "inet", no_argument, NULL, 'i' },
      { "unix", no_argument, NULL, 'x' },
      { "stream", no_argument, NULL, 'S' },
      { "dgram", no_argument, NULL, 'D' },
      { "size", required_argument, NULL, 's' },
      { "interval", required_argument, NULL, 'I' },
      { NULL, 0, NULL, 0 }
    };
#endif
  int rate = 1000;
  int unix_socket = 0;
  int sock_type = SOCK_STREAM;
  int sock = -1;
  int message_length = 256;
  int interval = 10;
  int opt;

#if HAVE_GETOPT_LONG
  while ((opt = getopt_long(argc, argv, "r:I:ixs:SDh", syslog_ng_options, NULL)) != -1)
#else
  while ((opt = getopt(argc, argv, "r:I:ixs:SDh")) != -1)
#endif
    {
      switch (opt) 
	{
	case 'r':
	  rate = atoi(optarg);
	  break;
        case 'I':
          interval = atoi(optarg);
          break;
        case 'i':
          unix_socket = 0;
          break;
        case 'x':
          unix_socket = 1;
          break;
        case 'D':
          sock_type = SOCK_DGRAM;
          break;
        case 'S':
          sock_type = SOCK_STREAM;
          break;
        case 's':
          message_length = atoi(optarg);
          break;
        case 'h':
          usage();
          break;
	}
    }
  if (!unix_socket)
    {

      if (argc - optind < 2)
        {
          fprintf(stderr, "No target server specified\n");
          usage();
        }

      if (1)
        {      
#if HAVE_GETADDRINFO
          struct addrinfo hints;
          struct addrinfo *res, *rp;

          memset(&hints, 0, sizeof(hints));
          hints.ai_family = AF_UNSPEC;
          hints.ai_socktype = sock_type;
          hints.ai_flags = AI_ADDRCONFIG;
          hints.ai_protocol = 0;
          if (getaddrinfo(argv[optind], argv[optind + 1], &hints, &res) != 0)
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
          struct sockaddr_in s_in;
          
          he = gethostbyname(argv[optind]);
          if (!he)
            {
              fprintf(stderr, "Name lookup error\n");
              return 2;
            }
          s_in.sin_family = AF_INET;
          s_in.sin_port = htons(atoi(argv[optind + 1]));
          s_in.sin_addr = *(struct in_addr *) he->h_addr;
          
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
      strncpy(saun.sun_path, argv[optind], sizeof(saun.sun_path));
      if (connect(sock, (struct sockaddr *) &saun, sizeof(saun)) < 0)
        {
          fprintf(stderr, "Error connecting to target server: %s\n", strerror(errno));
          return 2;
        }
    }
  
  gen_messages(sock, rate, message_length, interval);
  return 0;
}
