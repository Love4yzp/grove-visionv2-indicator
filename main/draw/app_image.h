/**
 * @file image.h
 * @date  27 February 2024

 * @author Spencer Yan
 *
 * @note Description of the file
 *
 * @copyright © 2024, Seeed Studio
 */

#ifndef IMAGE_H
#define IMAGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_log.h"
#include "lvgl.h"
#include <cJSON.h>

#define CANVAS_WIDTH 240
#define CANVAS_HEIGHT CANVAS_WIDTH

// 从JSON解析并显示图像的函数
void init_image(void);
void display_image_from_json(cJSON* json, lv_obj_t* canvas);

size_t decode_base64_image(const unsigned char* p_data, unsigned char* decoded_str);
void update_canvas_with_image(lv_obj_t* canvas, const unsigned char* image_data, size_t image_size);
// void update_canvas_with_image(lv_obj_t* canvas, const unsigned char* image_data, size_t image_size);
// void display_one_image(lv_obj_t* image, const unsigned char* p_data);
// void update_canvas_with_image(lv_obj_t* canvas, const unsigned char* base64_image_data);
#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*IMAGE_H*/