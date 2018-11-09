/*
 * Copyright (c) 2018 https://www.thecoderscorner.com (Nutricherry LTD).
 * This product is licensed under an Apache license, see the LICENSE file in the top-level directory.
 */

#ifndef _MOCK_IO_ABSTRACTION_H_
#define _MOCK_IO_ABSTRACTION_H_

#include <IoAbstraction.h>
#include <AUnit.h>
using namespace aunit;

/**
 * During any call to the mock version of IoAbstraction, any error detected
 * will be recorded in the error variable. However, only the last one is kept.
 */ 
enum MockIoError {
    NO_ERROR,
    PIN_TOO_HIGH,
    READ_NOT_INPUT,
    WRITE_NOT_OUTPUT
};

/**
 * This class implements the IoAbstraction interface but does not do anything
 * other than record the pinMode and write calls, it also allows the read
 * values to be set upfront. There are up to 16 pins in this abstraction and
 * each run loop cycles to another buffer. In the constructor you can set the 
 * number of storage slots (the point they will cycle back to slot 0).
 * 
 * It is very useful when trying to work with IoAbstraction in unit tests.
 */
class MockedIoAbstraction : public BasicIoAbstraction {
private:
    uint8_t pinModes[16];
    uint16_t *readValues;
    uint16_t *writeValues;
    int runLoopCalls;
    int numberOfCycles;
    
    /** the last error recorded */
    MockIoError error;
    
    /** the interrupt handler that was recorded. */
    RawIntHandler intHandler;
    /** pin the interrupt is registered too */
    uint8_t intPin;
    /** the mode for the interrupt */
    uint8_t intMode;

public:
    MockedIoAbstraction(int numberOfCycles = 6) {
        for(int i=0;i<16;i++) pinModes[i] = 0xff;
        error = NO_ERROR;

        this->numberOfCycles = numberOfCycles;
        readValues = new uint16_t[numberOfCycles];
        writeValues = new uint16_t[numberOfCycles];

        resetIo();
    }

 	virtual ~MockedIoAbstraction() { 
         delete readValues;
         delete writeValues;
    }

    void resetIo() {
        for(int i=0; i<numberOfCycles; i++) {
            readValues[i] = 0;
            writeValues[i] = 0;
        }
        runLoopCalls = 0;
    }


	virtual void pinDirection(uint8_t pin, uint8_t mode) {
        checkPinInRange(pin);
        pinModes[pin] = mode;
    }

   	virtual void writeValue(uint8_t pin, uint8_t value) {
        checkPinInRange(pin);
        if(pinModes[pin] != OUTPUT) error = WRITE_NOT_OUTPUT;
        bitWrite(writeValues[runLoopCalls], pin, value != 0);
    }

   	virtual uint8_t readValue(uint8_t pin) {
        checkPinInRange(pin);
        if(pinModes[pin] != INPUT && pinModes[pin] != INPUT_PULLUP) error = READ_NOT_INPUT;
        return bitRead(readValues[runLoopCalls], pin);
    }

  	void attachInterrupt(uint8_t pin, RawIntHandler interruptHandler, uint8_t mode) override {
        this->intHandler = interruptHandler;
        this->intPin = pin;
        this->intMode = mode;
    }

	void runLoop() override { 
        // copy over the last written values (as they are generally additive) and bump counter.
        uint16_t currentWritten = writeValues[runLoopCalls];
        runLoopCalls++;
        runLoopCalls = runLoopCalls % numberOfCycles;
        writeValues[runLoopCalls] = currentWritten;
    }

   	void writePort(uint8_t pin, uint8_t portVal) override {
        checkPinInRange(pin);

        if(pin < 8) {
            checkPinsAre(OUTPUT, 0, 7);
            writeValues[runLoopCalls] = (writeValues[runLoopCalls] & 0xff00) | portVal;
        }
        else {
            checkPinsAre(INPUT, 8, 15);
            writeValues[runLoopCalls] = (writeValues[runLoopCalls] & 0x00ff) | (portVal << 8);
        }
    }

	virtual uint8_t readPort(uint8_t pin) {
        checkPinInRange(pin);

        if(pin < 8) {
            checkPinsAre(INPUT, 0, 7);
            return readValues[runLoopCalls];
        }
        else {
            checkPinsAre(INPUT, 8, 15);
            return readValues[runLoopCalls] >> 8;
        }
    }

