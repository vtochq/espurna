// -----------------------------------------------------------------------------
// Event Counter Sensor
// Copyright (C) 2017-2018 by Xose Pérez <xose dot perez at gmail dot com>
// -----------------------------------------------------------------------------

#if SENSOR_SUPPORT && HLW8012_SUPPORT

#pragma once

#include "Arduino.h"
#include "BaseSensor.h"

#include <ESP8266WiFi.h>
#include <HLW8012.h>

class HLW8012Sensor : public BaseSensor {

    public:

        // ---------------------------------------------------------------------
        // Public
        // ---------------------------------------------------------------------

        HLW8012Sensor(): BaseSensor() {
            _count = 7;
            _sensor_id = SENSOR_HLW8012_ID;
            _hlw8012 = new HLW8012();
        }

        ~HLW8012Sensor() {
            _enableInterrupts(false);
            delete _hlw8012;
        }

        void expectedCurrent(double expected) {
            _hlw8012->expectedCurrent(expected);
        }

        void expectedVoltage(unsigned int expected) {
            _hlw8012->expectedVoltage(expected);
        }

        void expectedPower(unsigned int expected) {
            _hlw8012->expectedActivePower(expected);
        }

        void resetRatios() {
            _hlw8012->resetMultipliers();
        }

        // ---------------------------------------------------------------------

        void setSEL(unsigned char sel) {
            if (_sel == sel) return;
            _sel = sel;
            _dirty = true;
        }

        void setCF(unsigned char cf) {
            if (_cf == cf) return;
            _cf = cf;
            _dirty = true;
        }

        void setCF1(unsigned char cf1) {
            if (_cf1 == cf1) return;
            _cf1 = cf1;
            _dirty = true;
        }

        void setSELCurrent(bool value) {
            _sel_current = value;
        }

        void setCurrentRatio(double value) {
            _hlw8012->setCurrentMultiplier(value);
        };

        void setVoltageRatio(double value) {
            _hlw8012->setVoltageMultiplier(value);
        };

        void setPowerRatio(double value) {
            _hlw8012->setPowerMultiplier(value);
        };

        // ---------------------------------------------------------------------

        unsigned char getSEL() {
            return _sel;
        }

        unsigned char getCF() {
            return _cf;
        }

        unsigned char getCF1() {
            return _cf1;
        }

        unsigned char getSELCurrent() {
            return _sel_current;
        }

        double getCurrentRatio() {
            return _hlw8012->getCurrentMultiplier();
        };

        double getVoltageRatio() {
            return _hlw8012->getVoltageMultiplier();
        };

        double getPowerRatio() {
            return _hlw8012->getPowerMultiplier();
        };

        // ---------------------------------------------------------------------
        // Sensors API
        // ---------------------------------------------------------------------

        // Initialization method, must be idempotent
        // Defined outside the class body
        void begin() {

            // Initialize HLW8012
            // void begin(unsigned char cf_pin, unsigned char cf1_pin, unsigned char sel_pin, unsigned char currentWhen = HIGH, bool use_interrupts = false, unsigned long pulse_timeout = PULSE_TIMEOUT);
            // * cf_pin, cf1_pin and sel_pin are GPIOs to the HLW8012 IC
            // * currentWhen is the value in sel_pin to select current sampling
            // * set use_interrupts to true to use interrupts to monitor pulse widths
            // * leave pulse_timeout to the default value, recommended when using interrupts
            #if HLW8012_USE_INTERRUPTS
                _hlw8012->begin(_cf, _cf1, _sel, _sel_current, true);
            #else
                _hlw8012->begin(_cf, _cf1, _sel, _sel_current, false, 1000000);
            #endif

            // These values are used to calculate current, voltage and power factors as per datasheet formula
            // These are the nominal values for the Sonoff POW resistors:
            // * The CURRENT_RESISTOR is the 1milliOhm copper-manganese resistor in series with the main line
            // * The VOLTAGE_RESISTOR_UPSTREAM are the 5 470kOhm resistors in the voltage divider that feeds the V2P pin in the HLW8012
            // * The VOLTAGE_RESISTOR_DOWNSTREAM is the 1kOhm resistor in the voltage divider that feeds the V2P pin in the HLW8012
            _hlw8012->setResistors(HLW8012_CURRENT_R, HLW8012_VOLTAGE_R_UP, HLW8012_VOLTAGE_R_DOWN);

            // Handle interrupts
            #if HLW8012_USE_INTERRUPTS
                _enableInterrupts(true);
            #else
                _onconnect_handler = WiFi.onStationModeGotIP([this](WiFiEventStationModeGotIP ipInfo) {
                    _enableInterrupts(true);
                });
                _ondisconnect_handler = WiFi.onStationModeDisconnected([this](WiFiEventStationModeDisconnected ipInfo) {
                    _enableInterrupts(false);
                });
            #endif

        }

