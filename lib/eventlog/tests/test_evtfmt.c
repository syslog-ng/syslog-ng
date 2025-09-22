#include "evtlog.h"
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>

int
main(void)
{
  EVTCONTEXT *ctx;
  EVTREC *e;
  char *es;

  ctx = evt_ctx_init("evtfmt", LOG_AUTH);
  e = evt_rec_init(ctx, LOG_INFO, "Test message with an embedded ';' in it. It also contains an <XML> like tag.");
  evt_rec_add_tags(e,
                   evt_tag_str("test:tag", "'value'"),
                   evt_tag_str("test:tag2", "\n\n\n\n"),
                   evt_tag_int("test:fd", fileno(stderr)),
                   evt_tag_errno("test:error", EAGAIN),
                   evt_tag_printf("test:printf", "%d %d", 5, 6),
                   NULL);

  es = evt_format(e);
  printf("%s\n", es);
  free(es);

  evt_log(e);
  evt_ctx_free(ctx);
  return 0;
}
