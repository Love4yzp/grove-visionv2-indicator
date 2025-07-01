/**
 * @file app_keypoints.h
 * @date  12 March 2024

 * @author Spencer Yan
 *
 * @note Description of the file
 *
 * @copyright © 2024, Seeed Studio
 */

#ifndef APP_KEYPOINTS_H
#define APP_KEYPOINTS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"
#include "app_boxes.h"

/**
 * @brief 定义单个关键点的数据结构
 * 使用联合体，可以通过名称 (x, y, score) 或数组索引 (point[0]) 访问
 */
typedef union {
    struct {
        uint16_t x;
        uint16_t y;
        uint16_t score;
        // uint16_t target;
    };
    uint16_t point[3];
    // uint16_t point[4];
    // uint16_t point[2];
} keypoint_t;


/**
 * @brief 定义一个检测到的人体（包含边界框和所有关键点）
 */
typedef struct {
    boxes_t box;          // 边界框
    size_t points_count;  // 关键点数量
    keypoint_t* points;   // 指向关键点数组的指针
} keypoints_t;

// keypoint_t* create_keypoints_array(size_t count);
// // 释放关键点数组
// void free_keypoints_array(keypoint_t* array);
/**
 * @brief
 * {"keypoints":
*    [ // the first person : the first keypoint
*       {
            "box":[0,95,120,120,239,240],
            "points":[[127,77],[197,216]]
        }
*    ]
 * }
 */
// void init_keypoints_app();
// bool ParseJsonKeypoints(cJSON* receivedJson, keypoints_t** keypoints_array, int* keypoints_count);
void draw_one_point(lv_obj_t *parent, const keypoint_t point, lv_color_t color);
void draw_keypoints(lv_obj_t* canvas, const keypoints_t* keypoints);
// void draw_keypoints_array(lv_obj_t* canvas, const keypoints_t* keypoints, size_t count);

/**
 * @brief 初始化姿态绘制应用所需的资源
 * @note  例如初始化LVGL的绘图描述符
 */
void init_keypoints_app(void);

/**
 * @brief 从 cJSON 对象中解析出关键点数据
 *
 * @param keypoints_json 指向包含关键点数据的 cJSON 数组
 * @param keypoints_array 将被分配内存并填充数据的关键点数组指针
 * @param keypoints_count 解析出的关键点数量
 * @return true 解析成功
 * @return false 解析失败（例如内存不足或格式错误）
 * @note 如果解析失败，此函数会负责清理所有已分配的内存
 */
bool ParseJsonKeypoints(cJSON* receivedJson, keypoints_t** keypoints_array, int* keypoints_count);

/**
 * @brief 在LVGL画布上绘制所有检测到的人体骨骼
 *
 * @param canvas LVGL画布对象
 * @param keypoints 指向关键点数组的指针
 * @param count 数组中的人体数量
 */
void draw_keypoints_array(lv_obj_t* canvas, const keypoints_t* keypoints, size_t count);

/**
 * @brief 释放由 ParseJsonKeypoints 分配的所有内存
 *
 * @param keypoints_array 指向要释放的关键点数组的指针
 * @param count 数组中的人体数量
 */
void free_all_keypoints(keypoints_t* keypoints_array, size_t count);



#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*APP_KEYPOINTS_H*/