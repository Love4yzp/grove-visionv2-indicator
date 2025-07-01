#include "bsp_board.h"
#include "esp_event.h"
#include "esp_event_base.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lv_port.h"
#include "ui/ui.h"

#include "esp32_rp2040.h"
#include "main.h"

#include "cJSON.h"

#include "mbedtls/base64.h"
#include "string.h"

#define VERSION "v1.0.0"
#define LOG_MEM_INFO 0
#define SENSECAP \
    "\n\
   _____                      _________    ____         \n\
  / ___/___  ____  ________  / ____/   |  / __ \\       \n\
  \\__ \\/ _ \\/ __ \\/ ___/ _ \\/ /   / /| | / /_/ /   \n\
 ___/ /  __/ / / (__  )  __/ /___/ ___ |/ ____/         \n\
/____/\\___/_/ /_/____/\\___/\\____/_/  |_/_/           \n\
--------------------------------------------------------\n\
 Version: %s %s %s\n\
--------------------------------------------------------\n\
"
#include "app_boxes.h"
#include "app_image.h"
#include "app_keypoints.h"
#include "indicator_btn.h"

static const char* TAG = "app_main";

ESP_EVENT_DEFINE_BASE(VIEW_EVENT_BASE);
esp_event_loop_handle_t view_event_handle;

void JsonQueue_processing_task(void* pvParameters);


lv_obj_t* canvas_left;
lv_obj_t* canvas_right;

uint8_t* cbuf_left;
uint8_t* cbuf_right;
bool is_name_geted = false;

#define BASE64_IMAGE_MAX_SIZE (15 * 1024)
static unsigned char imageData[BASE64_IMAGE_MAX_SIZE] = {0};
#define DECODED_IMAGE_MAX_SIZE (13 * 1024)
static unsigned char jpegImage[DECODED_IMAGE_MAX_SIZE + 1];

static bool is_right_canva_drawn = false;

typedef struct {
    keypoints_t* keypoints_array;
    int keypoints_count;
} keypoints_array_t;

/* 处理JSON数据的函数 */
static void process_json_data(cJSON* receivedJson) {
    if (receivedJson == NULL) return;

    /* 处理图像数据 */
    cJSON* jsonImage = cJSON_GetObjectItem(receivedJson, "img");
    if (cJSON_IsString(jsonImage) && jsonImage->valuestring) {
        lv_memset_00(imageData, sizeof(imageData));
        strncpy((char*)imageData, jsonImage->valuestring, sizeof(imageData) - 1);
        esp_event_post_to(view_event_handle, VIEW_EVENT_BASE, VIEW_EVENT_IMG, imageData,
                          sizeof(imageData), portMAX_DELAY);
    }

    /* 处理boxes数据 */
    cJSON* jsonBoxes = cJSON_GetObjectItem(receivedJson, "boxes");
    if (cJSON_IsArray(jsonBoxes)) {
        int arraySize = cJSON_GetArraySize(jsonBoxes);
        for (int i = 0; i < arraySize; i++) {
            cJSON* boxJson = cJSON_GetArrayItem(jsonBoxes, i);
            if (cJSON_IsArray(boxJson) && cJSON_GetArraySize(boxJson) >= 6) {
                boxes_t box;
                for (int j = 0; j < 6; j++) {
                    cJSON* item = cJSON_GetArrayItem(boxJson, j);
                    if (item) {
                        box.boxArray[j] = item->valueint;
                    }
                }
                esp_event_post_to(view_event_handle, VIEW_EVENT_BASE, VIEW_EVENT_BOXES, &box,
                                  sizeof(box), portMAX_DELAY);
            }
        }
    }

    /* 处理keypoints数据 */
    cJSON* jsonKeypoints = cJSON_GetObjectItem(receivedJson, "keypoints");
    if (cJSON_IsArray(jsonKeypoints)) {
        keypoints_array_t keypoints_array;
        if (ParseJsonKeypoints(jsonKeypoints, &keypoints_array.keypoints_array, &keypoints_array.keypoints_count) 
            && keypoints_array.keypoints_array != NULL) {
            esp_event_post_to(view_event_handle, VIEW_EVENT_BASE, VIEW_EVENT_KEYPOINTS, &keypoints_array,
                              sizeof(keypoints_array_t), portMAX_DELAY);
        }
    }
}

