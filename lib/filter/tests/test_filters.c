/*
 * Copyright (c) 2005-2018 Balabit
 * Copyright (c) 2005-2015 Balázs Scheidler <balazs.scheidler@balabit.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 */

#include "filter/filter-expr.h"
#include "filter/filter-expr-grammar.h"
#include "filter/filter-netmask.h"
#include "filter/filter-netmask6.h"
#include "filter/filter-op.h"
#include "filter/filter-cmp.h"
#include "filter/filter-tags.h"
#include "filter/filter-re.h"
#include "filter/filter-pri.h"
#include "cfg.h"
#include "messages.h"
#include "syslog-names.h"
#include "logmsg/logmsg.h"
#include "apphook.h"
#include "plugin.h"
#include "test_filters_common.h"
#include <criterion/criterion.h>

#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>


Test(filter, test_filters, .init = setup, .fini = teardown)
{

  cr_assert_eq(create_pcre_regexp_filter(LM_V_PROGRAM, "((", 0), NULL);
  cr_assert_eq(create_pcre_regexp_filter(LM_V_HOST, "((", 0), NULL);
  cr_assert_eq(create_pcre_regexp_match("((", 0), NULL);
  cr_assert_eq(create_pcre_regexp_filter(LM_V_PROGRAM, "((", 0), NULL);
  cr_assert_eq(create_pcre_regexp_filter(LM_V_HOST, "((", 0), NULL);
  cr_assert_eq(create_pcre_regexp_filter(LM_V_HOST, "(?iana", 0), NULL);
  cr_assert_eq(create_pcre_regexp_filter(LM_V_HOST, "(?iana", 0), NULL);
  cr_assert_eq(create_pcre_regexp_match("((", 0), NULL);


  testcase_with_backref_chk("<15>Oct 15 16:17:01 host openvpn[2499]: al fa", create_pcre_regexp_filter(LM_V_MESSAGE,
                            "(a)(l) (fa)", LMF_STORE_MATCHES), 1, "1","a");
  testcase_with_backref_chk("<15>Oct 15 16:17:02 host openvpn[2499]: al fa", create_pcre_regexp_filter(LM_V_MESSAGE,
                            "(a)(l) (fa)", LMF_STORE_MATCHES), 1, "0","al fa");
  testcase_with_backref_chk("<15>Oct 15 16:17:03 host openvpn[2499]: al fa", create_pcre_regexp_filter(LM_V_MESSAGE,
                            "(a)(l) (fa)", LMF_STORE_MATCHES), 1, "232", NULL);
  testcase_with_backref_chk("<15>Oct 15 16:17:01 host openvpn[2499]: alma fa", create_pcre_regexp_filter(LM_V_MESSAGE,
                            "(?P<a>a)(?P<l>l)(?P<MM>m)(?P<aa>a) (?P<fa>fa)", LMF_STORE_MATCHES), 1, "MM","m");
  testcase_with_backref_chk("<15>Oct 15 16:17:01 host openvpn[2499]: alma fa", create_pcre_regexp_filter(LM_V_MESSAGE,
                            "(?P<a>a)(?P<l>l)(?P<MM>m)(?P<aa>a) (?P<fa>fa)", LMF_STORE_MATCHES), 1, "aaaa", NULL);
  testcase_with_backref_chk("<15>Oct 15 16:17:01 host openvpn[2499]: alma fa", create_pcre_regexp_filter(LM_V_MESSAGE,
                            "(?P<a>a)(?P<l>l)(?P<MM>m)(?P<aa>a) (?P<fa_name>fa)", LMF_STORE_MATCHES), 1, "fa_name","fa");
  testcase_with_backref_chk("<15>Oct 15 16:17:04 host openvpn[2499]: al fa", create_pcre_regexp_filter(LM_V_MESSAGE,
                            "(a)(l) (fa)", LMF_STORE_MATCHES), 1, "2","l");
  testcase_with_backref_chk("<15>Oct 15 16:17:05 host openvpn[2499]: al fa", create_pcre_regexp_filter(LM_V_MESSAGE,
                            "(a)(l) (fa)", LMF_STORE_MATCHES), 1, "0","al fa");
  testcase_with_backref_chk("<15>Oct 15 16:17:06 host openvpn[2499]: al fa", create_pcre_regexp_filter(LM_V_MESSAGE,
                            "(a)(l) (fa)", LMF_STORE_MATCHES), 1, "233",NULL);

  testcase("<15> openvpn[2499]: PTHREAD support initialized", filter_level_new(level_bits("debug") | level_bits("emerg")),
           TRUE);
  testcase("<15> openvpn[2499]: PTHREAD support initialized", filter_level_new(level_bits("emerg")), FALSE);

  testcase("<8> openvpn[2499]: PTHREAD support initialized", filter_level_new(level_range("crit", "emerg")), TRUE);
  testcase("<9> openvpn[2499]: PTHREAD support initialized", filter_level_new(level_range("crit", "emerg")), TRUE);
  testcase("<10> openvpn[2499]: PTHREAD support initialized", filter_level_new(level_range("crit", "emerg")), TRUE);
  testcase("<11> openvpn[2499]: PTHREAD support initialized", filter_level_new(level_range("crit", "emerg")), FALSE);
  testcase("<12> openvpn[2499]: PTHREAD support initialized", filter_level_new(level_range("crit", "emerg")), FALSE);
  testcase("<13> openvpn[2499]: PTHREAD support initialized", filter_level_new(level_range("crit", "emerg")), FALSE);
  testcase("<14> openvpn[2499]: PTHREAD support initialized", filter_level_new(level_range("crit", "emerg")), FALSE);
  testcase("<15> openvpn[2499]: PTHREAD support initialized", filter_level_new(level_range("crit", "emerg")), FALSE);

  testcase("<8> openvpn[2499]: PTHREAD support initialized", filter_level_new(level_range("debug", "notice")), FALSE);
  testcase("<9> openvpn[2499]: PTHREAD support initialized", filter_level_new(level_range("debug", "notice")), FALSE);
  testcase("<10> openvpn[2499]: PTHREAD support initialized", filter_level_new(level_range("debug", "notice")), FALSE);
  testcase("<11> openvpn[2499]: PTHREAD support initialized", filter_level_new(level_range("debug", "notice")), FALSE);
  testcase("<12> openvpn[2499]: PTHREAD support initialized", filter_level_new(level_range("debug", "notice")), FALSE);
  testcase("<13> openvpn[2499]: PTHREAD support initialized", filter_level_new(level_range("debug", "notice")), TRUE);
  testcase("<14> openvpn[2499]: PTHREAD support initialized", filter_level_new(level_range("debug", "notice")), TRUE);
  testcase("<15> openvpn[2499]: PTHREAD support initialized", filter_level_new(level_range("debug", "notice")), TRUE);

  testcase("<0> openvpn[2499]: PTHREAD support initialized", filter_level_new(level_bits("emerg")), TRUE);
  testcase("<1> openvpn[2499]: PTHREAD support initialized", filter_level_new(level_bits("alert")), TRUE);
  testcase("<2> openvpn[2499]: PTHREAD support initialized", filter_level_new(level_bits("crit")), TRUE);
  testcase("<3> openvpn[2499]: PTHREAD support initialized", filter_level_new(level_bits("err")), TRUE);
  testcase("<4> openvpn[2499]: PTHREAD support initialized", filter_level_new(level_bits("warning")), TRUE);
  testcase("<5> openvpn[2499]: PTHREAD support initialized", filter_level_new(level_bits("notice")), TRUE);
  testcase("<6> openvpn[2499]: PTHREAD support initialized", filter_level_new(level_bits("info")), TRUE);
  testcase("<7> openvpn[2499]: PTHREAD support initialized", filter_level_new(level_bits("debug")), TRUE);

  testcase("<15> openvpn[2499]: PTHREAD support initialized", create_pcre_regexp_filter(LM_V_PROGRAM, "^openvpn$", 0),
           TRUE);
  testcase("<15> openvpn[2499]: PTHREAD support initialized", create_pcre_regexp_filter(LM_V_PROGRAM, "^open$", 0),
           FALSE);

  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", create_pcre_regexp_filter(LM_V_HOST,
           "^host$", 0), TRUE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", create_pcre_regexp_filter(LM_V_HOST,
           "^hos$", 0), FALSE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", create_pcre_regexp_filter(LM_V_HOST,
           "pthread", 0), FALSE);


  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", create_pcre_regexp_filter(LM_V_MESSAGE,
           "^PTHREAD ", 0), TRUE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", create_pcre_regexp_filter(LM_V_MESSAGE,
           "PTHREAD s", 0), TRUE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", create_pcre_regexp_filter(LM_V_MESSAGE,
           "^PTHREAD$", 0), FALSE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", create_pcre_regexp_filter(LM_V_MESSAGE,
           "(?i)pthread", 0), TRUE);


  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", create_pcre_regexp_match(" PTHREAD ",
           0), TRUE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized",
           create_pcre_regexp_match("^openvpn\\[2499\\]: PTHREAD", 0), TRUE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", create_pcre_regexp_match("^PTHREAD$",
           0), FALSE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", create_pcre_regexp_match("(?i)pthread",
           0), TRUE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", create_pcre_regexp_match("pthread",
           LMF_ICASE), TRUE);


