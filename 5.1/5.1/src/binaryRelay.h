#ifndef BINARYRELAY_H
#define BINARYRELAY_H

#include <Arduino.h>

class Relay {
    public:
    Relay(int pin, uint8_t activeLevel = LOW, unsigned long minSwitchInterval = 300);
    virtual ~Relay() {}

    void begin();
    bool turnOn();                
    bool turnOff();                
    bool toggle();
    bool isOn() const;
    const char *stateName() const;  // for printing

    protected:
    // member that talks to the hardware
    virtual void write(bool active);

    private:
    // check
    bool canSwitch() const;

    int memberPin;
    uint8_t activeLevel; // for different kinds of hardware
    bool state;
    unsigned long minSwitchInterval;  // contact protection, ms
    unsigned long lastSwitchTime;
};
#endif