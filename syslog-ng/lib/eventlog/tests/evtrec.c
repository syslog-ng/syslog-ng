#include "evtlog.h"
#include <stdio.h>
#include <errno.h>

int
main(void)
{
  EVTREC *e;
  EVTCONTEXT *ctx;

  ctx = evt_ctx_init("evtrec", LOG_AUTH);
  e = evt_rec_init(ctx, LOG_INFO, "Test message");
  evt_rec_add_tags(e,
                   evt_tag_str("test:tag", "value"),
                   evt_tag_int("test:fd", fileno(stderr)),
                   evt_tag_errno("test:error", EAGAIN),
                   NULL);
  evt_log(e);
  evt_ctx_free(ctx);
  return 0;
}
