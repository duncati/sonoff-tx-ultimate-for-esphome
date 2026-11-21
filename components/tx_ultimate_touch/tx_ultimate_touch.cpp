#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include "tx_ultimate_touch.h"

#include <algorithm>
#include <cinttypes>
#include <cstdio>

namespace esphome {
   namespace tx_ultimate_touch {
      static const char *const TAG = "tx_ultimate_touch";
      // Separate tags so each kind of debug output can be silenced in logger: logs:
      static const char *const TAG_RAW = "txu.raw";    // every complete touch-panel frame
      static const char *const TAG_FADE = "txu.fade";  // every fade queued, started, finished, cancelled

      // Frame layout: 170 85 | 1 (version) | 2 (opcode) | len | data[len] | 2-byte checksum (ignored)
      static const uint8_t HEADER[] = {170, 85, 1, 2};
      static const uint8_t HEADER_LEN = sizeof(HEADER);
      static const uint8_t MAX_DATA_LEN = 8;

      void TxUltimateTouch::setup() {
         ESP_LOGI(TAG, "Tx Ultimate Touch is initialized");
      }

      void TxUltimateTouch::loop() {
         this->read_uart_();
         this->animate_();
      }

      void TxUltimateTouch::dump_config() {
         ESP_LOGCONFIG(TAG, "Tx Ultimate Touch");
         ESP_LOGCONFIG(TAG, "  Fade light: %s", this->light_ != nullptr ? "set" : "NOT SET (call set_light)");
      }

      // ---------------------------------------------------------------------
      // UART: assemble frames byte by byte so packets that arrive together or
      // split across loop() calls are still parsed correctly.
      // ---------------------------------------------------------------------
      void TxUltimateTouch::read_uart_() {
         uint8_t byte;
         while (this->available() && this->read_byte(&byte)) {
            if (this->frame_len_ < HEADER_LEN) {
               if (byte == HEADER[this->frame_len_]) {
                  this->frame_[this->frame_len_++] = byte;
               } else {
                  // mismatch (e.g. checksum bytes or an error response): resync,
                  // treating this byte as a possible new frame start
                  this->frame_len_ = 0;
                  if (byte == HEADER[0]) {
                     this->frame_[this->frame_len_++] = byte;
                  }
               }
               continue;
            }

            this->frame_[this->frame_len_++] = byte;

            const uint8_t data_len = this->frame_[HEADER_LEN];
            if (data_len == 0 || data_len > MAX_DATA_LEN) {
               ESP_LOGW(TAG, "[%" PRIu32 "] Dropping frame with bad length %d", millis(), data_len);
               this->frame_len_ = 0;
               continue;
            }
            // header + length byte + data; the checksum that follows is skipped by resync
            if (this->frame_len_ == HEADER_LEN + 1 + data_len) {
               char hex[3 * MAX_DATA_LEN + 1] = {};
               for (uint8_t i = 0; i < data_len; i++) {
                  snprintf(hex + 3 * i, 4, "%02X ", this->frame_[HEADER_LEN + 1 + i]);
               }
               ESP_LOGD(TAG_RAW, "[%" PRIu32 "] frame len=%d data=%s", millis(), data_len, hex);
               this->send_touch_(this->get_touch_point(this->frame_));
               this->frame_len_ = 0;
            }
         }
      }

      void TxUltimateTouch::send_touch_(TouchPoint tp) {
         switch (tp.state) {
            case TOUCH_STATE_RELEASE:
               if (tp.x >= 17) {
                  tp.x -= 16;
                  ESP_LOGD(TAG, "[%" PRIu32 "] Long Press Release (x=%d)", millis(), tp.x);
                  this->long_touch_release_trigger_.trigger(tp);
               } else {
                  ESP_LOGD(TAG, "[%" PRIu32 "] Release (x=%d)", millis(), tp.x);
                  this->release_trigger_.trigger(tp);
               }
               break;

            case TOUCH_STATE_PRESS:
               ESP_LOGD(TAG, "[%" PRIu32 "] Press (x=%d)", millis(), tp.x);
               this->touch_trigger_.trigger(tp);
               break;

            case TOUCH_STATE_SWIPE_LEFT:
               ESP_LOGD(TAG, "[%" PRIu32 "] Swipe Left (x=%d)", millis(), tp.x);
               this->swipe_trigger_left_.trigger(tp);
               break;

            case TOUCH_STATE_SWIPE_RIGHT:
               ESP_LOGD(TAG, "[%" PRIu32 "] Swipe Right (x=%d)", millis(), tp.x);
               this->swipe_trigger_right_.trigger(tp);
               break;

            case TOUCH_STATE_ALL_FIELDS:
               ESP_LOGD(TAG, "[%" PRIu32 "] Multi Touch Release", millis());
               this->multi_touch_release_trigger_.trigger(tp);
               break;

            default:
               break;
         }
      }

      TouchPoint TxUltimateTouch::get_touch_point(const uint8_t bytes[]) {
         TouchPoint tp;
         tp.state = bytes[4];
         switch (bytes[4]) {
            // some releases are bytes[4]=2 && bytes[5]!=0 where x=bytes[6]
            // others releases are bytes[4]=1 where x=bytes[5]
            case TOUCH_STATE_PRESS:
               if (bytes[5] != 0) {
                  tp.state = TOUCH_STATE_RELEASE;
                  tp.x = bytes[5];
               } else {
                  tp.x = bytes[6];
               }
               break;
            case TOUCH_STATE_RELEASE:
               if (bytes[5] == 11) {
                  tp.state = TOUCH_STATE_ALL_FIELDS;
               } else {
                  tp.x = bytes[5];
               }
               break;
            case TOUCH_STATE_SWIPE:
               tp.x = 0;
               tp.state = bytes[5];
               break;
            default:
               ESP_LOGW(TAG, "[%" PRIu32 "] Tx Ultimate Touch unknown state %d", millis(), bytes[4]);
         }
         return tp;
      }

