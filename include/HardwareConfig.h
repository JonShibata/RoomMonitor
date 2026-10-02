#ifndef HARDWARE_CONFIG_H
#define HARDWARE_CONFIG_H

#include <Arduino.h>

struct PinMap {
    const int doorPin = 5;
    const int motionPin = 4;
    const int beepPin = 0;
    const int ledMotionPin = 2;
    const int htPin = 14;
    const int lightD1Pin = 12;
    const int lightD2Pin = 13;
    const int ledDoorPin = 15;
    const int analogPin = A0;
    const int sdaPin = 12;
    const int sclPin = 13;
};

class HardwareConfig {
public:
    static PinMap pins;
};

#endif
