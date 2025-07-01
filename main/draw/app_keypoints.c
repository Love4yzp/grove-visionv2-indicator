#include "esp_log.h"
#include "lvgl.h"
#include "cJSON.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "app_keypoints.h"
#include "esp_log.h"

#define TAG "KEYPOINTS_APP"

//----------------------------------------------------------------
// 静态全局变量 (Static Globals)
//----------------------------------------------------------------

// LVGL 绘图描述符
static lv_draw_line_dsc_t line_dsc;
static lv_draw_rect_dsc_t rect_dsc;

// 关键点分数阈值，低于此分数的点和线将不被绘制
static const uint16_t LIMIT_SCORE = 10;

//----------------------------------------------------------------
// 数据驱动的骨骼结构定义 (Data-Driven Skeleton Definition)
//----------------------------------------------------------------

// 定义骨骼连接线类型
typedef enum {
    LINE_TYPE_NORMAL,        // 普通连接：连接两个真实的关键点
    LINE_TYPE_VIRTUAL_NECK,  // 虚拟连接：连接鼻子到“脖子”中点
    LINE_TYPE_VIRTUAL_SPINE, // 虚拟连接：连接“脖子”中点和“臀部”中点
} line_type_t;

// 定义骨骼连接的结构体
typedef struct {
    uint8_t p1_idx;      // 起始点索引
    uint8_t p2_idx;      // 结束点索引
    line_type_t type;    // 线条类型
    lv_color_t color;    // 线条颜色
} skeleton_conn_t;

// 预定义颜色
static const lv_color_t COLOR_HEAD = LV_COLOR_MAKE(255, 85, 85);
static const lv_color_t COLOR_BODY = LV_COLOR_MAKE(85, 255, 85);
static const lv_color_t COLOR_LEGS = LV_COLOR_MAKE(85, 85, 255);


/**
 * @brief 17点人体姿态模型的骨骼连接图
 * @note  该结构将骨骼拓扑、颜色和绘制逻辑完全解耦
 */
static const skeleton_conn_t SKELETON_CONNECTIONS[] = {
    // 头部 (鼻子-眼-耳)
    {0, 1, LINE_TYPE_NORMAL, COLOR_HEAD}, {1, 3, LINE_TYPE_NORMAL, COLOR_HEAD},
    {0, 2, LINE_TYPE_NORMAL, COLOR_HEAD}, {2, 4, LINE_TYPE_NORMAL, COLOR_HEAD},
    // 身体 (肩-肘-腕)
    {5, 6, LINE_TYPE_NORMAL, COLOR_BODY},
    {5, 7, LINE_TYPE_NORMAL, COLOR_BODY}, {7, 9, LINE_TYPE_NORMAL, COLOR_BODY},
    {6, 8, LINE_TYPE_NORMAL, COLOR_BODY}, {8, 10,LINE_TYPE_NORMAL, COLOR_BODY},
    // 腿部 (臀-膝-踝)
    {11, 13, LINE_TYPE_NORMAL, COLOR_LEGS}, {13, 15, LINE_TYPE_NORMAL, COLOR_LEGS},
    {12, 14, LINE_TYPE_NORMAL, COLOR_LEGS}, {14, 16, LINE_TYPE_NORMAL, COLOR_LEGS},
    {11, 12, LINE_TYPE_NORMAL, COLOR_LEGS}, // 连接左右臀
    // -- 特殊的虚拟连接线 --
    {0, 5, LINE_TYPE_VIRTUAL_NECK,  COLOR_BODY}, // 鼻子 -> 脖子中点
    {5, 11,LINE_TYPE_VIRTUAL_SPINE, COLOR_BODY}, // 脖子中点 -> 臀部中点
};

//----------------------------------------------------------------
// 内存管理函数 (Memory Management Functions)
//----------------------------------------------------------------

// 创建关键点数组
static keypoint_t* create_keypoints_array(size_t count) {
    return (keypoint_t*)malloc(sizeof(keypoint_t) * count);
}

// 释放单个 keypoints_t 结构体内部的 points 数组
static void free_keypoints_internal(keypoints_t* kp) {
    if (kp && kp->points) {
        free(kp->points);
        kp->points = NULL;
    }
}