        // Descriptive name of the sensor
        String description() {
            char buffer[25];
            snprintf(buffer, sizeof(buffer), "HLW8012 @ GPIO(%i,%i,%i)", _sel, _cf, _cf1);
            return String(buffer);
        }

        // Descriptive name of the slot # index
        String slot(unsigned char index) {
            return description();
        };

        // Address of the sensor (it could be the GPIO or I2C address)
        String address(unsigned char index) {
            char buffer[10];
            snprintf(buffer, sizeof(buffer), "%i:%i:%i", _sel, _cf, _cf1);
            return String(buffer);
        }

        // Type for slot # index
        unsigned char type(unsigned char index) {
            _error = SENSOR_ERROR_OK;
            if (index == 0) return MAGNITUDE_CURRENT;
            if (index == 1) return MAGNITUDE_VOLTAGE;
            if (index == 2) return MAGNITUDE_POWER_ACTIVE;
            if (index == 3) return MAGNITUDE_POWER_REACTIVE;
            if (index == 4) return MAGNITUDE_POWER_APPARENT;
            if (index == 5) return MAGNITUDE_POWER_FACTOR;
            if (index == 6) return MAGNITUDE_ENERGY;
            _error = SENSOR_ERROR_OUT_OF_RANGE;
            return MAGNITUDE_NONE;
        }

        // Current value for slot # index
        double value(unsigned char index) {
            _error = SENSOR_ERROR_OK;
            if (index == 0) return _hlw8012->getCurrent();
            if (index == 1) return _hlw8012->getVoltage();
            if (index == 2) return _hlw8012->getActivePower();
            if (index == 3) return _hlw8012->getReactivePower();
            if (index == 4) return _hlw8012->getApparentPower();
            if (index == 5) return 100 * _hlw8012->getPowerFactor();
            if (index == 6) return _hlw8012->getEnergy();
            _error = SENSOR_ERROR_OUT_OF_RANGE;
            return 0;
        }

        // Toggle between current and voltage monitoring
        #if HLW8012_USE_INTERRUPTS == 0
        // Post-read hook (usually to reset things)
        void post() { _hlw8012->toggleMode(); }
        #endif // HLW8012_USE_INTERRUPTS == 0

        // Handle interrupt calls
        void ICACHE_RAM_ATTR handleInterrupt(unsigned char gpio) {
            if (gpio == _cf) _hlw8012->cf_interrupt();
            if (gpio == _cf1) _hlw8012->cf1_interrupt();
        }

    protected:

        // ---------------------------------------------------------------------
        // Interrupt management
        // ---------------------------------------------------------------------

        void _attach(HLW8012Sensor * instance, unsigned char gpio, unsigned char mode);
        void _detach(unsigned char gpio);

        void _enableInterrupts(bool value) {

            static unsigned char _interrupt_cf = GPIO_NONE;
            static unsigned char _interrupt_cf1 = GPIO_NONE;

            if (value) {

                if (_interrupt_cf != _cf) {
                    if (_interrupt_cf != GPIO_NONE) _detach(_interrupt_cf);
                    _attach(this, _cf, CHANGE);
                    _interrupt_cf = _cf;
                }

                if (_interrupt_cf1 != _cf1) {
                    if (_interrupt_cf1 != GPIO_NONE) _detach(_interrupt_cf1);
                    _attach(this, _cf1, CHANGE);
                    _interrupt_cf1 = _cf1;
                }

            } else {

                _detach(_cf);
                _detach(_cf1);
                _interrupt_cf = GPIO_NONE;
                _interrupt_cf1 = GPIO_NONE;

            }

        }

