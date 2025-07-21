#pragma once

std::array<int, 2> get_led_and_button_at_x(int x) {
   int button = 0;
   int led_index = 0;
   if (id(relay_count)==4) {
      switch (x) {
         case 1:
         case 2:
            button = 1;
            led_index = 12;
            break;
         case 3:
         case 4:
         case 5:
            button = 2;
            led_index = 10;
            break;
         case 6:
         case 7:
         case 8:
            button = 3;
            led_index = 8;
            break;
         case 9:
         case 10:
            button = 4;
            led_index = 6;
            break;
         default:
            ESP_LOGW("script", "Unexpected X: %d", x);
            return;
      }
   } else if (id(relay_count)==3) {
      switch (x) {
         case 1:
         case 2:
         case 3:
            button = 1;
            led_index = 11;
            break;
         case 4:
         case 5:
         case 6:
            button = 2;
            led_index = 9;
            break;
         case 7:
         case 8:
         case 9:
         case 10:
            button = 3;
            led_index = 7;
            break;
         default:
            ESP_LOGW("script", "Unexpected X: %d", x);
            return;
      }
   } else if (id(relay_count)==2) {
      switch (x) {
         case 1:
         case 2:
         case 3:
         case 4:
         case 5:
            button = 1;
            led_index = 11;
            break;
         case 6:
         case 7:
         case 8:
         case 9:
         case 10:
            button = 2;
            led_index = 7;
            break;
         default:
            ESP_LOGW("script", "Unexpected X: %d", x);
            return;
      }
   } else {
      button = 1;
      led_index = 9;
   }
   return {led_index, button};
}