// 释放由 ParseJsonKeypoints 分配的所有内存
void free_all_keypoints(keypoints_t* keypoints_array, size_t count) {
    if (!keypoints_array) return;
    for (size_t i = 0; i < count; ++i) {
        free_keypoints_internal(&keypoints_array[i]);
    }
    free(keypoints_array);
}

//----------------------------------------------------------------
// JSON 解析函数 (JSON Parsing Function)
//----------------------------------------------------------------

bool ParseJsonKeypoints(cJSON* keypoints_json, keypoints_t** keypoints_array, int* keypoints_count) {
    if (!keypoints_json || !keypoints_array || !keypoints_count) return false;

    int count = cJSON_GetArraySize(keypoints_json);
    if (count <= 0) {
        // ESP_LOGI(TAG, "JSON array is empty or not an array.");
        return false;
    }

    // 使用 calloc 分配并自动清零，更安全
    keypoints_t* kp_array = (keypoints_t*)calloc(count, sizeof(keypoints_t));
    if (kp_array == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for keypoints array");
        return false;
    }

    bool success = true;
    for (int index = 0; index < count; index++) {
        cJSON* keypointJson = cJSON_GetArrayItem(keypoints_json, index);
        keypoints_t* current_keypoint = &kp_array[index];

        // 1. 解析边界框 "box"
        cJSON* boxJson = cJSON_GetObjectItem(keypointJson, "box");
        if (cJSON_IsArray(boxJson) && cJSON_GetArraySize(boxJson) == 6) {
            for (int i = 0; i < 6; i++) {
                cJSON* item = cJSON_GetArrayItem(boxJson, i);
                if (item) current_keypoint->box.boxArray[i] = item->valueint;
            }
        }

        // 2. 解析关键点 "points"
        cJSON* pointsJson = cJSON_GetObjectItem(keypointJson, "points");
        if (cJSON_IsArray(pointsJson)) {
            size_t points_count = cJSON_GetArraySize(pointsJson);
            if (points_count > 0) {
                current_keypoint->points = create_keypoints_array(points_count);
                if (current_keypoint->points == NULL) {
                    ESP_LOGE(TAG, "Failed to allocate memory for points at index %d", index);
                    success = false;
                    goto cleanup; // 分配失败，跳转到清理
                }
                current_keypoint->points_count = points_count;

                for (int p_idx = 0; p_idx < points_count; p_idx++) {
                    cJSON* point_json = cJSON_GetArrayItem(pointsJson, p_idx);
                    if (!cJSON_IsArray(point_json)) continue;

                    size_t point_dims = cJSON_GetArraySize(point_json);
                    size_t loop_count = (point_dims > 3) ? 3 : point_dims;

                    for (int i = 0; i < loop_count; i++) {
                        cJSON* item = cJSON_GetArrayItem(point_json, i);
                        if (item) {
                            current_keypoint->points[p_idx].point[i] = (uint16_t)item->valueint;
                        }
                    }
                }
            }
        }
    }

cleanup:
    if (success) {
        *keypoints_array = kp_array;
        *keypoints_count = count;
    } else {
        free_all_keypoints(kp_array, count); // 如果失败，释放所有已分配的内存
        *keypoints_array = NULL;
        *keypoints_count = 0;
    }
    return success;
}

//----------------------------------------------------------------
// 绘图函数 (Drawing Functions)
//----------------------------------------------------------------

