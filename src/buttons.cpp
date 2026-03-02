#include "buttons.h"
#include "pins.h"
#include <Arduino.h>
#include <AceButton.h>

using namespace ace_button;

#define VERY_LONG_PRESS_MS 3000

static AceButton button((uint8_t)BTN_A);
static volatile ButtonEvent lastEvent = BTN_NONE;

// Very long press tracking (AceButton doesn't support two long-press thresholds)
static bool buttonDown = false;
static uint32_t pressStartMs = 0;
static bool veryLongFired = false;
static uint32_t bootTime = 0;

static void handleEvent(AceButton* /* btn */, uint8_t eventType, uint8_t /* state */) {
    if (veryLongFired) return;  // suppress events after very long press fires

    switch (eventType) {
        case AceButton::kEventClicked:
            lastEvent = BTN_SHORT;
            buttonDown = false;
            break;
        case AceButton::kEventLongPressed:
            // Fire immediately — if it becomes very long press, that overwrites this
            lastEvent = BTN_LONG;
            break;
        case AceButton::kEventLongReleased:
            buttonDown = false;
            break;
        case AceButton::kEventDoubleClicked:
            lastEvent = BTN_DOUBLE;
            buttonDown = false;
            break;
        case AceButton::kEventPressed:
            buttonDown = true;
            pressStartMs = millis();
            veryLongFired = false;
            break;
        case AceButton::kEventReleased:
            buttonDown = false;
            veryLongFired = false;
            break;
    }
}

void buttonsInit() {
    pinMode(BTN_A, INPUT_PULLUP);
    bootTime = millis();

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

    // Ignore everything for 1s after boot (GPIO0 can glitch during startup)
    if ((millis() - bootTime) < 1000) return;

    // Check for very long press (3s) while button is held
    if (buttonDown && !veryLongFired && pressStartMs > bootTime + 1000) {
        if ((millis() - pressStartMs) >= VERY_LONG_PRESS_MS) {
            veryLongFired = true;
            lastEvent = BTN_VERY_LONG;
        }
    }
}

ButtonEvent buttonsGetEvent() {
    ButtonEvent evt = lastEvent;
    lastEvent = BTN_NONE;
    return evt;
}