        // ---------------------------------------------------------------------

        unsigned char _sel;
        unsigned char _cf;
        unsigned char _cf1;
        bool _sel_current;

        HLW8012 * _hlw8012 = NULL;

        #if HLW8012_USE_INTERRUPTS == 0
            WiFiEventHandler _onconnect_handler;
            WiFiEventHandler _ondisconnect_handler;
        #endif

};

// -----------------------------------------------------------------------------
// Interrupt helpers
// -----------------------------------------------------------------------------

HLW8012Sensor * _hlw8012_sensor_instance[10] = {NULL};

void ICACHE_RAM_ATTR _hlw8012_sensor_isr(unsigned char gpio) {
    unsigned char index = gpio > 5 ? gpio-6 : gpio;
    if (_hlw8012_sensor_instance[index]) {
        _hlw8012_sensor_instance[index]->handleInterrupt(gpio);
    }
}

void ICACHE_RAM_ATTR _hlw8012_sensor_isr_0() { _hlw8012_sensor_isr(0); }
void ICACHE_RAM_ATTR _hlw8012_sensor_isr_1() { _hlw8012_sensor_isr(1); }
void ICACHE_RAM_ATTR _hlw8012_sensor_isr_2() { _hlw8012_sensor_isr(2); }
void ICACHE_RAM_ATTR _hlw8012_sensor_isr_3() { _hlw8012_sensor_isr(3); }
void ICACHE_RAM_ATTR _hlw8012_sensor_isr_4() { _hlw8012_sensor_isr(4); }
void ICACHE_RAM_ATTR _hlw8012_sensor_isr_5() { _hlw8012_sensor_isr(5); }
void ICACHE_RAM_ATTR _hlw8012_sensor_isr_12() { _hlw8012_sensor_isr(12); }
void ICACHE_RAM_ATTR _hlw8012_sensor_isr_13() { _hlw8012_sensor_isr(13); }
void ICACHE_RAM_ATTR _hlw8012_sensor_isr_14() { _hlw8012_sensor_isr(14); }
void ICACHE_RAM_ATTR _hlw8012_sensor_isr_15() { _hlw8012_sensor_isr(15); }

static void (*_hlw8012_sensor_isr_list[10])() = {
    _hlw8012_sensor_isr_0, _hlw8012_sensor_isr_1, _hlw8012_sensor_isr_2,
    _hlw8012_sensor_isr_3, _hlw8012_sensor_isr_4, _hlw8012_sensor_isr_5,
    _hlw8012_sensor_isr_12, _hlw8012_sensor_isr_13, _hlw8012_sensor_isr_14,
    _hlw8012_sensor_isr_15
};

void HLW8012Sensor::_attach(HLW8012Sensor * instance, unsigned char gpio, unsigned char mode) {
    if (!gpioValid(gpio)) return;
    _detach(gpio);
    unsigned char index = gpio > 5 ? gpio-6 : gpio;
    _hlw8012_sensor_instance[index] = instance;
    attachInterrupt(gpio, _hlw8012_sensor_isr_list[index], mode);
    #if SENSOR_DEBUG
        DEBUG_MSG_P(PSTR("[SENSOR] GPIO%d interrupt attached to %s\n"), gpio, instance->description().c_str());
    #endif
}

void HLW8012Sensor::_detach(unsigned char gpio) {
    if (!gpioValid(gpio)) return;
    unsigned char index = gpio > 5 ? gpio-6 : gpio;
    if (_hlw8012_sensor_instance[index]) {
        detachInterrupt(gpio);
        #if SENSOR_DEBUG
            DEBUG_MSG_P(PSTR("[SENSOR] GPIO%d interrupt detached from %s\n"), gpio, _hlw8012_sensor_instance[index]->description().c_str());
        #endif
        _hlw8012_sensor_instance[index] = NULL;
    }
}

#endif // SENSOR_SUPPORT && HLW8012_SUPPORT