      // ---------------------------------------------------------------------
      // LED fade engine
      // ---------------------------------------------------------------------
      static inline uint8_t lerp(uint8_t a, uint8_t b, float t) {
         return static_cast<uint8_t>(a + (b - a) * t);
      }

      void TxUltimateTouch::fade_led(int index, Color to, uint32_t duration_ms, uint32_t delay_ms, FadeEase ease) {
         if (index < 0 || index >= NUM_LEDS) return;
         LedFade &f = this->fades_[index];
         ESP_LOGD(TAG_FADE, "[%" PRIu32 "] queue  led=%2d to=(%3d,%3d,%3d) dur=%" PRIu32 " delay=%" PRIu32 " ease=%s%s",
                  millis(), index, to.r, to.g, to.b, duration_ms, delay_ms,
                  ease == EASE_LINEAR ? "linear" : "quad", f.active ? " (replaces active fade)" : "");
         f.to = to;
         f.start = millis() + delay_ms;
         f.duration = duration_ms;
         f.ease = ease;
         f.captured = false;
         f.active = true;
      }

      void TxUltimateTouch::cancel_fade(int index) {
         if (index < 0 || index >= NUM_LEDS) return;
         if (this->fades_[index].active) {
            ESP_LOGD(TAG_FADE, "[%" PRIu32 "] cancel led=%2d%s", millis(), index,
                     this->fades_[index].captured ? " (mid-fade, LED keeps its current color)" : " (was still waiting)");
         }
         this->fades_[index].active = false;
      }

      void TxUltimateTouch::cancel_all_fades() {
         ESP_LOGD(TAG_FADE, "[%" PRIu32 "] cancel all", millis());
         for (auto &f : this->fades_) f.active = false;
      }

      bool TxUltimateTouch::is_fading(int index) const {
         return index >= 0 && index < NUM_LEDS && this->fades_[index].active;
      }

      void TxUltimateTouch::animate_() {
         if (this->light_ == nullptr) return;
         const uint32_t now = millis();
         const int count = std::min(NUM_LEDS, static_cast<int>(this->light_->size()));
         bool dirty = false;

         for (int i = 0; i < count; i++) {
            LedFade &f = this->fades_[i];
            if (!f.active) continue;
            // signed difference: negative while still in the delay, and safe across millis() wrap
            const int32_t elapsed = static_cast<int32_t>(now - f.start);
            if (elapsed < 0) continue;

            if (!f.captured) {
               f.from = (*this->light_)[i].get();
               f.captured = true;
               ESP_LOGD(TAG_FADE, "[%" PRIu32 "] start  led=%2d from=(%3d,%3d,%3d) to=(%3d,%3d,%3d)",
                        now, i, f.from.r, f.from.g, f.from.b, f.to.r, f.to.g, f.to.b);
            }

            float t = f.duration > 0 ? static_cast<float>(elapsed) / f.duration : 1.0f;
            if (t >= 1.0f) {
               t = 1.0f;
               f.active = false;
               ESP_LOGD(TAG_FADE, "[%" PRIu32 "] done   led=%2d at=(%3d,%3d,%3d)", now, i, f.to.r, f.to.g, f.to.b);
            }
            const float k = f.ease == EASE_LINEAR ? t : t * t;  // EASE_IN_QUAD: slow start, fast finish
            (*this->light_)[i] = Color(lerp(f.from.r, f.to.r, k),
                                       lerp(f.from.g, f.to.g, k),
                                       lerp(f.from.b, f.to.b, k));
            dirty = true;
         }

         if (dirty) this->light_->schedule_show();
      }

      // ---------------------------------------------------------------------
      // Helpers
      // ---------------------------------------------------------------------
      int TxUltimateTouch::get_button_leds(int button, int button_count, bool bottom, bool top, int out[2]) {
         int n = 0;
         if (bottom) {
            int led = this->get_bottom_led_for_button(button, button_count);
            if (led >= 0 && led < NUM_LEDS) out[n++] = led;
         }
         if (top) {
            int led = this->get_top_led_for_button(button, button_count);
            if (led >= 0 && led < NUM_LEDS) out[n++] = led;
         }
         return n;
      }

      Color TxUltimateTouch::get_color_gradient(const Color &start, const Color &end, uint8_t step) {
         if (step > 7) {
            step = 7;
         }
         float t = step / 7.0f; // 8 total steps, so 7 intervals
         return Color(lerp(start.r, end.r, t),
                      lerp(start.g, end.g, t),
                      lerp(start.b, end.b, t),
                      lerp(start.w, end.w, t)
         );
      }

      int TxUltimateTouch::get_top_led_for_button(int button, int button_count) {
         switch(button_count) {
            case 4: return 18 + 2*button;
            case 3: return 19 + 2*button;
            case 2: return 17 + 4*button;
            case 1: return 23;
         }
         return -1;
      }

      int TxUltimateTouch::get_bottom_led_for_button(int button, int button_count) {
         switch(button_count) {
            case 4: return 14 - 2*button;
            case 3: return 13 - 2*button;
            case 2: return 15 - 4*button;
            case 1: return 9;
         }
         return -1;
      }
   }
}
