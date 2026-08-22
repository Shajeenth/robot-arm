#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"

#include "esp_http_server.h"

#include "driver/ledc.h"


#define SERVO_GPIO 27

#define WIFI_SSID "Gripper-ESP32"
#define WIFI_PASSWORD "gripper123"


static const char *TAG = "GRIPPER";

// GRIPPER COMMANDS

typedef enum {
    GRIPPER_OPEN,
    GRIPPER_CLOSE
} gripper_command_t;


static QueueHandle_t gripper_queue;

// SERVO

static void servo_init(void)
{
    ledc_timer_config_t timer_config = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_0,
        .duty_resolution = LEDC_TIMER_16_BIT,
        .freq_hz = 50,
        .clk_cfg = LEDC_AUTO_CLK
    };

    ledc_timer_config(&timer_config);


    ledc_channel_config_t channel_config = {
        .gpio_num = SERVO_GPIO,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
        .hpoint = 0
    };

    ledc_channel_config(&channel_config);
}


static void servo_set_angle(int angle)
{

    int pulse_us =
        500 + ((2400 - 500) * angle) / 180;

    uint32_t duty =
        (pulse_us * 65535) / 20000;


    ledc_set_duty(
        LEDC_LOW_SPEED_MODE,
        LEDC_CHANNEL_0,
        duty
    );

    ledc_update_duty(
        LEDC_LOW_SPEED_MODE,
        LEDC_CHANNEL_0
    );
}

// GRIPPER TASK

static void gripper_task(void *pvParameters)
{
    gripper_command_t command;

    while (1)
    {
        if (xQueueReceive(
                gripper_queue,
                &command,
                portMAX_DELAY))
        {
            if (command == GRIPPER_OPEN)
            {
                ESP_LOGI(TAG, "OPEN");

                servo_set_angle(180);
            }

            else if (command == GRIPPER_CLOSE)
            {
                ESP_LOGI(TAG, "CLOSE");

                servo_set_angle(60);
            }
        }
    }
}

// WEB PAGE

extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[]   asm("_binary_index_html_end");


static esp_err_t home_handler(httpd_req_t *req)
{
    size_t length =
        index_html_end - index_html_start;

    httpd_resp_set_type(
        req,
        "text/html"
    );

    httpd_resp_send(
        req,
        (const char *)index_html_start,
        length
    );

    return ESP_OK;
}

// OPEN BUTTON

static esp_err_t open_handler(httpd_req_t *req)
{
    gripper_command_t command =
        GRIPPER_OPEN;

    xQueueSend(
        gripper_queue,
        &command,
        portMAX_DELAY
    );

    httpd_resp_send(
        req,
        "Opening",
        HTTPD_RESP_USE_STRLEN
    );

    return ESP_OK;
}

// CLOSE BUTTON

static esp_err_t close_handler(httpd_req_t *req)
{
    gripper_command_t command =
        GRIPPER_CLOSE;

    xQueueSend(
        gripper_queue,
        &command,
        portMAX_DELAY
    );

    httpd_resp_send(
        req,
        "Closing",
        HTTPD_RESP_USE_STRLEN
    );

    return ESP_OK;
}

// WEB SERVER

static void start_webserver(void)
{
    httpd_config_t config =
        HTTPD_DEFAULT_CONFIG();

    httpd_handle_t server = NULL;


    ESP_ERROR_CHECK(
        httpd_start(
            &server,
            &config
        )
    );


    httpd_uri_t home = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = home_handler
    };


    httpd_uri_t open = {
        .uri = "/open",
        .method = HTTP_GET,
        .handler = open_handler
    };


    httpd_uri_t close = {
        .uri = "/close",
        .method = HTTP_GET,
        .handler = close_handler
    };


    httpd_register_uri_handler(
        server,
        &home
    );

    httpd_register_uri_handler(
        server,
        &open
    );

    httpd_register_uri_handler(
        server,
        &close
    );


    ESP_LOGI(
        TAG,
        "Web server started"
    );
}

// WIFI

static void wifi_init(void)
{
    ESP_ERROR_CHECK(
        esp_netif_init()
    );

    ESP_ERROR_CHECK(
        esp_event_loop_create_default()
    );

    esp_netif_create_default_wifi_ap();


    wifi_init_config_t cfg =
        WIFI_INIT_CONFIG_DEFAULT();

    ESP_ERROR_CHECK(
        esp_wifi_init(&cfg)
    );


    wifi_config_t wifi_config = {
        .ap = {
            .ssid = WIFI_SSID,
            .ssid_len = strlen(WIFI_SSID),
            .channel = 1,
            .password = WIFI_PASSWORD,
            .max_connection = 4,
            .authmode = WIFI_AUTH_WPA2_PSK
        }
    };


    ESP_ERROR_CHECK(
        esp_wifi_set_mode(WIFI_MODE_AP)
    );

    ESP_ERROR_CHECK(
        esp_wifi_set_config(
            WIFI_IF_AP,
            &wifi_config
        )
    );

    ESP_ERROR_CHECK(
        esp_wifi_start()
    );


    ESP_LOGI(
        TAG,
        "WiFi started"
    );

    ESP_LOGI(
        TAG,
        "SSID: %s",
        WIFI_SSID
    );

    ESP_LOGI(
        TAG,
        "Password: %s",
        WIFI_PASSWORD
    );

    ESP_LOGI(
        TAG,
        "Website: http://192.168.4.1"
    );
}

// MAIN

void app_main(void)
{
    ESP_ERROR_CHECK(
        nvs_flash_init()
    );


    // Servo
    servo_init();


    // Queue
    gripper_queue =
        xQueueCreate(
            5,
            sizeof(gripper_command_t)
        );


    if (gripper_queue == NULL)
    {
        ESP_LOGE(
            TAG,
            "Queue creation failed!"
        );

        return;
    }


    // Gripper task
    xTaskCreate(
        gripper_task,
        "gripper_task",
        2048,
        NULL,
        5,
        NULL
    );


    // WiFi
    wifi_init();


    // Website
    start_webserver();
}