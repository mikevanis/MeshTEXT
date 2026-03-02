#include <Arduino.h>
#include "render.h"
#include "buttons.h"
#include "nav.h"

void setup() {
    Serial.begin(115200);
    Serial.println("MeshText starting...");

    renderInit();
    buttonsInit();
    navInit();

    Serial.println("Phase 1 ready. Controls:");
    Serial.println("  Click        = scroll directory");
    Serial.println("  Double-click = select page");
    Serial.println("  Long press   = back");
}

void loop() {
    buttonsCheck();
    ButtonEvent evt = buttonsGetEvent();

    if (evt != BTN_NONE) {
        const char* names[] = {"", "SHORT", "LONG", "DOUBLE", "VLONG"};
        Serial.printf("Button: %s\n", names[evt]);
    }

    navHandleButton(evt);
    navRender();
}
