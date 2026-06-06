#pragma once

#include <stdint.h>
#include "esp_err.h"

typedef struct {
    uint32_t frames_read;
    uint32_t samples_read;
    int32_t mean_abs;
    int16_t peak_abs;
    int16_t min_sample;
    int16_t max_sample;
} audio_pipeline_self_test_result_t;

esp_err_t audio_pipeline_init(void);
esp_err_t audio_pipeline_self_test(audio_pipeline_self_test_result_t *result);
esp_err_t audio_pipeline_start(uint32_t session_id);
esp_err_t audio_pipeline_stop(void);
uint32_t audio_pipeline_session_id(void);
