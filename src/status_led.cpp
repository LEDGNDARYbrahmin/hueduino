#include "status_led.h"

// Initialize NeoPixel with pin from config
StatusLed statusLed;

StatusLed::StatusLed() : 
    _strip(STATUS_LED_COUNT, STATUS_LED_PIN, NEO_RGB + NEO_KHZ800),
    _currentState(LED_STATE_BOOT),
    _currentAnim(ANIM_BREATHE),
    _lastUpdate(0),
    _animStep(0),
    _animDir(true) {
}

void StatusLed::begin() {
    _strip.begin();
    _strip.show(); // Initialize all pixels to 'off'
    setState(LED_STATE_BOOT);
}

void StatusLed::setState(StatusLedState state) {
    _currentState = state;
    _setAnimation(state);
    _targetColor = _getColorForState(state);
    
    // Reset animation
    _animStep = 0;
    _animDir = true;
    _updateAnimation();
}

void StatusLed::update() {
    if (millis() - _lastUpdate < 20) {
        return;
    }
    _lastUpdate = millis();
    _updateAnimation();
}

void StatusLed::_setAnimation(StatusLedState state) {
    switch (state) {
        case LED_STATE_BOOT:
        case LED_STATE_WIFI_CONFIG:
            _currentAnim = ANIM_BREATHE;
            break;
        case LED_STATE_IDLE:
        case LED_STATE_STREAM:
            _currentAnim = ANIM_SOLID;
            break;
        case LED_STATE_LINK:
        case LED_STATE_ERROR:
            _currentAnim = ANIM_BLINK;
            break;
    }
}

uint32_t StatusLed::_getColorForState(StatusLedState state) {
    switch (state) {
        case LED_STATE_BOOT:        return _strip.Color(0, 0, 255);    // Blue
        case LED_STATE_IDLE:        return _strip.Color(0, 255, 255);  // Teal
        case LED_STATE_LINK:        return _strip.Color(0, 255, 0);    // Green
        case LED_STATE_ERROR:       return _strip.Color(255, 0, 0);    // Red
        case LED_STATE_STREAM:      return _strip.Color(128, 0, 128);  // Purple
        case LED_STATE_WIFI_CONFIG: return _strip.Color(255, 165, 0);  // Orange
        default:                    return _strip.Color(0, 0, 0);
    }
}

void StatusLed::_updateAnimation() {
    uint32_t color = _targetColor;
    uint8_t r = (uint8_t)(color >> 16);
    uint8_t g = (uint8_t)(color >> 8);
    uint8_t b = (uint8_t)color;
    
    switch (_currentAnim) {
        case ANIM_SOLID:
            // Constant color
            break;
            
        case ANIM_BREATHE:
            // Smooth breathing
            if (_animDir) {
                _animStep += 2;
                if (_animStep >= 255) _animDir = false;
            } else {
                _animStep -= 2;
                if (_animStep <= 20) _animDir = true;
            }
            // Scale color by breathing step
            r = (r * _animStep) / 255;
            g = (g * _animStep) / 255;
            b = (b * _animStep) / 255;
            break;
            
        case ANIM_BLINK:
            // Fast blink
            _animStep++;
            if (_animStep > 20) _animStep = 0; // Reset every ~400ms
            
            if (_animStep < 10) {
                // On - keep original color
            } else {
                // Off
                r = 0; g = 0; b = 0;
            }
            break;
    }
    
    // Apply global brightness scaling
    float brightness = STATUS_LED_BRIGHTNESS / 255.0f;
    r = (uint8_t)(r * brightness);
    g = (uint8_t)(g * brightness);
    b = (uint8_t)(b * brightness);
    
    _strip.setPixelColor(0, _strip.Color(r, g, b));
    _strip.show();
}
