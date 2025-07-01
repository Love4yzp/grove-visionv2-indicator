/**
 * @file main.h
 * @date  23 February 2024

 * @author Spencer Yan
 *
 * @copyright © 2024, Seeed Studio
 */

#ifndef MAIN_H
#define MAIN_H

#include <stdlib.h>

#include "bsp_board.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "esp_event_base.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char* img_base64; // 存储Base64编码的图像数据
    int img_size;
} img_event_data_t;

ESP_EVENT_DECLARE_BASE(VIEW_EVENT_BASE);
extern esp_event_loop_handle_t view_event_handle;

enum {
    VIEW_EVENT_MODEL_NAME,
    VIEW_EVENT_IMG,
    VIEW_EVENT_BOXES,
    VIEW_EVENT_KEYPOINTS,
    VIEW_EVENT_PAGE_SWITCH,
    VIEW_EVENT_ALL,
};

extern QueueHandle_t JsonQueue;
extern bool is_name_geted;

typedef struct {
    uint8_t target;
    uint8_t score;
} classes_t;

typedef struct {
    uint16_t prepocess;
    uint16_t inference;
    uint16_t postprocess;
} perf_t;

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*MAIN_H*/