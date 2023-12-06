#include "driver/rmt_tx.h"

#define LED_GPIO 4
#define LED_RESOLUTION_HZ 10000000 // 10MHz resolution, 1 tick = 0.1us (led strip needs a high resolution)
#define MAX_LEDS 64

struct Leds {

    rmt_channel_handle_t chan;
    rmt_tx_channel_config_t tx_chan_config;
    rmt_encoder_handle_t encoder;

    uint8_t n_pixels;
    uint8_t pixels[MAX_LEDS * 3];
    uint8_t scratch[MAX_LEDS * 3];
    
    uint8_t out_buffer[MAX_LEDS * 3];

};


#ifndef IM_LED
extern struct Leds leds;
#endif

void setup_led();
void sync(bool wait);
void set_n_pixels(uint8_t num);
void set_pixel(uint8_t num, uint8_t r, uint8_t g, uint8_t b);
void led_blur(uint8_t n);
void led_clear();