static void __json_event_handler(void* handler_args, esp_event_base_t base, int32_t id, void* event_data) {

    static int right_canvs_delay = 0;
    static bool is_right_canvas_cleaned = 0;

    switch (id) {
    case VIEW_EVENT_IMG: {
        unsigned char* img_data = (unsigned char*)event_data;

        size_t jpegImageSize = decode_base64_image(img_data, jpegImage);
        if (!jpegImageSize) {
            ESP_LOGE(TAG, "Failed to decode image");
        }
        lv_port_sem_take();
        if (!is_right_canvas_cleaned && ++right_canvs_delay >= 2) {
            is_right_canvas_cleaned = true;
            lv_canvas_fill_bg(canvas_right, lv_palette_main(LV_PALETTE_NONE), LV_OPA_COVER);
        }
        update_canvas_with_image(canvas_left, jpegImage, jpegImageSize);
        lv_port_sem_give();
        break;
    }

    case VIEW_EVENT_BOXES: {
        boxes_t* box = (boxes_t*)event_data;
        lv_port_sem_take();
        draw_one_box(canvas_left, *box, lv_color_make(113, 235, 52));
        lv_port_sem_give();
        break;
    }
    case VIEW_EVENT_KEYPOINTS: {
        keypoints_array_t* keypoints_array = (keypoints_array_t*)event_data;
        keypoints_t* keypoints = keypoints_array->keypoints_array;
        int keypoints_count = keypoints_array->keypoints_count;

        right_canvs_delay = 0;
        is_right_canvas_cleaned = false;
        lv_port_sem_take();
        lv_canvas_fill_bg(canvas_right, lv_palette_main(LV_PALETTE_NONE), LV_OPA_COVER);
        
        draw_keypoints_array(canvas_left, keypoints, keypoints_count);
        draw_keypoints_array(canvas_right, keypoints, keypoints_count);

        lv_port_sem_give();

        free_all_keypoints(keypoints, keypoints_count);
        break;
    }
    case VIEW_EVENT_ALL: {
        ESP_LOGI(TAG, "VIEW_EVENT_ALL");
        break;
    }
    default:
        break;
    }
}

void app_main(void) {

    ESP_ERROR_CHECK(bsp_board_init());
    lv_port_init();
    indicator_btn_init();
    esp_event_loop_args_t view_event_task_args = {.queue_size = 20,
                                                  .task_name = "view_event_task",
                                                  .task_priority = uxTaskPriorityGet(NULL),
                                                  .task_stack_size = 1024 * 5,
                                                  .task_core_id = tskNO_AFFINITY};
    ESP_ERROR_CHECK(esp_event_loop_create(&view_event_task_args, &view_event_handle));

    lv_port_sem_take();
    ui_init();
    ESP_LOGI(TAG, "Out of ui_init()");
    cbuf_left = (uint8_t*)heap_caps_malloc(LV_IMG_BUF_SIZE_TRUE_COLOR(CANVAS_WIDTH, CANVAS_HEIGHT), MALLOC_CAP_SPIRAM);
    cbuf_right = (uint8_t*)heap_caps_malloc(LV_IMG_BUF_SIZE_TRUE_COLOR(CANVAS_WIDTH, CANVAS_HEIGHT), MALLOC_CAP_SPIRAM);
    
    if (cbuf_left == NULL || cbuf_right == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for canvas buffers");
        return;
    }

    canvas_left = lv_canvas_create(lv_scr_act());
    canvas_right = lv_canvas_create(lv_scr_act());
    
    lv_canvas_set_buffer(canvas_left, cbuf_left, CANVAS_WIDTH, CANVAS_HEIGHT, LV_IMG_CF_TRUE_COLOR);
    lv_canvas_set_buffer(canvas_right, cbuf_right, CANVAS_WIDTH, CANVAS_HEIGHT, LV_IMG_CF_TRUE_COLOR);

    lv_obj_align(canvas_left, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_align(canvas_right, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_canvas_fill_bg(canvas_left, lv_palette_main(LV_PALETTE_NONE), LV_OPA_COVER);
    lv_canvas_fill_bg(canvas_right, lv_palette_main(LV_PALETTE_GREY), LV_OPA_COVER);

    init_image();
    init_keypoints_app();
    init_boxes_app();
    lv_port_sem_give();

    ESP_ERROR_CHECK(esp_event_handler_instance_register_with(view_event_handle, VIEW_EVENT_BASE, VIEW_EVENT_IMG,
                                                             __json_event_handler, NULL, NULL));

    ESP_ERROR_CHECK(esp_event_handler_instance_register_with(view_event_handle, VIEW_EVENT_BASE, VIEW_EVENT_BOXES,
                                                             __json_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register_with(view_event_handle, VIEW_EVENT_BASE, VIEW_EVENT_KEYPOINTS,
                                                             __json_event_handler, NULL, NULL));

    esp32_rp2040_init();
    ESP_LOGI(TAG, "RP2040 init success");

    char* receivedStr = NULL;

    for (;;) {
        if (xQueueReceive(JsonQueue, &receivedStr, portMAX_DELAY) == pdPASS && receivedStr != NULL) {
            cJSON* receivedJson = cJSON_Parse(receivedStr);
            if (receivedJson != NULL) {
                process_json_data(receivedJson);
                cJSON_Delete(receivedJson);
            } else {
                ESP_LOGE(TAG, "Invalid JSON string: %s", receivedStr);
                vTaskDelay(pdMS_TO_TICKS(1));
            }
        }
    }


}