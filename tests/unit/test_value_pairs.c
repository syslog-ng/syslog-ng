#include "value-pairs.h"
#include "logmsg.h"
#include "apphook.h"
#include "cfg.h"
#include "plugin.h"

#include <stdlib.h>

gboolean
cat_vp_keys_foreach(const gchar  *name, const gchar *value, gpointer user_data)
{
  GString *res = (GString *) user_data;

  if (res->len > 0)
    g_string_append_c(res, ',');
  g_string_append(res, name);
  return FALSE;
}

MsgFormatOptions parse_options;

LogMessage *
create_message(void)
{
  const gchar *text = "<134>1 2009-10-16T11:51:56+02:00 exchange.macartney.esbjerg MSExchange_ADAccess 20208 _MSGID_ [origin ip=\"exchange.macartney.esbjerg\"][meta sequenceId=\"191732\" sysUpTime=\"68807696\"][EventData@18372.4 Data=\"MSEXCHANGEOWAAPPPOOL.CONFIG\\\" -W \\\"\\\" -M 1 -AP \\\"MSEXCHANGEOWAAPPPOOL5244fileserver.macartney.esbjerg CDG 1 7 7 1 0 1 1 7 1 mail.macartney.esbjerg CDG 1 7 7 1 0 1 1 7 1 maindc.macartney.esbjerg CD- 1 6 6 0 0 1 1 6 1 \"][Keywords@18372.4 Keyword=\"Classic\"] ApplicationMSExchangeADAccess: message";
  return log_msg_new(text, strlen(text), NULL, &parse_options);
}

void
testcase(const gchar *scope, const gchar *exclude, const gchar *expected)
{
  ValuePairs *vp;
  GString *vp_keys, *arg_keys;
  LogMessage *msg = create_message();

  vp_keys = g_string_sized_new(0);
  arg_keys = g_string_sized_new(0);

  vp = value_pairs_new();
  value_pairs_add_scope(vp, scope);
  if (exclude)
    value_pairs_add_exclude_glob(vp, exclude);
  value_pairs_foreach(vp, cat_vp_keys_foreach, msg, 0, vp_keys);

  if (strcmp(vp_keys->str, expected) != 0)
    {
      fprintf(stderr, "Scope keys mismatch, scope=[%s], exclude=[%s], value=[%s], expected=[%s]\n", scope, exclude ? exclude : "(none)", vp_keys->str, expected);
      exit(1);
    }
  g_string_free(vp_keys, TRUE);
  g_string_free(arg_keys, TRUE);
  log_msg_unref(msg);
}

int
main(int argc, char *argv[])
{
  app_startup();
  putenv("TZ=MET-1METDST");
  tzset();

  configuration = cfg_new(0x0302);
  plugin_load_module("syslogformat", configuration, NULL);
  msg_format_options_defaults(&parse_options);
  msg_format_options_init(&parse_options, configuration);
  parse_options.flags |= LP_SYSLOG_PROTOCOL;

  /* FIXME: we should probably sort the key names for better reliablity */
  testcase("rfc3164", NULL, "FACILITY,PROGRAM,PID,PRIORITY,DATE,MSG,HOST");
  testcase("rfc5424", NULL, "FACILITY,PROGRAM,PID,PRIORITY,DATE,MSG,MSGID,SDATA,HOST");
  testcase("selected-macros", NULL, "FACILITY,PROGRAM,PID,PRIORITY,DATE,TAGS,MSG,SDATA,HOST,SOURCEIP,SEQNUM");
  testcase("nv-pairs", NULL, "PROGRAM,PID,.SDATA.meta.sysUpTime,MSGID,.SDATA.EventData@18372.4.Data,.SDATA.origin.ip,.SDATA.Keywords@18372.4.Keyword,MESSAGE,.SDATA.meta.sequenceId,HOST");
  testcase("everything", NULL, "MESSAGE,MONTH_WEEK,R_SEC,DAY,S_SEC,S_TZOFFSET,PRIORITY,WEEK,TZOFFSET,R_WEEK_DAY,S_HOUR,S_UNIXTIME,LEVEL_NUM,MONTH,TAG,WEEKDAY,UNIXTIME,S_FULLDATE,FULLDATE,R_WEEK_DAY_ABBREV,YEAR_DAY,FACILITY_NUM,MONTH_NAME,MIN,S_WEEK,R_ISODATE,R_MONTH_WEEK,S_WEEK_DAY_NAME,YEAR,R_HOUR,S_MONTH,R_MONTH,S_WEEK_DAY,PROGRAM,WEEK_DAY,WEEK_DAY_NAME,S_WEEKDAY,HOST,R_WEEK,S_YEAR,S_MONTH_WEEK,ISODATE,MSG,SEC,SOURCEIP,R_MONTH_NAME,MONTH_ABBREV,FACILITY,.SDATA.meta.sequenceId,R_MONTH_ABBREV,PRI,.SDATA.origin.ip,DATE,.SDATA.meta.sysUpTime,R_YEAR,MSGHDR,R_TZ,S_MONTH_NAME,.SDATA.Keywords@18372.4.Keyword,TAGS,S_ISODATE,MSGID,S_DATE,SDATA,R_DAY,BSDTAG,.SDATA.EventData@18372.4.Data,S_DAY,S_TZ,SEQNUM,R_WEEK_DAY_NAME,STAMP,LEVEL,R_DATE,R_YEAR_DAY,TZ,R_MIN,PID,S_MIN,R_TZOFFSET,S_MONTH_ABBREV,R_UNIXTIME,S_WEEK_DAY_ABBREV,WEEK_DAY_ABBREV,R_FULLDATE,S_STAMP,R_STAMP,R_WEEKDAY,HOUR,S_YEAR_DAY");

  testcase("nv-pairs", ".SDATA.*", "PROGRAM,PID,MSGID,MESSAGE,HOST");

  app_shutdown();
}