    /** get the number of run loops that have been performed */
    int getNumberOfRunLoops() {return runLoopCalls;}

    /** get the data that's been written in a given run loop */
    uint16_t getWrittenValuesForRunLoop(int runLoop) { return writeValues[runLoop]; }

    /** set the value that will be used to return during read functions */
    void setValueForReading(int runLoopNo, uint16_t val) {readValues[runLoopNo] = val;}

    /** get the value that was written using the write functions */
    uint16_t getWrittenValue(int runLoopNo) {return writeValues[runLoopNo];}

    /** get any error in usage of the class */
    MockIoError getErrorMode() {return error;}

    /** get the interrupt function registered using the attachInterrupt call */
    RawIntHandler getInterruptFunction() {return intHandler;}

    /** check if the registered interrupt pin and mode are right */
    bool isIntRegisteredAs(uint8_t pin, uint8_t mode) {
        return intPin == pin && intMode == mode && intHandler != NULL;
    }
private:
    void checkPinInRange(int pin) {
        if(pin > 15) error = PIN_TOO_HIGH;
    }
    void checkPinsAre(uint8_t mode, uint8_t start, uint8_t end) {
        for(int i = start; i < end; ++i) {
            if(mode == OUTPUT) {
                if(pinModes[i] != OUTPUT) error = WRITE_NOT_OUTPUT;
            }
            else if(mode == INPUT) {
                if(pinModes[i] != INPUT && pinModes[i] != INPUT_PULLUP) error = READ_NOT_INPUT;
            }
        }
    }
};

/**
 * This wraps any other IOAbstraction by delegation and logs every sync to the serial port.
 * 
 * It takes a number of ports to read and assumes that the read back state will include any
 * writes that have been made. It is very useful for debugging.
 * 
 * NEVER use this class in production, it calls Serial.print every write to the device.
 * 
 * Example usage: IoAbstractionRef ioDevice = new LoggingIoAbstraction(ioFrom8574(0x20), 1);
 */
class LoggingIoAbstraction : public BasicIoAbstraction {
private:
    IoAbstractionRef delegate;
    uint32_t writeVals;
    int ports;
public:    
    LoggingIoAbstraction(IoAbstractionRef delegate, int ports) { 
        this->delegate = delegate; 
        this->ports = ports;
        writeVals = 0;
    }

    void pinDirection(uint8_t pin, uint8_t mode) override { delegate->pinDirection(pin, mode); }
    void writeValue(uint8_t pin, uint8_t value) override { 
        bitWrite(writeVals, pin, value);
        delegate->writeValue(pin, value); 
    }
    uint8_t readValue(uint8_t pin) override { return delegate->readValue(pin); }
    void attachInterrupt(uint8_t pin, RawIntHandler interruptHandler, uint8_t mode) override { delegate->attachInterrupt(pin, interruptHandler, mode); }
    void writePort(uint8_t pin, uint8_t portVal) override { 
        if(pin < 8) {
            writeVals &= 0xffffff00L;
            writeVals |= (uint32_t)portVal;
        }
        else if(pin < 16) {
            writeVals &= 0xffff00ffL;
            writeVals |= ((uint32_t)portVal<<8);
        }
        else if(pin < 24) {
            writeVals &= 0xff00ffffL;
            writeVals |= ((uint32_t)portVal<<16);
        }
        else {
            writeVals &= 0x00ffffffL;
            writeVals |= ((uint32_t)portVal<<24);
        }
        delegate->writePort(pin, portVal);
    }
    uint8_t readPort(uint8_t pin) override { return delegate->readPort(pin);}

    void runLoop() override { 
        Serial.print("Port write ");
        uint32_t val = writeVals;
        for(int i=0;i<ports;i++) {
            printHexZeroPad(val);
            val = val >> 8;
        }
        delegate->runLoop();
        Serial.print("read ");
        for(int i=0;i<ports;i++) {
            printHexZeroPad(delegate->readPort(i * 8));
        }
        Serial.println();
    }

    void printHexZeroPad(uint8_t val) {
        Serial.write(hexchar(val / 16));
        Serial.write(hexchar(val % 16));
        Serial.write(' ');
    }

    char hexchar(uint8_t ch) {
        return ch < 10 ? ch + '0' : (ch-10) + 'A';
    }
};

#endif // _MOCK_IO_ABSTRACTION_H_
