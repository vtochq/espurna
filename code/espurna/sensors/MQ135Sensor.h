// -----------------------------------------------------------------------------
// MQ-135 TVOC Air Quality MQ135 Sensor
// Based on Analog Sensor and MQ-135 Arduino lib (https://github.com/GeorgK/MQ135)
// Copyright (C) 2017-2019 by Xose Pérez <xose dot perez at gmail dot com>
// Copyright (C) 2020 by Alexander Kolesnikov <vtochq at gmail dot com>
// Copyright (C) 2014 G.Krocker (Mad Frog Labs)
// -----------------------------------------------------------------------------

#if SENSOR_SUPPORT && MQ135_SUPPORT

#pragma once

#include <Arduino.h>

#include "../debug.h"

#include "BaseSensor.h"
#include "BaseAnalogSensor.h"

#define MQ135_NAME  "MQ-135"

/// The load resistance on the board
#define RLOAD 10.0
/// Calibration resistance at atmospheric CO2 level
#define RZERO 76.63
/// Parameters for calculating ppm of CO2 from sensor resistance
#define PARA 116.6020682
#define PARB 2.769034857

/// Parameters to model temperature and humidity dependence
#define CORA 0.00035
#define CORB 0.02718
#define CORC 1.39538
#define CORD 0.0018

/// Atmospheric CO2 level for calibration purposes
#define ATMOCO2 397.13

class MQ135 {
 private:
  uint8_t _pin;

 public:
  MQ135(uint8_t pin);
  float getCorrectionFactor(float t, float h);
  float getResistance();
  float getCorrectedResistance(float t, float h);
  float getPPM();
  float getCorrectedPPM(float t, float h);
  float getRZero();
  float getCorrectedRZero(float t, float h);
};



class MQ135Sensor : public BaseAnalogSensor {

    public:

        // ---------------------------------------------------------------------
        // Public
        // ---------------------------------------------------------------------

        MQ135Sensor() {
            _count = 1;
            _sensor_id = SENSOR_ANALOG_ID;
        }

        void setSamples(unsigned int samples) {
            if (_samples > 0) _samples = samples;
        }

        void setDelay(unsigned long micros) {
            _micros = micros;
        }

        void setFactor(double factor) {
            //DEBUG_MSG(("[ANALOG_SENSOR] Factor set to: %s \n"), String(factor,6).c_str());
            _factor = factor;
        }

        void setOffset(double offset) {
          //DEBUG_MSG(("[ANALOG_SENSOR] Factor set to: %s \n"), String(offset,6).c_str());
            _offset = offset;
        }

        // ---------------------------------------------------------------------

        unsigned int getSamples() {
            return _samples;
        }

        unsigned long getDelay() {
            return _micros;
        }

        double getFactor() {
            return _factor;
        }

        double getOffset() {
            return _offset;
        }

        // ---------------------------------------------------------------------
        // Sensor API
        // ---------------------------------------------------------------------

        // Initialization method, must be idempotent
        void begin() {            
            _ready = true;
        }

        // Descriptive name of the sensor
        String description() {
            return String("MQ135_NAME");
        }

        // Descriptive name of the slot # index
        String slot(unsigned char index) {
            return description();
        };

        // Address of the sensor (it could be the GPIO or I2C address)
        String address(unsigned char index) {
            return String("0");
        }

        // Type for slot # index
        unsigned char type(unsigned char index) {
            if (index == 0) return MAGNITUDE_ANALOG;
            return MAGNITUDE_NONE;
        }

        // Current value for slot # index
        // Changed return type as moving to scaled value
        double value(unsigned char index) {
            if (index == 0) return _read();
            return 0;
        }

    protected:

        //CICM: this should be for raw values
        // renaming protected function "_read" to "_rawRead"
        unsigned int _rawRead() {
            if (1 == _samples) return analogRead(0);
            unsigned long sum = 0;
            for (unsigned int i=0; i<_samples; i++) {
                if (i>0) delayMicroseconds(_micros);
                sum += analogRead(0);
            }
            return sum / _samples;
        }

        //CICM: and proper read should be scalable and thus needs sign
        //and decimal part
        double _read() {
          //Raw measure could also be a class variable with getter so that can
          //be reported through MQTT, ...
          unsigned int rawValue;
          double scaledValue;
          // Debugging doubles to string
          //DEBUG_MSG(("[ANALOG_SENSOR] Started standard read, factor: %s , offset: %s, decimals: %d \n"), String(_factor).c_str(), String(_offset).c_str(), ANALOG_DECIMALS);
          rawValue = _rawRead();
          //DEBUG_MSG(("[ANALOG_SENSOR] Raw read received: %d \n"), rawValue);
          scaledValue = _factor*rawValue  + _offset;
          //DEBUG_MSG(("[ANALOG_SENSOR] Scaled value result: %s \n"), String(scaledValue).c_str());
          return scaledValue;
        }


        unsigned int _samples = 1;
        unsigned long _micros = 0;
        //CICM: for scaling and offset, also with getters and setters
        double _factor = 1.0;
        double _offset = 0.0;

};

#endif // SENSOR_SUPPORT && MQ135_SUPPORT
