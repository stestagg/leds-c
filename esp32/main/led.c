#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"

#define IM_LED 1
#include "led_driver.h"
#include "led.h"

static const char *TAG = "led";

const uint8_t gamma8[] = {
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  1,  1,  1,  1,
    1,  1,  1,  1,  1,  1,  1,  1,  1,  2,  2,  2,  2,  2,  2,  2,
    2,  3,  3,  3,  3,  3,  3,  3,  4,  4,  4,  4,  4,  5,  5,  5,
    5,  6,  6,  6,  6,  7,  7,  7,  7,  8,  8,  8,  9,  9,  9, 10,
   10, 10, 11, 11, 11, 12, 12, 13, 13, 13, 14, 14, 15, 15, 16, 16,
   17, 17, 18, 18, 19, 19, 20, 20, 21, 21, 22, 22, 23, 24, 24, 25,
   25, 26, 27, 27, 28, 29, 29, 30, 31, 32, 32, 33, 34, 35, 35, 36,
   37, 38, 39, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 50,
   51, 52, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 66, 67, 68,
   69, 70, 72, 73, 74, 75, 77, 78, 79, 81, 82, 83, 85, 86, 87, 89,
   90, 92, 93, 95, 96, 98, 99,101,102,104,105,107,109,110,112,114,
  115,117,119,120,122,124,126,127,129,131,133,135,137,138,140,142,
  144,146,148,150,152,154,156,158,160,162,164,167,169,171,173,175,
  177,180,182,184,186,189,191,193,196,198,200,203,205,208,210,213,
  215,218,220,223,225,228,231,233,236,239,241,244,247,249,252,255 };

struct Leds leds = { 0 };

void setup_led() {
    ESP_LOGI(TAG, "Create RMT TX channel");
    leds.tx_chan_config.clk_src = RMT_CLK_SRC_DEFAULT;
    leds.tx_chan_config.gpio_num = LED_GPIO;
    leds.tx_chan_config.mem_block_symbols = 64;  // increasing might reduce flicker
    leds.tx_chan_config.resolution_hz = LED_RESOLUTION_HZ;
    leds.tx_chan_config.trans_queue_depth = 4; // num pending txns
    //leds.tx_chan_config.flags.invert_out = true;
    ESP_ERROR_CHECK(rmt_new_tx_channel(&leds.tx_chan_config, &leds.chan));

    ESP_LOGI(TAG, "Install led strip encoder");
    led_strip_encoder_config_t encoder_config = {
        .resolution = LED_RESOLUTION_HZ,
    };
    ESP_ERROR_CHECK(rmt_new_led_strip_encoder(&encoder_config, &leds.encoder));

    ESP_LOGI(TAG, "Enable RMT TX channel");
    ESP_ERROR_CHECK(rmt_enable(leds.chan));
}

void sync(bool wait) {
    for (int i = 0; i < leds.n_pixels * 3; ++i) {
        uint8_t pixel_val = leds.pixels[i];
        leds.out_buffer[i] = gamma8[pixel_val];
    }
    rmt_transmit_config_t tx_config = {
        .loop_count = 0, // no transfer loop
    };
    ESP_ERROR_CHECK(rmt_transmit(leds.chan, leds.encoder, leds.out_buffer, leds.n_pixels *3, &tx_config));
    if (wait) {
        ESP_ERROR_CHECK(rmt_tx_wait_all_done(leds.chan, portMAX_DELAY));
    }
}

void set_n_pixels(uint8_t num) {
    if (num < leds.n_pixels) {
        size_t offset = num * 3;
        size_t to_clear = (MAX_LEDS * 3) - offset;
        memset(&leds.pixels[offset], 0, to_clear);
        sync(false);
    }
    leds.n_pixels = num;
}

void set_pixel(uint8_t num, uint8_t r, uint8_t g, uint8_t b) {
    leds.pixels[num * 3 + 0] = g;
    leds.pixels[num * 3 + 1] = b;
    leds.pixels[num * 3 + 2] = r;
}

void led_clear() {
    memset(leds.pixels, 0, leds.n_pixels * 3);
}

static const uint8_t kernel1[9] = {71 ,113, 71};
static const uint8_t kernel2[9] = {39 ,56, 64, 56, 39};
static const uint8_t kernel3[9] = {27 ,36, 42, 45, 42, 36, 27};
static const uint8_t kernel4[9] = {21, 26, 30, 33, 34, 33, 30, 26, 21};


void led_blur(uint8_t n) {
    n = (n > 4) ? 4 : n;
    if (n < 1) { return;}
    const uint8_t *kernel;
    switch(n) {
        case 1: kernel = kernel1; break;
        case 2: kernel = kernel2; break;
        case 3: kernel = kernel3; break;
        case 4: kernel = kernel4; break;
        default: return;
    }
    const int kernel_size = (n * 2) + 1;
    const int offset = n;

    for (int i=0; i < leds.n_pixels; ++i) {
        uint16_t acc[3] = {0, 0, 0};
        for (int j=0; j < kernel_size; ++j) {
            uint8_t kernel_val = kernel[j];
            int16_t pixel = (i + j) - offset;
            while (pixel < 0) pixel += leds.n_pixels;
            while (pixel >= leds.n_pixels) pixel -= leds.n_pixels;
            uint8_t *pixel_base = &leds.pixels[pixel * 3];
            for (int z=0; z < 3; ++z) {
                const uint8_t pixel_val = pixel_base[z];
                acc[z] += (pixel_val * kernel_val);
            }
        }
        leds.scratch[i * 3] = acc[0] / 255;
        leds.scratch[(i * 3) + 1] = acc[1] / 255;
        leds.scratch[(i * 3) + 2] = acc[2] / 255;
    }
    memcpy(leds.pixels, leds.scratch, leds.n_pixels * 3);
}