// 内部函数，绘制单个人的骨骼
static void draw_one_person_keypoints(lv_obj_t* canvas, const keypoints_t* kp) {
    if (kp->points_count != 17) {
        ESP_LOGW(TAG, "Pose estimation requires 17 keypoints, but got %zu. Skipping.", kp->points_count);
        return;
    }

    // 1. 绘制所有分数达标的关键点
    for (size_t i = 0; i < kp->points_count; i++) {
        if (kp->points[i].score >= LIMIT_SCORE) {
            lv_canvas_draw_rect(canvas, kp->points[i].x - 2, kp->points[i].y - 2, 5, 5, &rect_dsc);
        }
    }

    // 2. 绘制骨骼连线
    for (size_t i = 0; i < sizeof(SKELETON_CONNECTIONS) / sizeof(SKELETON_CONNECTIONS[0]); ++i) {
        const skeleton_conn_t* conn = &SKELETON_CONNECTIONS[i];

        const keypoint_t* p1 = &kp->points[conn->p1_idx];
        const keypoint_t* p2 = &kp->points[conn->p2_idx];

        // 检查连接两端的点分数是否都达标
        if (p1->score < LIMIT_SCORE || p2->score < LIMIT_SCORE) {
            continue;
        }

        line_dsc.color = conn->color;
        lv_point_t line_points[2];

        // 根据线条类型计算起点和终点
        switch (conn->type) {
            case LINE_TYPE_NORMAL:
                line_points[0] = (lv_point_t){p1->x, p1->y};
                line_points[1] = (lv_point_t){p2->x, p2->y};
                break;
            case LINE_TYPE_VIRTUAL_NECK:
                line_points[0] = (lv_point_t){p1->x, p1->y}; // p1是鼻子
                line_points[1].x = (kp->points[5].x + kp->points[6].x) / 2;
                line_points[1].y = (kp->points[5].y + kp->points[6].y) / 2;
                break;
            case LINE_TYPE_VIRTUAL_SPINE:
                line_points[0].x = (kp->points[5].x + kp->points[6].x) / 2;
                line_points[0].y = (kp->points[5].y + kp->points[6].y) / 2;
                line_points[1].x = (kp->points[11].x + kp->points[12].x) / 2;
                line_points[1].y = (kp->points[11].y + kp->points[12].y) / 2;
                break;
        }
        
        lv_canvas_draw_line(canvas, line_points, 2, &line_dsc);
    }
}

// 公开接口，绘制所有人的骨骼
void draw_keypoints_array(lv_obj_t* canvas, const keypoints_t* keypoints, size_t count) {
    for (size_t i = 0; i < count; i++) {
        draw_one_person_keypoints(canvas, &keypoints[i]);
    }
}

//----------------------------------------------------------------
// 初始化函数 (Initialization Function)
//----------------------------------------------------------------

void init_keypoints_app() {
    // 初始化线条样式
    lv_draw_line_dsc_init(&line_dsc);
    line_dsc.color = lv_palette_main(LV_PALETTE_RED); // 默认颜色
    line_dsc.width = 3;
    line_dsc.round_end = 1;
    line_dsc.round_start = 1;

    // 初始化关键点矩形样式
    lv_draw_rect_dsc_init(&rect_dsc);
    rect_dsc.bg_color = lv_palette_main(LV_PALETTE_YELLOW);
    rect_dsc.bg_opa = LV_OPA_COVER;
    rect_dsc.radius = LV_RADIUS_CIRCLE;
    rect_dsc.border_width = 0;
}


// void draw_one_point(lv_obj_t* parent, const keypoint_t point, lv_color_t color) {}

// // 创建关键点数组
// keypoint_t* create_keypoints_array(size_t count) {
//     return (keypoint_t*)malloc(sizeof(keypoint_t) * count);
// }

// // 释放关键点数组
// void free_keypoints_array(keypoint_t* array) {
//     free(array);
// }

// // 初始化keypoints_t结构体
// void init_keypoints(keypoints_t* kp, size_t count) {
//     kp->points = create_keypoints_array(count);
//     kp->points_count = count;
//     // 此处可以初始化boxes_t等其他成员
// }

// // 释放keypoints_t结构体
// void free_keypoints(keypoints_t* kp) {
//     free_keypoints_array(kp->points);
//     // 如果有其他需要释放的资源，也在这里处理
// }

// void print_keypoints(const keypoints_t* keypoints, size_t count) {
//     for (size_t i = 0; i < count; i++) {
//         const keypoints_t* kp = &keypoints[i];

//         // 打印boxes_t信息，根据你的定义调整
//         ESP_LOGI(TAG, "Box: [%d, %d, %d, %d, %d, %d]", kp->box.x, kp->box.y, kp->box.w, kp->box.h, kp->box.score,
//                  kp->box.target);
//         // 打印所有关键点
//         for (size_t j = 0; j < kp->points_count; j++) {
//             const keypoint_t* point = &kp->points[j];
//             ESP_LOGI(TAG, "Point[%d]: [%d, %d]", j, point->x, point->y);
//         }
//     }
// }

