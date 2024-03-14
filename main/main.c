#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"
#include "ssd1306.h"
#include "gfx.h"

// Pin configuration
const int TRIGGER_PIN = 4;
const int ECHO_PIN = 18;

// FreeRTOS Handles
SemaphoreHandle_t xSemaphoreTrigger;
QueueHandle_t xQueueDistance;

// OLED Display object
ssd1306_t disp;

void ECHO_PIN_callback(uint gpio, uint32_t events) {
    static BaseType_t xHigherPriorityTaskWoken;
    static absolute_time_t rise_time;

    if (events & GPIO_IRQ_EDGE_RISE) {
        rise_time = get_absolute_time();
    } else if (events & GPIO_IRQ_EDGE_FALL) {
        absolute_time_t fall_time = get_absolute_time();
        uint32_t difference_us = absolute_time_diff_us(rise_time, fall_time);
        float distance_m = (difference_us / 1000000.0) * 340.29 / 2.0;
        xQueueSendFromISR(xQueueDistance, &distance_m, &xHigherPriorityTaskWoken);
    }
}

void trigger_task(void *params) {
    while (1) {
        xSemaphoreGive(xSemaphoreTrigger);
        vTaskDelay(pdMS_TO_TICKS(60)); // Delay to avoid continuous trigger
    }
}

void echo_task(void *params) {
    while (1) {
        if (xSemaphoreTake(xSemaphoreTrigger, portMAX_DELAY) == pdTRUE) {
            gpio_put(TRIGGER_PIN, 1);
            busy_wait_us_32(10); // Wait for 10 microseconds
            gpio_put(TRIGGER_PIN, 0);
        }
    }
}

void oled_task(void *params) {
    float distance;
    while (1) {
        if (xQueueReceive(xQueueDistance, &distance, portMAX_DELAY) == pdTRUE) {
            gfx_clear_buffer(&disp);
            gfx_draw_string(&disp, 0, 0, 1, "Distancia:");
            char distance_str[16];
            snprintf(distance_str, sizeof(distance_str), "%.2f m", distance);
            gfx_draw_string(&disp, 0, 10, 1, distance_str);
            float scale = 0.64; // Mapeia 1 cm para 0.64 pixels
            int bar_length = (int)(distance * 100 * scale); // Converte metros para cm e aplica o fator de escala
            if(bar_length > 128) bar_length = 128; // Limita o comprimento da barra ao máximo da tela
            gfx_draw_line(&disp, 0, 20, bar_length, 20);

            // Mostra as atualizações no display
            gfx_show(&disp);
        }
    }
}

int main() {
    stdio_init_all();

    // Initialize GPIO for trigger and echo
    gpio_init(TRIGGER_PIN);
    gpio_set_dir(TRIGGER_PIN, GPIO_OUT);
    gpio_put(TRIGGER_PIN, 0);

    gpio_init(ECHO_PIN);
    gpio_set_dir(ECHO_PIN, GPIO_IN);
    gpio_set_irq_enabled_with_callback(ECHO_PIN, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true, &ECHO_PIN_callback);

    ssd1306_init();
    gfx_init(&disp, 128, 32);

    xSemaphoreTrigger = xSemaphoreCreateBinary();
    xQueueDistance = xQueueCreate(5, sizeof(float));

    xTaskCreate(trigger_task, "Trigger Task", 256, NULL, 1, NULL);
    xTaskCreate(echo_task, "Echo Task", 256, NULL, 1, NULL);
    xTaskCreate(oled_task, "OLED Task", 256, NULL, 1, NULL);

    vTaskStartScheduler();

    while (1) {}
}