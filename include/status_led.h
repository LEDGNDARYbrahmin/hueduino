#ifndef STATUS_LED_H
#define STATUS_LED_H

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "config_defaults.h"

// LED States
enum StatusLedState {
    LED_STATE_BOOT,
    LED_STATE_IDLE,
    LED_STATE_LINK,
    LED_STATE_ERROR,
    LED_STATE_STREAM,
    LED_STATE_WIFI_CONFIG
};

// Animation Types
enum LedAnimation {
    ANIM_SOLID,
    ANIM_BLINK,
    ANIM_BREATHE
};

class StatusLed {
public:
    StatusLed();
    
    void begin();
    void update();
    void setState(StatusLedState state);
    
private:
    Adafruit_NeoPixel _strip;
    StatusLedState _currentState;
    LedAnimation _currentAnim;
    uint32_t _targetColor;
    
    unsigned long _lastUpdate;
    uint16_t _animStep;
    bool _animDir; // true = up, false = down
    
    void _setAnimation(StatusLedState state);
    void _updateAnimation();
    uint32_t _getColorForState(StatusLedState state);
};

extern StatusLed statusLed;

#endif // STATUS_LED_H
