#include "app_log.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "stm32g0xx_hal.h"
#include "usart.h"

#define APP_LOG_BUFFER_SIZE       192U
#define APP_LOG_UART_TIMEOUT_MS   30U

static char g_log_buffer[APP_LOG_BUFFER_SIZE];
static bool g_log_busy;
static char tx_ring[2048];
static size_t tx_write, tx_read;
volatile uint32_t g_log_dropped_records, g_log_tx_errors;
void AppLog_Tick(void)
{
    size_t available = tx_write >= tx_read ? tx_write - tx_read : sizeof(tx_ring) - tx_read;
    if (available > 32U) available = 32U;
    if (!available) return;
    if (HAL_UART_Transmit(&huart1, (uint8_t *)&tx_ring[tx_read], (uint16_t)available, 4U) != HAL_OK) g_log_tx_errors++;
    tx_read = (tx_read + available) % sizeof(tx_ring);
}

void AppLog_Init(void)
{
    g_log_busy = false;
}

void AppLog_WriteRaw(const char *data, size_t length)
{
    if (data == NULL || length == 0U) {
        return;
    }
    if (length > UINT16_MAX) {
        length = UINT16_MAX;
    }
    size_t free_bytes = (tx_read + sizeof(tx_ring) - tx_write - 1U) % sizeof(tx_ring);
    if (length > free_bytes) { g_log_dropped_records++; return; }
    for (size_t i = 0; i < length; ++i) {
        tx_ring[tx_write] = data[i]; tx_write = (tx_write + 1U) % sizeof(tx_ring);
    }
}

void AppLog_Write(const char *level, const char *format, ...)
{
    int prefix_length;
    int body_length;
    size_t used;
    va_list args;

    if (level == NULL || format == NULL || g_log_busy) {
        return;
    }
    g_log_busy = true;
    prefix_length = snprintf(g_log_buffer, sizeof(g_log_buffer), "[%010lu] [%s] ",
                             (unsigned long)HAL_GetTick(), level);
    if (prefix_length < 0) {
        g_log_busy = false;
        return;
    }
    used = (size_t)prefix_length;
    if (used >= sizeof(g_log_buffer)) {
        used = sizeof(g_log_buffer) - 1U;
    }

    va_start(args, format);
    body_length = vsnprintf(&g_log_buffer[used], sizeof(g_log_buffer) - used, format, args);
    va_end(args);
    if (body_length > 0) {
        size_t appended = (size_t)body_length;
        size_t available = sizeof(g_log_buffer) - used;
        used += appended < available ? appended : available - 1U;
    }
    if (used < sizeof(g_log_buffer) - 2U) {
        g_log_buffer[used++] = '\r';
        g_log_buffer[used++] = '\n';
        g_log_buffer[used] = '\0';
    }
    AppLog_WriteRaw(g_log_buffer, used);
    g_log_busy = false;
}

int _write(int file, char *data, int length)
{
    (void)file;
    if (data == NULL || length <= 0) {
        return 0;
    }
    AppLog_WriteRaw(data, (size_t)length);
    return length;
}
