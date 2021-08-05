#include "sntp.h"

#include <esp_log.h>
#include <sntp.h>
#include <time.h>

#include "libiot.h"

#define SNTP_SYNC_INTERVAL_MS (60 * 1000)

// A negative value means "never give up".
#define SNTP_STARTUP_POLL_MAX_RETRIES -1
#define SNTP_STARTUP_POLL_DELAY_MS 2000

void libiot_init_sntp() {
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_set_sync_mode(SNTP_SYNC_MODE_IMMED);
    sntp_set_sync_interval(SNTP_SYNC_INTERVAL_MS);

    assert(sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET);
    sntp_init();
}

void libiot_start_sntp() {
    size_t retries = 0;
    while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET) {
        ESP_LOGI(TAG, "sntp: waiting for system time to be set... (retry %d)",
                 retries);
        vTaskDelay(SNTP_STARTUP_POLL_DELAY_MS / portTICK_PERIOD_MS);
        retries++;

        if (retries > SNTP_STARTUP_POLL_MAX_RETRIES
            && SNTP_STARTUP_POLL_MAX_RETRIES >= 0) {
            ESP_LOGW(TAG, "sntp: timed out waiting for system time to be set");
            break;
        }
    }

    sntp_set_sync_mode(SNTP_SYNC_MODE_SMOOTH);

    char buff[100];
    time_t now = time(0);
    strftime(buff, sizeof(buff), "%Y-%m-%d %H:%M:%S", localtime(&now));

    ESP_LOGI(TAG, "sntp: synced time (%s)", buff);
}
