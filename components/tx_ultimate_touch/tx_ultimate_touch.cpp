#include "esphome/core/log.h"
#include "tx_ultimate_touch.h"

namespace esphome {
   namespace tx_ultimate_touch {
      #define MIN(a, b) (((a) < (b)) ? (a) : (b))
      static const char *TAG = "tx_ultimate_touch";
      // 170, 85 is the header, 1 is the version, 2 is the opcode (it's always 2)
      // after the header there's data length (1 or 2) followed by 1 or 2 bytes of data
      // after that is a 2 byte checksum (ignored)
      static const uint8_t HEADER[] = {170, 85, 1, 2};
      static const int BUFFER_SIZE = 16;

      void TxUltimateTouch::setup()
      {
         ESP_LOGI("log", "%s", "Tx Ultimate Touch is initialized");
      }

      void TxUltimateTouch::loop()
      {
         bool found = false;

         uint8_t bytes[BUFFER_SIZE] = {};

         int avail;
         while ((avail = available())) {
            ESP_LOGD(TAG, "avail=%d", avail);
            read_array(bytes, MIN(avail, BUFFER_SIZE));
            if (memcmp(bytes, HEADER, 4) == 0) {
               handle_touch(bytes);
            }
         }
      }

      void TxUltimateTouch::handle_touch(uint8_t bytes[]) {
         char buf[64];
         int len = 0;
         for (uint8_t i = 0; i < 15; i++) {
            len += snprintf(buf+len, sizeof(buf)-len, "%d ", bytes[i]);
         }
         // TODO set this back to LOGV
         ESP_LOGD(TAG, "Read bytes(%d): %s", len, buf);

         send_touch_(get_touch_point(bytes));
      }

      void TxUltimateTouch::dump_config() {
         ESP_LOGCONFIG(TAG, "Tx Ultimate Touch");
      }

      void TxUltimateTouch::send_touch_(TouchPoint tp) {
         ESP_LOGD(TAG, "send touch");
         switch (tp.state) {
            case TOUCH_STATE_RELEASE:
               if (tp.x >= 17) {
                  tp.x -= 16;
                  ESP_LOGD(TAG, "Long Press Release (x=%d)", tp.x);
                  this->long_touch_release_trigger_.trigger(tp);
               } else {
                  ESP_LOGD(TAG, "Release (x=%d)", tp.x);
                  this->release_trigger_.trigger(tp);
               }
               break;

            case TOUCH_STATE_PRESS:
               ESP_LOGD(TAG, "Press (x=%d)", tp.x);
               this->touch_trigger_.trigger(tp);
               break;

            case TOUCH_STATE_SWIPE_LEFT:
               ESP_LOGD(TAG, "Swipe Left (x=%d)", tp.x);
               this->swipe_trigger_left_.trigger(tp);
               break;

            case TOUCH_STATE_SWIPE_RIGHT:
               ESP_LOGD(TAG, "Swipe Right (x=%d)", tp.x);
               this->swipe_trigger_right_.trigger(tp);
               break;

            case TOUCH_STATE_ALL_FIELDS:
               ESP_LOGD(TAG, "Multi Touch Release");
               this->multi_touch_release_trigger_.trigger(tp);
               break;

            default:
               break;
         }
      }

      TouchPoint TxUltimateTouch::get_touch_point(uint8_t bytes[]) {
         TouchPoint tp;
         tp.state = bytes[4];
         ESP_LOGD(TAG, "get touch point");
         switch (bytes[4]) {
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
            case TOUCH_STATE_PRESS:
               tp.x = bytes[6];
               break;
            default:
               ESP_LOGW("main", "Tx Ultimate Touch unknown state %d", bytes[4]);
         }
         return tp;
      }
   }
}
