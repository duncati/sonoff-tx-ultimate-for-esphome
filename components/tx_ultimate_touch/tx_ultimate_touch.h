#pragma once

#include "esphome/core/automation.h"
#include "esphome/core/color.h"
#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/light/addressable_light.h"

namespace esphome {
    namespace tx_ultimate_touch {
        const static uint8_t TOUCH_STATE_RELEASE = 1;
        const static uint8_t TOUCH_STATE_PRESS = 2;
        const static uint8_t TOUCH_STATE_SWIPE = 3;

        const static uint8_t TOUCH_STATE_ALL_FIELDS = 11;

        const static uint8_t TOUCH_STATE_SWIPE_RIGHT = 12;
        const static uint8_t TOUCH_STATE_SWIPE_LEFT = 13;

        const static int NUM_LEDS = 28;

        // How a fade moves from its start color to its target.
        enum FadeEase : uint8_t {
            EASE_IN_QUAD = 0,  // slow start, fast finish (default)
            EASE_LINEAR = 1,   // constant speed
        };

        struct TouchPoint {
            uint8_t x = -1;
            uint8_t state = -1;
        };

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
            // Fills `out` with the LED indices for `button` (bottom first, then top, each
            // only if enabled) and returns how many were written (0-2).
            int get_button_leds(int button, int button_count, bool bottom, bool top, int out[2]);

            // ---- LED fade engine ----
            // The light all fades are drawn on (set once, e.g. from on_boot).
            void set_light(light::AddressableLight *light) { this->light_ = light; }
            // Fade LED `index` to `to` over `duration_ms`, starting after `delay_ms`.
            // The start color is read from the LED when the fade actually begins,
            // so whatever is showing at that moment (touch, long-press, blink...) fades smoothly.
            void fade_led(int index, Color to, uint32_t duration_ms, uint32_t delay_ms = 0,
                          FadeEase ease = EASE_IN_QUAD);
            // Stop a fade, leaving the LED at whatever color it has reached.
            void cancel_fade(int index);
            void cancel_all_fades();
            // True while a fade is pending (in its delay) or running.
            bool is_fading(int index) const;

            void setup() override;
            void loop() override;
            void dump_config() override;

        protected:
            struct LedFade {
                Color from;
                Color to;
                uint32_t start{0};
                uint32_t duration{0};
                FadeEase ease{EASE_IN_QUAD};
                bool active{false};
                bool captured{false};
            };

            void read_uart_();
            void animate_();
            void send_touch_(TouchPoint tp);
            TouchPoint get_touch_point(const uint8_t bytes[]);

            Trigger<TouchPoint> touch_trigger_;
            Trigger<TouchPoint> release_trigger_;
            Trigger<TouchPoint> swipe_trigger_left_;
            Trigger<TouchPoint> swipe_trigger_right_;
            Trigger<TouchPoint> multi_touch_release_trigger_;
            Trigger<TouchPoint> long_touch_release_trigger_;

            // UART frame assembly
            uint8_t frame_[16]{};
            uint8_t frame_len_{0};

            LedFade fades_[NUM_LEDS];
            light::AddressableLight *light_{nullptr};
        };
    }
}