// bool ParseJsonKeypoints(cJSON* keypoints_json, keypoints_t** keypoints_array, int* keypoints_count) {

//     int count = cJSON_GetArraySize(keypoints_json);
//     if (count <= 0) {
//         ESP_LOGE(TAG, "No keypoints found");
//         return false;
//     }

//     *keypoints_array = (keypoints_t*)malloc(sizeof(keypoints_t) * count);
//     if (*keypoints_array == NULL) {
//         ESP_LOGE(TAG, "Failed to allocate memory for keypoints_array");
//         return false;
//     }
//     memset(*keypoints_array, 0, sizeof(keypoints_t) * count); // Initializes memory
//     *keypoints_count = count;

//     for (int index = 0; index < count; index++) { // Every keypoint
//         cJSON* keypointJson = cJSON_GetArrayItem(keypoints_json, index);
//         keypoints_t* current_keypoint = &(*keypoints_array)[index];

//         cJSON* boxJson = cJSON_GetObjectItem(keypointJson, "box");
//         if (cJSON_IsArray(boxJson) && cJSON_GetArraySize(boxJson) == 6) {
//             for (int i = 0; i < 6; i++) { // x, y, w, h, score, target
//                 cJSON* item = cJSON_GetArrayItem(boxJson, i);
//                 if (item)
//                     current_keypoint->box.boxArray[i] = item->valueint;
//             }
//         }

//         cJSON* pointsJson = cJSON_GetObjectItem(keypointJson, "points");
//         if (cJSON_IsArray(pointsJson)) {
//             size_t points_count = cJSON_GetArraySize(pointsJson);
//             if (points_count > 0) {
//                 current_keypoint->points = create_keypoints_array(points_count);
//                 current_keypoint->points_count = points_count;

//                 for (int point_index = 0; point_index < points_count; point_index++) {
//                     cJSON* point = cJSON_GetArrayItem(pointsJson, point_index);
//                     // size_t point_size = cJSON_GetArraySize(point);
//                     // for (int i = 0; i < 2; i++) {
//                     // for (int i = 0; i < 4; i++) {
//                     for (int i = 0; i < 3; i++) {
//                         cJSON* item = cJSON_GetArrayItem(point, i);
//                         if (item) {
//                             // ESP_LOGI(TAG, "point[%d]: %d", i, item->valueint);
//                             current_keypoint->points[point_index].point[i] = item->valueint;
//                         }
//                     }
//                 }
//             }
//         }
//     }
//     return true;
// }

// static lv_draw_line_dsc_t line_dsc;
// static lv_draw_rect_dsc_t rect_dsc;
// // 人体骨骼连接关系，每对数字代表需要连接的关键点索引, 并按顺序画线
// static const int skeleton_num[][2] = {
//     // 鼻子 -> 左眼 -> 左耳
//     {0, 1}, // nose to left eye
//     {1, 3}, // left eye to left ear

//     // 鼻子 -> 右眼 -> 右耳
//     {0, 2}, // nose to right eye
//     {2, 4}, // right eye to right ear

//     // 鼻子 -> 脖子（虚拟点，通常是左肩和右肩中点）-> 中脊柱（虚拟点，位于左髋和右髋中点）
//     {0, 5},  // nose to neck
//     {5, 11}, // neck to mid spine

//     // 左肩 -> 左肘 -> 左手腕
//     {5, 7}, // left shoulder to left elbow
//     {5, 7}, // left shoulder to left elbow
//     {7, 9}, // left elbow to left wrist

//     {5, 6}, // left shoulder to right shoulder

//     // 右肩 -> 右肘 -> 右手腕
//     {6, 8},  // right shoulder to right elbow
//     {8, 10}, // right elbow to right wrist

//     // 左髋 -> 左膝 -> 左脚踝
//     {11, 13}, // left hip to left knee
//     {13, 15}, // left knee to left ankle