#if SYSLOG_NG_ENABLE_IPV6
  testcase_with_socket("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized",
                       "2001:db80:85a3:8d30:1319:8a2e:3700:7348", filter_netmask6_new("::/1"), TRUE);
  testcase_with_socket("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized",
                       "2001:db80:85a3:8d30:1319:8a2e:3700:7348",
                       filter_netmask6_new("2001:db80:85a3:8d30:1319:8a2e::/95"), TRUE);

  testcase_with_socket("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized",
                       "2001:db80:85a3:8d30:1319:8a2e:3700:7348",
                       filter_netmask6_new("2001:db80:85a3:8d30:1319:8a2e:3700:7348/60"), TRUE);

  testcase_with_socket("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized",
                       "2001:db80:85a3:8d30:1319:8a2e:3700:7348",
                       filter_netmask6_new("2001:db80:85a3:8d30:1319:8a2e:3700::/114"), FALSE);
  testcase_with_socket("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized",
                       "2001:db80:85a3:8d30:1319:8a2e:3700:7348",
                       filter_netmask6_new("::85a3:8d30:1319:8a2e:3700::/114"), FALSE);

  testcase_with_socket("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized",
                       "2001:db80:85a3:8d30:1319:8a2e:3700:7348", filter_netmask6_new("aaaaaa/32"), FALSE);
  testcase_with_socket("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized",
                       "2001:db80:85a3:8d30:1319:8a2e:3700:7348", filter_netmask6_new("/8"), FALSE);
  testcase_with_socket("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized",
                       "2001:db80:85a3:8d30:1319:8a2e:3700:7348", filter_netmask6_new("::"), FALSE);
  testcase_with_socket("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized",
                       "2001:db80:85a3:8d30:1319:8a2e:3700:7348", filter_netmask6_new(""), FALSE);
  testcase_with_socket("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized",
                       "2001:db80:85a3:8d30:1319:8a2e:3700:7348", filter_netmask6_new("::1/8"), FALSE);
  testcase_with_socket("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized",
                       "2001:db80:85a3:8d30:1319:8a2e:3700:7348", filter_netmask6_new("::1/128"), FALSE);
  testcase_with_socket("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized",
                       "2001:db80:85a3:8d30:1319:8a2e:3700:7348", filter_netmask6_new("::2/32"), FALSE);

  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", filter_netmask6_new("aaaaaa/32"),
           FALSE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", filter_netmask6_new("/8"), FALSE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", filter_netmask6_new(""), FALSE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", filter_netmask6_new("::1"), TRUE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", filter_netmask6_new("::/32"), TRUE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", filter_netmask6_new("::1/8"), TRUE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", filter_netmask6_new("::1/128"), TRUE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", filter_netmask6_new("::/16"), TRUE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", filter_netmask6_new("::/599"), FALSE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", filter_netmask6_new("::/aaa"), FALSE);
