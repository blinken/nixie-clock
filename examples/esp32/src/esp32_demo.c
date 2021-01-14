/*
 * Example driver for the MAX6921, controlling an old-school Soviet IV-18 VFD
 * tube.
 *
 * See https://github.com/blinken/nixie-clock for more information.
 *
 * This file is copyright 2021 Patrick Coleman.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is furnished to do
 * so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "sys/time.h"
#include "lwip/err.h"

#include "driver/ledc.h"
#include "driver/spi_master.h"
#include "driver/timer.h"

#define DISP_CLK   21
#define DISP_DATA  22
#define DISP_LOAD  17
#define DISP_BLANK 16

#define DISP_BUF_WIDTH 3
#define DISP_WIDTH 10 // width including minus sign

static volatile spi_device_handle_t display_h;
static char display_contents[DISP_WIDTH];
static volatile uint8_t display_idx;
static unsigned char * display_buf;

void IRAM_ATTR display_blank() {
  gpio_set_level(DISP_BLANK, 1); // blank
}

void IRAM_ATTR display_unblank() {
  gpio_set_level(DISP_BLANK, 0); // unblank
}

void display_init() {

  display_buf = malloc(DISP_BUF_WIDTH);

  gpio_set_direction(DISP_LOAD, GPIO_MODE_OUTPUT);
  gpio_set_level(DISP_LOAD, 1); // output unfrozen

  gpio_set_direction(DISP_BLANK, GPIO_MODE_OUTPUT);
  display_blank();

  spi_bus_config_t bus_config;
  bus_config.sclk_io_num = DISP_CLK;
  bus_config.mosi_io_num = DISP_DATA;
  bus_config.miso_io_num = -1; // MISO, not used
  bus_config.quadwp_io_num = -1;
  bus_config.quadhd_io_num = -1;
  bus_config.max_transfer_sz = 0;
  bus_config.flags = 0;
  bus_config.intr_flags = 0;
  ESP_ERROR_CHECK(spi_bus_initialize(HSPI_HOST, &bus_config, 1));

  spi_device_handle_t handle;
  spi_device_interface_config_t dev_config;
  dev_config.address_bits = 0;
  dev_config.command_bits = 0;
  dev_config.dummy_bits = 0;
  dev_config.mode = 0;
  dev_config.duty_cycle_pos = 0;
  dev_config.cs_ena_posttrans = 0;
  dev_config.cs_ena_pretrans = 0;
  dev_config.clock_speed_hz = 800000; // 800kHz
  dev_config.input_delay_ns = 0;
  dev_config.spics_io_num = -1;
  dev_config.flags = 0;
  dev_config.queue_size = 3;
  dev_config.pre_cb = display_blank;
  dev_config.post_cb = display_unblank;
  ESP_ERROR_CHECK(spi_bus_add_device(HSPI_HOST, &dev_config, &handle));

  display_h = handle;
}

// Write 24 bits of data to the display. Data must point to at least four bytes
void IRAM_ATTR display_write(uint32_t data) {
  memcpy(display_buf, &data, DISP_BUF_WIDTH);

  spi_transaction_t trans_desc;
  trans_desc.addr = 0;
  trans_desc.cmd = 0;
  trans_desc.flags = 0;
  trans_desc.length = DISP_BUF_WIDTH * 8;
  trans_desc.rxlength = 0;
  trans_desc.tx_buffer = display_buf;
  trans_desc.rx_buffer = display_buf;

  spi_device_queue_trans(display_h, &trans_desc, 0);
}

void display_free() {
  display_blank();

  ESP_ERROR_CHECK(spi_bus_remove_device(display_h));
  ESP_ERROR_CHECK(spi_bus_free(HSPI_HOST));

  free(display_buf);
}

#define GRID_7     0x800000
#define GRID_6     0x400000
#define GRID_5     0x200000
#define GRID_4     0x100000
#define GRID_3     0x080000
#define GRID_2     0x040000
#define GRID_1     0x020000
#define GRID_0     0x010000
#define GRID_SIGN  0x000100
#define GRID_ALL   0xff0100

#define SEG_DEGREE 0x000001
#define SEG_MINUS  0x008000
#define SEG_DOT    0x000001
#define SEG_G      0x008000
#define SEG_F      0x004000
#define SEG_E      0x002000
#define SEG_D      0x001000
#define SEG_C      0x000800
#define SEG_B      0x000400
#define SEG_A      0x000200

#define DIGIT_0   SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F
#define DIGIT_1   SEG_B | SEG_C
#define DIGIT_2   SEG_A | SEG_B | SEG_D | SEG_E | SEG_G
#define DIGIT_3   SEG_A | SEG_B | SEG_C | SEG_D | SEG_G
#define DIGIT_4   SEG_B | SEG_C | SEG_F | SEG_G
#define DIGIT_5   SEG_A | SEG_C | SEG_D | SEG_F | SEG_G
#define DIGIT_6   SEG_A | SEG_C | SEG_D | SEG_E | SEG_F | SEG_G
#define DIGIT_7   SEG_A | SEG_B | SEG_C
#define DIGIT_8   SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F | SEG_G
#define DIGIT_9   SEG_A | SEG_B | SEG_C | SEG_D | SEG_F | SEG_G
#define DIGIT_A   SEG_A | SEG_B | SEG_C | SEG_E | SEG_F | SEG_G
#define DIGIT_B   SEG_C | SEG_D | SEG_E | SEG_F | SEG_G
#define DIGIT_C   SEG_A | SEG_D | SEG_E | SEG_F
#define DIGIT_D   SEG_B | SEG_C | SEG_D | SEG_E | SEG_G
#define DIGIT_E   SEG_A | SEG_D | SEG_E | SEG_F | SEG_G
#define DIGIT_F   SEG_A | SEG_E | SEG_F | SEG_G
#define DIGIT_ALL SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F | SEG_G | SEG_DOT

int characters[] = {DIGIT_0, DIGIT_1, DIGIT_2, DIGIT_3, DIGIT_4, DIGIT_5, DIGIT_6, DIGIT_7, DIGIT_8, DIGIT_9, DIGIT_A, DIGIT_B, DIGIT_C, DIGIT_D, DIGIT_E, DIGIT_F};
int grids[] = {GRID_7, GRID_6, GRID_5, GRID_4, GRID_3, GRID_2, GRID_1, GRID_0};
int segments[] = {SEG_A, SEG_B, SEG_C, SEG_D, SEG_E, SEG_F, SEG_G, SEG_DOT};

int IRAM_ATTR char_to_code(char c) {
  if ((c <= 0x46) && (c >= 0x41)) { // A-F
    return characters[c - 0x41 + 10];
  } else if ((c <= 0x66) && (c >= 0x61)) { // a-f
    return characters[c - 0x61 + 10];
  } else if ((c <= 0x39) && (c >= 0x30)) { // a-f
    return characters[c - 0x30];
  } else if (c == '.') {
    return SEG_DOT;
  } else if (c == '*') {
    return SEG_DEGREE;
  } else if (c == '-') {
    return SEG_MINUS;
  } else if (c == ' ') {
    return 0x0;
  }

  return DIGIT_A;
}

/*
 * Callback triggered by 500Hz timer.
 *
 * Lights the digit pointed at by display_idx with the appropriate segments.
 * Other digits will be "dark", but we're moving so fast we'll refresh them
 * before anyone notices!
 */