//     // 右髋 -> 右膝 -> 右脚踝
//     {12, 14}, // right hip to right knee
//     {14, 16}, // right knee to right ankle
// };

// const int limit_score = 5; // 限制关键点的最低分数
// // keypoints is not keypoints_array
// void draw_keypoints(lv_obj_t* canvas, const keypoints_t* keypoints) {

//     // 定义颜色
//     const lv_color_t color_head = lv_color_make(255, 0, 0); // 红色，头部
//     const lv_color_t color_body = lv_color_make(0, 255, 0); // 绿色，身体
//     const lv_color_t color_legs = lv_color_make(0, 0, 255); // 蓝色，腿部

//     keypoints_t* kp = (keypoints_t*)keypoints;

//     if (kp->points_count != 17) { // Posture Detection
//         ESP_LOGI(TAG, "keypoints count is not 17, %d", kp->points_count);
//         return;
//     }

//     // draw points
//     for (int i = 0; i < kp->points_count; i++) {
//         const keypoint_t* point = &kp->points[i];
//         lv_point_t rect_point = {point->x - 1, point->y - 1};
//         // ESP_LOGI("Draw Points", "point[%d]: %d, %d", i, point->x, point->y);
//         lv_canvas_draw_rect(canvas, point->x, point->y, 6, 6, &rect_dsc);
//     }

//     // draw lines
//     for (int idx = 0; idx < sizeof(skeleton_num) / sizeof(skeleton_num[0]); idx++) {
//         // target 就是 skeleton_num 每一对的索引
//         int start_point = skeleton_num[idx][0];
//         int end_point = skeleton_num[idx][1];

//         // if (kp->points[start_point].score < limit_score || kp->points[end_point].score < limit_score) {
//         //     continue;
//         // }

//         lv_point_t line_points[2] = {{kp->points[start_point].x, kp->points[start_point].y},
//                                      {kp->points[end_point].x, kp->points[end_point].y}};

//         // 根据连接关系选择颜色
//         if (start_point == 0 || end_point == 0) // 鼻子连接线
//             line_dsc.color = color_head;
//         else if ((start_point >= 5 && start_point <= 6) || (end_point >= 5 && end_point <= 6)) // 身体连接线
//             line_dsc.color = color_body;
//         else // 腿部连接线
//             line_dsc.color = color_legs;

//         // if ((start_point == 5 || start_point == 6) && (end_point == 11 || end_point == 12)) { // Shoulder to hip
//         if (start_point == 5 && end_point == 11) { // Shoulder to hip
//             line_points[0].x = (kp->points[5].x + kp->points[6].x) / 2;
//             line_points[0].y = (kp->points[5].y + kp->points[6].y) / 2;
//             line_points[1].x = (kp->points[11].x + kp->points[12].x) / 2;
//             line_points[1].y = (kp->points[11].y + kp->points[12].y) / 2;
//         } else if (start_point == 0 && end_point == 5) { // Nose to neck
//             line_points[1].x = (kp->points[5].x + kp->points[6].x) / 2;
//             line_points[1].y = (kp->points[5].y + kp->points[6].y) / 2;
//         } else {
//         }
//         lv_canvas_draw_line(canvas, line_points, 2, &line_dsc);
//     }
// }
// /**
//  * @brief 绘制所有一张图片中 People 的关键点
//  *
//  * @param parent
//  * @param keypoints
//  * @param count
//  */
// void draw_keypoints_array(lv_obj_t* canvas, const keypoints_t* keypoints, size_t count) {
//     for (size_t i = 0; i < count; i++) { // 遍历所有人
//         draw_keypoints(canvas, &keypoints[i]);
//     }
// }

// void init_keypoints_app() {

//     lv_draw_line_dsc_init(&line_dsc);
//     line_dsc.color = lv_palette_main(LV_PALETTE_RED);
//     line_dsc.width = 2;
//     line_dsc.round_end = 1;
//     line_dsc.round_start = 1;

//     lv_draw_rect_dsc_init(&rect_dsc);
//     rect_dsc.border_color = lv_palette_main(LV_PALETTE_PURPLE);
//     rect_dsc.border_width = 1;
//     rect_dsc.radius = 4;
// }