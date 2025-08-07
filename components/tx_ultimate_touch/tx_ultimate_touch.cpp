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

      void TxUltimateTouch::setup() {
         ESP_LOGI("log", "%s", "Tx Ultimate Touch is initialized");
      }

      void TxUltimateTouch::loop() {
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
         for (uint8_t i = 4; i < 11; i++) {
            len += snprintf(buf+len, sizeof(buf)-len, "%d ", bytes[i]);
         }
         // TODO set this back to LOGV or wrap it in a "if log level" or comment it out
         ESP_LOGD(TAG, "Read bytes: %s", buf);
         send_touch_(get_touch_point(bytes));
      }

      void TxUltimateTouch::dump_config() {
         ESP_LOGCONFIG(TAG, "Tx Ultimate Touch");
      }

      void TxUltimateTouch::send_touch_(TouchPoint tp) {
         uint8_t response[8] = {170, 85, 1, 2, 1, 1};
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
               append_crc16_modbus(response, 6, 8);
               write_array(response, 8);
               flush();
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
               ESP_LOGW("main", "Tx Ultimate Touch unknown state %d", bytes[4]);
         }
         return tp;
      }

      // Compute CRC16/MODBUS (poly 0x8005 reversed: 0xA001)
      uint16_t TxUltimateTouch::crc16_modbus(const uint8_t* data, size_t length) {
         uint16_t crc = 0xFFFF;
         for (size_t i = 0; i < length; ++i) {
            crc ^= data[i];
            for (int j = 0; j < 8; j++) {
               if (crc & 0x0001)
                  crc = (crc >> 1) ^ 0xA001;
               else
                  crc = crc >> 1;
            }
         }
         return crc;
      }

      // Appends CRC to array (if space allows)
      int TxUltimateTouch::append_crc16_modbus(uint8_t *data, size_t data_len, size_t max_len) {
         if (data_len + 2 > max_len) {
            return -1; // Not enough space to append CRC
         }

         uint16_t crc = crc16_modbus(data, data_len);
         data[data_len]     = crc & 0xFF;       // Low byte
         data[data_len + 1] = (crc >> 8) & 0xFF; // High byte

         return 0; // Success
      }
   }
}