void IRAM_ATTR display_refresh(void *param) {
  int j;

  TIMERG0.int_ena.val = 0;
  TIMERG0.hw_timer[0].update = 1;
  TIMERG0.int_clr_timers.t0 = 1;
  TIMERG0.hw_timer[0].config.alarm_en = 0;

  /*
  // Uncomment for clock based on ESP32 local time. This will update
  // display_contents (what gets written to the display) with the local time on
  // the ESP32. You will need to set the local time somehow - manually, or via SNTP.
  struct tm timeinfo;
  time_t now;

  memset(display_contents, ' ', DISP_WIDTH);
  time(&now);
  localtime_r(&now, &timeinfo);
  strftime(display_contents, DISP_WIDTH, " %H %M %S", &timeinfo);
  */

  // Special case to display sign or degree
  if ((display_idx == 0) && ((*display_contents == '-') || (*display_contents == '*'))) {
    j = GRID_SIGN | char_to_code(*display_contents);
  } else {
    j = grids[display_idx-1] | char_to_code(display_contents[display_idx]);
  }
  display_write(j);

  display_idx++;
  if (display_idx >= DISP_WIDTH-1)
    display_idx = 0;

  TIMERG0.hw_timer[0].config.alarm_en = 1;
  TIMERG0.int_ena.val = 1;
}

/* 
 * The display must be continuously refreshed (faster than the human eye can
 * notice). Achieve this with a 500Hz timer and a callback to display_refresh.
 */
void display_timer_init() {
  timer_config_t timer_conf;
  timer_conf.alarm_en = 1;
  timer_conf.counter_en = 1;
  timer_conf.intr_type = TIMER_INTR_LEVEL;
  timer_conf.counter_dir = TIMER_COUNT_UP;
  timer_conf.auto_reload = 1;
  timer_conf.divider = 80; // 80Mhz / 80 = 1Mhz
  timer_init(TIMER_GROUP_0, TIMER_0, &timer_conf);

  timer_pause(TIMER_GROUP_0, TIMER_0);
  timer_set_counter_value(TIMER_GROUP_0, TIMER_0, 0x00000000ULL);

  // We're refreshing 9 grids, which is 9 timer cycles. For an imperceptible
  // display refresh (>50Hz) we want a timer frequency of at least 50*9=450Hz.
  timer_set_alarm_value(TIMER_GROUP_0, TIMER_0, 2000); // 1Mhz / 2000 = 500Hz 
  timer_set_auto_reload(TIMER_GROUP_0, TIMER_0, TIMER_AUTORELOAD_EN);

  timer_enable_intr(TIMER_GROUP_0, TIMER_0);
  timer_isr_register(TIMER_GROUP_0, TIMER_0, display_refresh, NULL, ESP_INTR_FLAG_IRAM, NULL);
  timer_start(TIMER_GROUP_0, TIMER_0);

  // Turn the display on
  gpio_set_level(DISP_LOAD, 1);
  display_unblank();
}


void app_main()
{
  nvs_flash_init();

  printf("* Main: Initialising display control\n");
  display_init();

  strncpy(display_contents, "*DEADBEEF", DISP_WIDTH);
  display_timer_init();

  // sleep forever
  printf("* Main: task sleeping.\n");
  vTaskSuspend(NULL);
}
