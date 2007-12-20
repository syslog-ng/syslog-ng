#include "apphook.h"
#include "messages.h"
#include "children.h"
#include "dnscache.h"

typedef struct _ApplicationHookEntry
{
  gint type;
  ApplicationHookFunc func;
  gpointer user_data;
} ApplicationHookEntry;

static GList *application_hooks = NULL;
static gint current_state = AH_STARTUP;

void 
register_application_hook(gint type, ApplicationHookFunc func, gpointer user_data)
{
  if (current_state < type)
    {
      ApplicationHookEntry *entry = g_new0(ApplicationHookEntry, 1);
      
      entry->type = type;
      entry->func = func;
      entry->user_data = user_data;
      
      application_hooks = g_list_prepend(application_hooks, entry);
    }
  else
    {
      /* the requested hook has already passed, call the requested function immediately */
      msg_debug("Application hook registered after the given point passed", 
                evt_tag_int("current", current_state), 
                evt_tag_int("hook", type), NULL);
      func(type, user_data);
    }
}

static void
run_application_hook(gint type)
{
  GList *l;
  
  g_assert(current_state < type);
  
  msg_debug("Running application hooks", evt_tag_int("hook", type), NULL);
  current_state = type;
  for (l = application_hooks; l; l = l->next)
    {
      ApplicationHookEntry *e = l->data;
      
      if (e->type == type)
        {
          e->func(type, e->user_data);
          g_free(e);
        }
    }

}

void 
app_startup(void)
{
  msg_init();
  child_manager_init();
  dns_cache_init();
  tzset();
}

void
app_post_daemonized(void)
{
  run_application_hook(AH_POST_DAEMONIZED);
}

void 
app_shutdown(void)
{
  run_application_hook(AH_SHUTDOWN);
  dns_cache_destroy();
  child_manager_deinit();
  msg_deinit();
  g_list_free(application_hooks);
}

