#include <stdio.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/ledc.h"

#define SERVO_GPIO 27

#define SERVO_FREQ_HZ 50
#define SERVO_TIMER LEDC_TIMER_0
#define SERVO_CHANNEL LEDC_CHANNEL_0
#define SERVO_MODE LEDC_LOW_SPEED_MODE

static void servo_init(void)
{
    ledc_timer_config_t timer = {
        .speed_mode = SERVO_MODE,
        .timer_num = SERVO_TIMER,
        .duty_resolution = LEDC_TIMER_16_BIT,
        .freq_hz = SERVO_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK
    };

    ledc_timer_config(&timer);

    ledc_channel_config_t channel = {
        .gpio_num = SERVO_GPIO,
        .speed_mode = SERVO_MODE,
        .channel = SERVO_CHANNEL,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = SERVO_TIMER,
        .duty = 0,
        .hpoint = 0
    };

    ledc_channel_config(&channel);
}

static void servo_set_angle(int angle)
{
    const int min_pulse_us = 500;
    const int max_pulse_us = 2400;

    if (angle < 0) angle = 0;
    if (angle > 180) angle = 180;

    int pulse_us = min_pulse_us +
                   ((max_pulse_us - min_pulse_us) * angle) / 180;

    uint32_t duty = ((uint32_t)pulse_us * 65535) / 20000;

    ledc_set_duty(SERVO_MODE, SERVO_CHANNEL, duty);
    ledc_update_duty(SERVO_MODE, SERVO_CHANNEL);
}

void app_main(void)
{
    servo_init();

    while (1)
    {
        printf("OPEN\n");
        servo_set_angle(180);
        vTaskDelay(pdMS_TO_TICKS(2000));

        printf("CLOSE\n");
        servo_set_angle(60);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}