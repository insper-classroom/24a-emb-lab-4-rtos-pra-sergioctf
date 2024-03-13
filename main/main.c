#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <queue.h>

#include "ssd1306.h"
#include "gfx.h"

#include "pico/stdlib.h"
#include <stdio.h>

const int TRIGGER_PIN = 4;
const int ECHO_PIN = 18;

volatile absolute_time_t rise_time;
volatile absolute_time_t ECHO_PIN_time;
volatile int entrou = 1;

void ECHO_PIN_callback(uint gpio, uint32_t events) {
    if (events == 0x4) { // fall edge
        entrou = 2;
        ECHO_PIN_time = get_absolute_time();
    }
    if (events == 0x8) { // rise edge
        rise_time = get_absolute_time();
    }
}

void oled_sensor_init(void) {
    gpio_init(TRIGGER_PIN);
    gpio_set_dir(TRIGGER_PIN, GPIO_OUT);

    gpio_init(ECHO_PIN);
    gpio_set_dir(ECHO_PIN, GPIO_IN);

    gpio_set_irq_enabled_with_callback(ECHO_PIN, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true, &ECHO_PIN_callback);
}

void oled_distance_display(void *p) {
    printf("Inicializando Driver\n");
    ssd1306_init();

    printf("Inicializando GLX\n");
    ssd1306_t disp;
    gfx_init(&disp, 128, 32);

    printf("Inicializando sensor\n");
    oled_sensor_init();

    while (1) {
        // Envia um pulso ultrassônico
        gpio_put(TRIGGER_PIN, 1);
        vTaskDelay(pdMS_TO_TICKS(10)); // Delay de 10ms
        gpio_put(TRIGGER_PIN, 0);

        // Espera por um tempo suficiente para a detecção do eco
        vTaskDelay(pdMS_TO_TICKS(100));

        if (entrou == 2) {
            uint32_t difference_us = absolute_time_diff_us(rise_time, ECHO_PIN_time);
            float distance_m = (difference_us / 1000000.0) * 340.0 / 2.0;

            // Limpa o display
            gfx_clear_buffer(&disp);
            
            // Mostra a distância
            gfx_draw_string(&disp, 0, 0, 1, "Distancia:");
            char distance_str[16];
            snprintf(distance_str, sizeof(distance_str), "%.2f m", distance_m);
            gfx_draw_string(&disp, 0, 10, 1, distance_str);

            // Desenha a barra proporcional à distância
            // Ajuste o fator de escala conforme necessário para sua aplicação
            float scale = 0.64; // Mapeia 1 cm para 0.64 pixels
            int bar_length = (int)(distance_m * 100 * scale); // Converte metros para cm e aplica o fator de escala
            if(bar_length > 128) bar_length = 128; // Limita o comprimento da barra ao máximo da tela
            gfx_draw_line(&disp, 0, 20, bar_length, 20);

            // Mostra as atualizações no display
            gfx_show(&disp);

            printf("Distância (m): %.2f\n", distance_m);

            entrou = 1; // Prepara para a próxima medição
        }

        // Ajusta o tempo de espera conforme necessário para controlar a frequência de medição
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

int main() {
    stdio_init_all();
    xTaskCreate(oled_distance_display, "Distance Display", 4095, NULL, 1, NULL);

    vTaskStartScheduler();


    while (true)
        ;
}