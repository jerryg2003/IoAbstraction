/*
 * Copyright (c) 2018 https://www.thecoderscorner.com (Nutricherry LTD).
 * This product is licensed under an Apache license, see the LICENSE file in the top-level directory.
 */

/**
 * @file IoAbstractionWire.h
 * 
 * Contains the versions of BasicIoAbstraction that use i2c communication. Including PCF8574 and MCP23017.
 */

#ifndef _IOABSTRACTION_IOABSTRACTIONWIRE_H_
#define _IOABSTRACTION_IOABSTRACTIONWIRE_H_

#include <IoAbstraction.h>
#include <Wire.h>

/**
 * An implementation of BasicIoAbstraction that supports the PCF8574 i2c IO chip. Providing all possible capabilities
 * of the chip in a similar manner to Arduino pins. 
 * @see ioFrom8574 for how to create an instance
 * @see ioDevicePinMode for setting pin modes
 */
class PCF8574IoAbstraction : public BasicIoAbstraction {
private:
	uint8_t address;
	uint8_t lastRead;
	uint8_t toWrite;
	bool needsWrite;
	uint8_t interruptPin;
public:
	/** Construct a 8574 expander on i2c address and with interrupts connected to a given pin (0xff no interrupts) */
	PCF8574IoAbstraction(uint8_t addr, uint8_t interruptPin);
	virtual ~PCF8574IoAbstraction() { }

	/** 
	 * sets the pin direction on the device, notice that on this device input is achieved by setting the port to high 
	 * so it is always set as INPUT_PULLUP, even if INPUT is chosen 
	 */
	virtual void pinDirection(uint8_t pin, uint8_t mode);

	/** 
	 * writes a new value to the device after a sync. 
	 */
	virtual void writeValue(uint8_t pin, uint8_t value);

	/**
	 * reads a value from the last cached state  - updated each sync
	 */
	virtual uint8_t readValue(uint8_t pin);

	/**
	 * Writes a complete 8 bit port value, that is updated to the device each sync
	 */
	virtual void writePort(uint8_t pin, uint8_t port);

	/**
	 * Reads the complete 8 bit byte from the last cached state, that is updated each sync.
	 */ 
	virtual uint8_t readPort(uint8_t pin);

	/** 
	 * attaches an interrupt handler for this device. Notice for this device, all pin changes will be notified
	 * on any pin of the port, it is not configurable at the device level, the type of interrupt will also
	 * always be CHANGE.
	 */
	virtual void attachInterrupt(uint8_t pin, RawIntHandler intHandler, uint8_t mode);

	/** 
	 * updates settings on the board after changes 
	 */
	virtual void runLoop();
private:
	void writeData();
	uint8_t readData();
};

//
// MCP23017 support.
//

// definitions of register addresses.

#define IODIR_ADDR       0x00
#define IPOL_ADDR        0x02
#define GPINTENA_ADDR    0x04
#define DEFVAL_ADDR      0x06
#define INTCON_ADDR      0x08
#define IOCON_ADDR       0x0a
#define GPPU_ADDR        0x0c
#define INTF_ADDR        0x0e
#define INTCAP_ADDR      0x10
#define GPIO_ADDR        0x12

// definitions for the IO control register

#define IOCON_HAEN_BIT  3
#define IOCON_SEQOP_BIT  5
#define IOCON_MIRROR_BIT  6
#define IOCON_BANK_BIT  7

enum Mcp23xInterruptMode {
	NOT_ENABLED = 0, ACTIVE_HIGH_OPEN = 0b110, ACTIVE_LOW_OPEN = 0b100, ACTIVE_HIGH = 0b010, ACTIVE_LOW = 0b000 
};

class MCP23017IoAbstraction : public BasicIoAbstraction {
private:
	uint8_t address;
	uint8_t intPinA;
	uint8_t intPinB;
	uint8_t intMode;
	uint16_t portCache;
	bool needsWrite, needsInit;
public:
	MCP23017IoAbstraction(uint8_t address, Mcp23xInterruptMode intMode,  uint8_t intPinA, uint8_t intPinB);
	virtual ~MCP23017IoAbstraction() {;}

	virtual void pinDirection(uint8_t pin, uint8_t mode);
	virtual void writeValue(uint8_t pin, uint8_t value);
	virtual uint8_t readValue(uint8_t pin);

	/**
	 * Attaches an interrupt to the device and links it to the arduino pin. On the MCP23017 nearly all interrupt modes
	 * are supported, including CHANGE, RISING, FALLING and are selective both per port and by pin.
	 */
	virtual void attachInterrupt(uint8_t pin, RawIntHandler intHandler, uint8_t mode);
	
	/** 
	 * updates settings on the board after changes 
	 */
	virtual void runLoop();
	
	/**
	 * Writes a complete 8 bit port value, that is updated to the device each sync
	 * Any pin between 0-7 refers to portA, otherwise portB.
	 */
	virtual void writePort(uint8_t pin, uint8_t port);

	/**
	 * Reads the complete 8 bit byte from the last cached state, that is updated each sync.
	 * Any pin between 0-7 refers to portA, otherwise portB.
	 */ 
	virtual uint8_t readPort(uint8_t pin);
private:
	void toggleBitInRegister(uint8_t regAddr, uint8_t theBit, bool value);
	void initDevice();
	void writeToDevice(uint8_t reg, uint16_t command);
	uint16_t readFromDevice(uint8_t reg);
};

// to remain compatible with old code
#define ioFrom8754 ioFrom8574

/**
 * Creates an instance of an IoAbstraction that works with a PCF8574 chip over i2c, which optionally
 * has support for interrupts should it be needed. Note that only interrupt mode CHANGE is support, 
 * and a change on any pin raises an interrupt. All inputs are by default INPUT_PULLUP by device design.
 * @param addr the i2c address of the device
 * @param interruptPin (optional) the pin on the Arduino side that is used for interrupt handling if needed.
 */
IoAbstractionRef ioFrom8574(uint8_t addr, uint8_t interruptPin = 0xff);

/**
 * Perform digital read and write functions using 23017 expanders. These expanders are the closest include
 * terms of functionality to regular Arduino pins, supporting most interrupt modes and very similar GPIO
 * capabilities. If interrupts are needed, this uses one Arduino pin for BOTH ports on the device.
 * @param addr the i2c address of the device
 * @param intMode the interrupt mode the device will operate in
 * @param interruptPin the pin on the Arduino that will be used to detect the interrupt condition.
 */
IoAbstractionRef iofrom23017(uint8_t addr, Mcp23xInterruptMode intMode, uint8_t interruptPin = 0xff);

/**
 * Perform digital read and write functions using 23017 expanders. These expanders are the closest include
 * terms of functionality to regular Arduino pins, supporting most interrupt modes and very similar GPIO
 * capabilities. If interrupts are needed, this uses one Arduino pin for EACH port on the device.
 * @param addr the i2c address of the device
 * @param intMode the interrupt mode the device will operate in
 * @param interruptPinA the pin on the Arduino that will be used to detect the PORTA interrupt condition.
 * @param interruptPinB the pin on the Arduino that will be used to detect the PORTB interrupt condition.
 */
IoAbstractionRef iofrom23017IntPerPort(uint8_t addr, Mcp23xInterruptMode intMode, uint8_t interruptPinA, uint8_t interruptPinB);

#endif /* _IOABSTRACTION_IOABSTRACTIONWIRE_H_ */
