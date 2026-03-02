#include "buttons.h"
#include "pins.h"
#include <Arduino.h>
#include <AceButton.h>

using namespace ace_button;

static AceButton button((uint8_t)BTN_A);
static volatile ButtonEvent lastEvent = BTN_NONE;

static void handleEvent(AceButton* /* btn */, uint8_t eventType, uint8_t /* state */) {
    switch (eventType) {
        case AceButton::kEventClicked:
            lastEvent = BTN_SHORT;
            break;
        case AceButton::kEventLongPressed:
            lastEvent = BTN_LONG;
            break;
        case AceButton::kEventDoubleClicked:
            lastEvent = BTN_DOUBLE;
            break;
        case AceButton::kEventLongReleased:
            // We use this for very long press detection:
            // kEventLongPressed fires first, then if held longer we could
            // differentiate. For now, use a separate approach below.
            break;
    }
}

void buttonsInit() {
    pinMode(BTN_A, INPUT_PULLUP);

    ButtonConfig* config = button.getButtonConfig();
    config->setEventHandler(handleEvent);
    config->setFeature(ButtonConfig::kFeatureClick);
    config->setFeature(ButtonConfig::kFeatureDoubleClick);
    config->setFeature(ButtonConfig::kFeatureLongPress);
    config->setFeature(ButtonConfig::kFeatureSuppressAfterClick);
    config->setFeature(ButtonConfig::kFeatureSuppressAfterDoubleClick);
    config->setFeature(ButtonConfig::kFeatureSuppressClickBeforeDoubleClick);
    config->setFeature(ButtonConfig::kFeatureSuppressAfterLongPress);
}

void buttonsCheck() {
    button.check();
}

ButtonEvent buttonsGetEvent() {
    ButtonEvent evt = lastEvent;
    lastEvent = BTN_NONE;
    return evt;
}
