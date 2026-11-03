#pragma once

#include "esphome.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/uart/uart_component.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/light/addressable_light.h"

namespace esphome {
    namespace tx_ultimate_touch {
        const static uint8_t TOUCH_STATE_RELEASE = 1;
        const static uint8_t TOUCH_STATE_PRESS = 2;
        const static uint8_t TOUCH_STATE_SWIPE = 3;

        const static uint8_t TOUCH_STATE_ALL_FIELDS = 11;

        const static uint8_t TOUCH_STATE_SWIPE_RIGHT = 12;
        const static uint8_t TOUCH_STATE_SWIPE_LEFT = 13;

        struct TouchPoint {
            uint8_t x = -1;
            uint8_t state = -1;
        };

         // added 2/11/26 per gemini ------
         struct LedSlot {
          esphome::Color from_color{0, 0, 0};
          esphome::Color to_color{0, 0, 0};
          uint32_t start_time{0};
          uint32_t duration_ms{0};
          bool active{false};
         };

         inline LedSlot g_leds[28];

         inline void trigger_single_led(int index, uint8_t to_r, uint8_t to_g, uint8_t to_b, uint32_t duration, esphome::Color current_color) {
          if (index < 0 || index >= 28) return;
          g_leds[index].from_color = current_color;
          g_leds[index].to_color = esphome::Color(to_r, to_g, to_b);
          g_leds[index].start_time = millis();
          g_leds[index].duration_ms = duration;
          g_leds[index].active = true;
         }
         // ^^^^^ added 2/11/26 per gemini

        class TxUltimateTouch : public uart::UARTDevice, public Component {
        public:
            Trigger<TouchPoint> *get_touch_trigger() { return &this->touch_trigger_; }
            Trigger<TouchPoint> *get_release_trigger() { return &this->release_trigger_; }
            Trigger<TouchPoint> *get_swipe_left_trigger() { return &this->swipe_trigger_left_; }
            Trigger<TouchPoint> *get_swipe_right_trigger() { return &this->swipe_trigger_right_; }
            Trigger<TouchPoint> *get_multi_touch_release_trigger() { return &this->multi_touch_release_trigger_; }
            Trigger<TouchPoint> *get_long_touch_release_trigger() { return &this->long_touch_release_trigger_; }

            void set_uart_component(esphome::uart::UARTComponent *uart_component) {
                this->set_uart_parent(uart_component);
            }

            int get_top_led_for_button(int button, int button_count);
            int get_bottom_led_for_button(int button, int button_count);
            Color get_color_gradient(const Color &start, const Color &end, uint8_t step);

            void setup() override;
            void loop() override;
            void dump_config() override;

        protected:
            void send_touch_(TouchPoint tp);
            void handle_touch(uint8_t bytes[]);
            TouchPoint get_touch_point(uint8_t bytes[]);

            Trigger<TouchPoint> touch_trigger_;
            Trigger<TouchPoint> release_trigger_;
            Trigger<TouchPoint> swipe_trigger_left_;
            Trigger<TouchPoint> swipe_trigger_right_;
            Trigger<TouchPoint> multi_touch_release_trigger_;
            Trigger<TouchPoint> long_touch_release_trigger_;
        };
    }
}
