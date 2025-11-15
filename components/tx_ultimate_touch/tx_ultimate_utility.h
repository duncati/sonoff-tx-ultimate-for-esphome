#include "esphome.h"

class MyComponent : public Component {
 public:
  int add_values(int a, int b) {
    return a + b;
  }
};
