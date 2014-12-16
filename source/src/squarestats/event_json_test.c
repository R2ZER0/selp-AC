/* event_json_test.c */

#include <stdlib.h>
#include <check.h>
#include "cJSON.h"
#include "event_json.h"

/*******************************************************************************
 * JSON-checking utility functions
 ******************************************************************************/

void ck_json_item_string(cJSON* obj, const char* item, const char* str)
{
        cJSON* jitem = cJSON_GetObjectItem(obj, item);
        ck_assert(jitem != NULL);
        ck_assert(jitem->type == cJSON_String);
        ck_assert_str_eq(str, jitem->valuestring);
}

void ck_json_item_int(cJSON* obj, const char* item, int num)
{
        cJSON* jitem = cJSON_GetObjectItem(obj, item);
        ck_assert(jitem != NULL);
        ck_assert(jitem->type == cJSON_Number);
        ck_assert_int_eq(num, jitem->valueint);
}

void ck_json_item_true(cJSON* obj, const char* item)
{
        cJSON* jitem = cJSON_GetObjectItem(obj, item);
        ck_assert(jitem != NULL);
        ck_assert(jitem->type == cJSON_True);
}

void ck_json_item_false(cJSON* obj, const char* item)
{
        cJSON* jitem = cJSON_GetObjectItem(obj, item);
        ck_assert(jitem != NULL);
        ck_assert(jitem->type == cJSON_False);
}

/*******************************************************************************
 * Tests
 ******************************************************************************/

START_TEST(test_frag)
{
        const char* json = event_kill_json(
                "k1ll3r", "v!icTim",
                "shotgun", 12345,
                0, 0
        );
        
        cJSON* obj = cJSON_Parse(json);
        
        ck_json_item_string(obj, "killer", "k1ll3r");
        ck_json_item_string(obj, "victim", "v!icTim");
        ck_json_item_string(obj, "weapon", "shotgun");
        ck_json_item_int(obj, "time", 12345);
        ck_json_item_false(obj, "gib");
        ck_json_item_false(obj, "suicide");
        
        /* We do not test that killer != victim when not suicide, as it is
         * possible that two different people with the same name killed one
         * another, so this situation is valid. */

        cJSON_Delete(obj);
        free((void*) json);
}
END_TEST

START_TEST(test_gib)
{
        const char* json = event_kill_json(
                "k1ll3r", "v!icTim",
                "shotgun", 12345,
                1, 0
        );
        
        cJSON* obj = cJSON_Parse(json);
        
        ck_json_item_true(obj, "gib");
        
        cJSON_Delete(obj);
        free((void*) json);
}
END_TEST

START_TEST(test_suicide)
{
        const char* json = event_kill_json(
                "idiot", "idiot",
                "grenade", 12345,
                0, 1
        );
        
        cJSON* obj = cJSON_Parse(json);
        
        ck_json_item_true(obj, "suicide");
        
        cJSON_Delete(obj);
        free((void*) json);
}
END_TEST

Suite* event_json_suite(void)
{
        TCase* tcase_core = tcase_create("core");
        tcase_add_test(tcase_core, test_frag);
        tcase_add_test(tcase_core, test_gib);
        tcase_add_test(tcase_core, test_suicide);
        
        Suite* suite = suite_create("event_json");
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