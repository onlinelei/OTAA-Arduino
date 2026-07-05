/**
 * OTAA Library Test
 *
 * Unit tests for OTAA library
 *
 * Run with PlatformIO:
 *   pio test -e esp32dev
 */

#include <unity.h>
#include <OTAA.h>

void test_otaa_initialization() {
    OTAA ota;
    bool result = ota.begin("http://localhost", "test_device", "test_token");
    TEST_ASSERT_TRUE(result);
}

void test_otaa_version() {
    OTAA ota;
    TEST_ASSERT_EQUAL_STRING("1.0.0", OTAA_VERSION);
}

void test_otaa_state() {
    OTAA ota;
    TEST_ASSERT_EQUAL(OTA_IDLE, ota.getState());
}

void test_otaa_progress() {
    OTAA ota;
    TEST_ASSERT_EQUAL(0, ota.getProgress());
}

void setup() {
    UNITY_BEGIN();

    RUN_TEST(test_otaa_initialization);
    RUN_TEST(test_otaa_version);
    RUN_TEST(test_otaa_state);
    RUN_TEST(test_otaa_progress);

    UNITY_END();
}

void loop() {
    // Nothing to do here
}
