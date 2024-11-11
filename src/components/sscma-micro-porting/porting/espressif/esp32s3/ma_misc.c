
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/portmacro.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <freertos/timers.h>

#include "../../../../sscma-micro/sscma/porting/ma_misc.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>

#include "ma_config_board.h"

void* ma_malloc(size_t size) {
    return heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
}

void* ma_calloc(size_t nmemb, size_t size) {
    void* ptr = pvPortMalloc(nmemb * size);
    if (ptr) {
        memset(ptr, 0, nmemb * size);
    }
    return ptr;
}

void ma_free(void* ptr) {
    free(ptr);
}

int ma_printf(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int ret = vprintf(fmt, args);
    va_end(args);
    return ret;
}

void ma_abort(void) {
    vTaskSuspendAll();
    while (1) {
    }
}

void ma_usleep(uint32_t usec) {
    vTaskDelay(usec / 1000 / portTICK_PERIOD_MS);
}

void ma_sleep(uint32_t msec) {
    ma_usleep(msec * 1000);
}

int64_t ma_get_time_us(void) {
    return xTaskGetTickCount() * portTICK_PERIOD_MS * 1000;
}

int64_t ma_get_time_ms(void) {
    return xTaskGetTickCount() * portTICK_PERIOD_MS;
}