#endif

  testcase_with_socket("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", "10.10.0.1",
                       filter_netmask_new("10.10.0.0/16"), TRUE);
  testcase_with_socket("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", "10.10.0.1",
                       filter_netmask_new("10.10.0.0/24"), TRUE);
  testcase_with_socket("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", "10.10.0.1",
                       filter_netmask_new("10.10.10.0/24"), FALSE);
  testcase_with_socket("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", "10.10.0.1",
                       filter_netmask_new("0.0.10.10/24"), FALSE);

  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", filter_netmask_new("127.0.0.1/32"),
           TRUE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", filter_netmask_new("127.0.0.2/32"),
           FALSE);

  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized",
           fop_or_new(create_pcre_regexp_match(" PTHREAD ", 0), create_pcre_regexp_match("PTHREAD", 0)), TRUE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized",
           fop_or_new(create_pcre_regexp_match(" PTHREAD ", 0), create_pcre_regexp_match("^PTHREAD$", 0)), TRUE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized",
           fop_or_new(create_pcre_regexp_match("^PTHREAD$", 0), create_pcre_regexp_match(" PTHREAD ", 0)), TRUE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized",
           fop_or_new(create_pcre_regexp_match(" PAD ", 0), create_pcre_regexp_match("^PTHREAD$", 0)), FALSE);

  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized",
           fop_and_new(create_pcre_regexp_match(" PTHREAD ", 0), create_pcre_regexp_match("PTHREAD", 0)), TRUE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized",
           fop_and_new(create_pcre_regexp_match(" PTHREAD ", 0), create_pcre_regexp_match("^PTHREAD$", 0)), FALSE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized",
           fop_and_new(create_pcre_regexp_match("^PTHREAD$", 0), create_pcre_regexp_match(" PTHREAD ", 0)), FALSE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized",
           fop_and_new(create_pcre_regexp_match(" PAD ", 0), create_pcre_regexp_match("^PTHREAD$", 0)), FALSE);

  /* LEVEL_NUM is 7 */
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized",
           fop_cmp_new(create_template("$LEVEL_NUM"), create_template("7"), KW_NUM_EQ), TRUE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized",
           fop_cmp_new(create_template("$LEVEL_NUM"), create_template("5"), KW_NUM_NE), FALSE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized",
           fop_cmp_new(create_template("$LEVEL_NUM"), create_template("8"), KW_NUM_LT), TRUE);

  /* 7 < 10 is TRUE */
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized",
           fop_cmp_new(create_template("$LEVEL_NUM"), create_template("10"), KW_NUM_LT), TRUE);
  /* 7 lt 10 is FALSE as 10 orders lower when interpreted as a string */
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized",
           fop_cmp_new(create_template("$LEVEL_NUM"), create_template("10"), KW_LT), FALSE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized",
           fop_cmp_new(create_template("$LEVEL_NUM"), create_template("5"), KW_NUM_GT), TRUE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized",
           fop_cmp_new(create_template("$LEVEL_NUM"), create_template("7"), KW_NUM_GE), TRUE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized",
           fop_cmp_new(create_template("$LEVEL_NUM"), create_template("7"), KW_NUM_LE), TRUE);

  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", fop_cmp_new(create_template("alma"),
           create_template("korte"), KW_LT), TRUE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", fop_cmp_new(create_template("alma"),
           create_template("korte"), KW_LE), TRUE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", fop_cmp_new(create_template("alma"),
           create_template("korte"), KW_EQ), FALSE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", fop_cmp_new(create_template("alma"),
           create_template("korte"), KW_NE), TRUE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", fop_cmp_new(create_template("alma"),
           create_template("korte"), KW_GE), FALSE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", fop_cmp_new(create_template("alma"),
           create_template("korte"), KW_GT), FALSE);

  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", fop_cmp_new(create_template("korte"),
           create_template("alma"), KW_LT), FALSE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", fop_cmp_new(create_template("korte"),
           create_template("alma"), KW_LE), FALSE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", fop_cmp_new(create_template("korte"),
           create_template("alma"), KW_EQ), FALSE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", fop_cmp_new(create_template("korte"),
           create_template("alma"), KW_NE), TRUE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", fop_cmp_new(create_template("korte"),
           create_template("alma"), KW_GE), TRUE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", fop_cmp_new(create_template("korte"),
           create_template("alma"), KW_GT), TRUE);

  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", fop_cmp_new(create_template("alma"),
           create_template("alma"), KW_LT), FALSE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", fop_cmp_new(create_template("alma"),
           create_template("alma"), KW_LE), TRUE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", fop_cmp_new(create_template("alma"),
           create_template("alma"), KW_EQ), TRUE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", fop_cmp_new(create_template("alma"),
           create_template("alma"), KW_NE), FALSE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", fop_cmp_new(create_template("alma"),
           create_template("alma"), KW_GE), TRUE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", fop_cmp_new(create_template("alma"),
           create_template("alma"), KW_GT), FALSE);




  testcase("<15> openvpn[2499]: PTHREAD support initialized", create_pcre_regexp_filter(LM_V_PROGRAM, "^openvpn$", 0),
           TRUE);
  testcase("<15> openvpn[2499]: PTHREAD support initialized", create_pcre_regexp_filter(LM_V_PROGRAM, "^open$", 0),
           FALSE);

  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", create_pcre_regexp_filter(LM_V_HOST,
           "^host$", 0), TRUE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", create_pcre_regexp_filter(LM_V_HOST,
           "^hos$", 0), FALSE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", create_pcre_regexp_filter(LM_V_HOST,
           "pthread", 0), FALSE);

  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", create_pcre_regexp_filter(LM_V_MESSAGE,
           "^PTHREAD ", 0), TRUE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", create_pcre_regexp_filter(LM_V_MESSAGE,
           "PTHREAD s", 0), TRUE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", create_pcre_regexp_filter(LM_V_MESSAGE,
           "^PTHREAD$", 0), FALSE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", create_pcre_regexp_filter(LM_V_MESSAGE,
           "(?i)pthread", 0), TRUE);

  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized",
           fop_and_new(create_pcre_regexp_match(" PTHREAD ", 0), create_pcre_regexp_match("PTHREAD", 0)), TRUE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized",
           fop_and_new(create_pcre_regexp_match(" PTHREAD ", 0), create_pcre_regexp_match("^PTHREAD$", 0)), FALSE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized",
           fop_and_new(create_pcre_regexp_match("^PTHREAD$", 0), create_pcre_regexp_match(" PTHREAD ", 0)), FALSE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized",
           fop_and_new(create_pcre_regexp_match(" PAD ", 0), create_pcre_regexp_match("^PTHREAD$", 0)), FALSE);

  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized",
           fop_or_new(create_pcre_regexp_match(" PTHREAD ", 0), create_pcre_regexp_match("PTHREAD", 0)), TRUE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized",
           fop_or_new(create_pcre_regexp_match(" PTHREAD ", 0), create_pcre_regexp_match("^PTHREAD$", 0)), TRUE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized",
           fop_or_new(create_pcre_regexp_match("^PTHREAD$", 0), create_pcre_regexp_match(" PTHREAD ", 0)), TRUE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized",
           fop_or_new(create_pcre_regexp_match(" PAD ", 0), create_pcre_regexp_match("^PTHREAD$", 0)), FALSE);

  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", create_pcre_regexp_match(" PTHREAD ",
           0), TRUE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized",
           create_pcre_regexp_match("^openvpn\\[2499\\]: PTHREAD", 0), TRUE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", create_pcre_regexp_match("^PTHREAD$",
           0), FALSE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", create_pcre_regexp_match("(?i)pthread",
           0), TRUE);
  testcase("<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", create_pcre_regexp_match("pthread",
           LMF_ICASE), TRUE);
}
