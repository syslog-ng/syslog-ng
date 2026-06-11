#include <check.h>
#include <stdlib.h>
#include <string.h>

#include "syslog-format.h"
#include "cfg.h"
#include "msg-format.h"
#include "logmsg/logmsg.h"

/* Invariant: Parsing untrusted syslog messages with oversized structured data
 * fields must not overflow the sd_value_name buffer. The parser must handle
 * attacker-controlled input (which arrives without authentication on syslog
 * ports) safely regardless of SD-ID/SD-PARAM length. */

START_TEST(test_sdata_prefix_overflow_no_auth)
{
    /* These represent unauthenticated syslog messages arriving on a network port.
     * The structured data fields are attacker-controlled. */
    const char *payloads[] = {
        /* Exploit case: oversized SD-ID to overflow sd_value_name buffer */
        "<165>1 2023-01-01T00:00:00Z host app - - "
        "[" "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        " overflow=\"yes\"] msg",
        /* Boundary: SD-PARAM name at exactly buffer boundary */
        "<165>1 2023-01-01T00:00:00Z host app - - "
        "[sdid " "BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB"
        "BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB"
        "=\"val\"] msg",
        /* Valid input: normal RFC5424 message */
        "<165>1 2023-01-01T00:00:00Z host app - - "
        "[exampleSDID@32473 iut=\"3\" eventSource=\"App\"] normal msg",
    };
    int num_payloads = sizeof(payloads) / sizeof(payloads[0]);

    GlobalConfig *cfg = cfg_new_snippet();
    MsgFormatOptions options;
    msg_format_options_defaults(&options);
    options.flags |= LP_SYSLOG_PROTOCOL;
    msg_format_options_init(&options, cfg);

    for (int i = 0; i < num_payloads; i++) {
        LogMessage *msg = log_msg_new_empty();
        /* Parse should not crash/overflow - unauthenticated input must be safe */
        syslog_format_handler(&options, msg, (const guchar *)payloads[i],
                              strlen(payloads[i]));
        /* Message must be created without segfault; valid parse or error flag */
        ck_assert_ptr_nonnull(msg);
        log_msg_unref(msg);
    }

    msg_format_options_destroy(&options);
    cfg_free(cfg);
}
END_TEST

Suite *security_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("Security");
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_sdata_prefix_overflow_no_auth);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = security_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}