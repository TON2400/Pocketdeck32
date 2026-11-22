#include <Arduino.h>
#include "core/SystemManager.h"

void setup() {
    g_system.begin();
}

void loop() {
    g_system.update();
}
