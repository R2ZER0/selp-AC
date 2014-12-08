/* squarestats_event_json_test.c */

#include <stdlib.h>
#include <check.h>
#include "squarestats_event_json.h"

START_TEST(test_frag)
{
        const char* json = event_kill_json(
                "k1ll3r", "v!icTim",
                "shotgun", 12345,
                0, 0
        );
        
        ck_assert_str_eq(json, "{ \"victim\": \"k1ll3r\" }");
        
        free((void*) json);
}
END_TEST

START_TEST(test_fail)
{
        ck_assert(0);
}
END_TEST

Suite* event_json_suite(void)
{
        Suite* suite = suite_create("event_json");
        
        TCase* tcase_core = tcase_create("core");
        
        tcase_add_test(tcase_core, test_frag);
        tcase_add_test(tcase_core, test_fail);
        
        suite_add_tcase(suite, tcase_core);
        
        return suite;
}

int main(void)
{
        Suite* suite = event_json_suite();
        SRunner* srunner = srunner_create(suite);
        
        srunner_run_all(srunner, CK_NORMAL);
        int number_failed = srunner_ntests_failed(srunner);
        
        srunner_free(srunner);
        srunner = NULL;
        
        return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}