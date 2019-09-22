/****************************************************************************
 *
 *   Copyright (c) 2012-2015 PX4 Development Team. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name PX4 nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

/**
 * @file batt_smbus.h
 *
 * Header for a battery monitor connected via SMBus (I2C).
 * Designed for BQ40Z50-R1/R2
 *
<<<<<<< HEAD
 * @author Jacob Dahl <dahl.jakejacob@gmail.com>
 * @author Alex Klimaj <alexklimaj@gmail.com>
 */

#include "batt_smbus.h"

extern "C" __EXPORT int batt_smbus_main(int argc, char *argv[]);

struct work_s BATT_SMBUS::_work = {};

BATT_SMBUS::BATT_SMBUS(SMBus *interface, const char *path) :
	_interface(interface),
	_cycle(perf_alloc(PC_ELAPSED, "batt_smbus_cycle")),
	_batt_topic(nullptr),
	_cell_count(4),
=======
 * @author Randy Mackay <rmackay9@yahoo.com>
 * @author Alex Klimaj <alexklimaj@gmail.com>
 */

#include <float.h>
#include <stdio.h>
#include <string.h>
#include <ecl/geo/geo.h>

#include <drivers/device/i2c.h>
#include <drivers/device/ringbuffer.h>
#include <drivers/drv_hrt.h>
#include <px4_config.h>
#include <px4_workqueue.h>
#include <perf/perf_counter.h>
#include <uORB/topics/battery_status.h>
#include <uORB/uORB.h>

#define BATT_SMBUS_ADDR_MIN             0x00	///< lowest possible address
#define BATT_SMBUS_ADDR_MAX             0xFF	///< highest possible address

#define BATT_SMBUS_I2C_BUS              PX4_I2C_BUS_EXPANSION
#define BATT_SMBUS_ADDR                 0x0B	///< Default 7 bit address I2C address. 8 bit = 0x16
#define BATT_SMBUS_TEMP                 0x08	///< temperature register
#define BATT_SMBUS_VOLTAGE              0x09	///< voltage register
#define BATT_SMBUS_REMAINING_CAPACITY	0x0F	///< predicted remaining battery capacity as a percentage
#define BATT_SMBUS_FULL_CHARGE_CAPACITY 0x10    ///< capacity when fully charged
#define BATT_SMBUS_DESIGN_CAPACITY		0x18	///< design capacity register
#define BATT_SMBUS_DESIGN_VOLTAGE		0x19	///< design voltage register
#define BATT_SMBUS_MANUFACTURE_DATE   	0x1B  	///< manufacture date register
#define BATT_SMBUS_SERIAL_NUMBER      	0x1C  	///< serial number register
#define BATT_SMBUS_MANUFACTURER_NAME	0x20	///< manufacturer name
#define BATT_SMBUS_CURRENT              0x0A	///< current register
#define BATT_SMBUS_AVERAGE_CURRENT      0x0B	///< current register
#define BATT_SMBUS_MEASUREMENT_INTERVAL_US	(1000000 / 10)	///< time in microseconds, measure at 10Hz
#define BATT_SMBUS_TIMEOUT_US			10000000	///< timeout looking for battery 10seconds after startup
#define BATT_SMBUS_CYCLE_COUNT			0x17	///< number of cycles the battery has experienced
#define BATT_SMBUS_RUN_TIME_TO_EMPTY	0x11	///< predicted remaining battery capacity based on the present rate of discharge in min
#define BATT_SMBUS_AVERAGE_TIME_TO_EMPTY	0x12	///< predicted remaining battery capacity based on the present rate of discharge in min

#define BATT_SMBUS_MANUFACTURER_ACCESS	0x00
#define BATT_SMBUS_MANUFACTURER_BLOCK_ACCESS    0x44

#define BATT_SMBUS_PEC_POLYNOMIAL	0x07		///< Polynomial for calculating PEC

#ifndef CONFIG_SCHED_WORKQUEUE
# error This requires CONFIG_SCHED_WORKQUEUE.
#endif


class BATT_SMBUS : public device::I2C
{
public:
	BATT_SMBUS(int bus = PX4_I2C_BUS_EXPANSION, uint16_t batt_smbus_addr = BATT_SMBUS_ADDR);
	virtual ~BATT_SMBUS();

	/**
	 * Initialize device
	 *
	 * Calls probe() to check for device on bus.
	 *
	 * @return 0 on success, error code on failure
	 */
	virtual int		init();

	/**
	 * Test device
	 *
	 * @return 0 on success, error code on failure
	 */
	virtual int		test();

	/**
	 * Search all possible slave addresses for a smart battery
	 */
	int			search();

	/**
	 * Get the SBS manufacturer name of the battery device
	 *
	 * @param manufacturer_name pointer a buffer into which the manufacturer name is to be written
	* @param max_length the maximum number of bytes to attempt to read from the manufacturer name register, including the null character that is appended to the end
	 *
	 * @return the number of bytes read
	 */
	uint8_t     manufacturer_name(uint8_t *man_name, uint8_t max_length);

	/**
	 * Return the SBS manufacture date of the battery device
	 *
	 * @return the date in the following format:
	*  see Smart Battery Data Specification, Revision  1.1
	*  http://sbs-forum.org/specs/sbdat110.pdf for more details
	 *  Date as uint16_t = (year-1980) * 512 + month * 32 + day
	 *  | Field | Bits | Format             | Allowable Values                           |
	 *  | ----- | ---- | ------------------ | ------------------------------------------ |
	 *  | Day     0-4    5-bit binary value   1-31 (corresponds to day)                  |
	 *  | Month   5-8    4-bit binary value   1-12 (corresponds to month number)         |
	 *  | Year    9-15   7-bit binary value   0-127 (corresponds to year biased by 1980) |
	 *  otherwise, return 0 on failure
	 */
	uint16_t  manufacture_date();

	/**
	 * Return the SBS serial number of the battery device
	 */
	uint16_t     serial_number();

protected:
	/**
	 * Check if the device can be contacted
	 */
	virtual int		probe();

private:

	/**
	 * Start periodic reads from the battery
	 */
	void			start();

	/**
	 * Stop periodic reads from the battery
	 */
	void			stop();

	/**
	 * static function that is called by worker queue
	 */
	static void		cycle_trampoline(void *arg);

	/**
	 * perform a read from the battery
	 */
	void			cycle();

	/**
	 * Read a word from specified register
	 */
	int			read_reg(uint8_t reg, uint16_t &val);

	/**
	 * Write a word to specified register
	 */
	int			write_reg(uint8_t reg, uint16_t val);

	/**
	 * Convert from 2's compliment to decimal
	 * @return the absolute value of the input in decimal
	 */
	uint16_t	convert_twos_comp(uint16_t val);

	/**
	 * Read block from bus
	 * @return returns number of characters read if successful, zero if unsuccessful
	 */
	uint8_t			read_block(uint8_t reg, uint8_t *data, uint8_t max_len, bool append_zero);

	/**
	 * Write block to the bus
	 * @return the number of characters sent if successful, zero if unsuccessful
	 */
	uint8_t			write_block(uint8_t reg, uint8_t *data, uint8_t len);

	/**
	 * Calculate PEC for a read or write from the battery
	 * @param buff is the data that was read or will be written
	 */
	uint8_t	get_PEC(uint8_t cmd, bool reading, const uint8_t buff[], uint8_t len);

	/**
	 * Write a word to Manufacturer Access register (0x00)
	 * @param cmd the word to be written to Manufacturer Access
	 */
	uint8_t	ManufacturerAccess(uint16_t cmd);

	// internal variables
	bool			_enabled;	///< true if we have successfully connected to battery
	work_s			_work{};		///< work queue for scheduling reads

	battery_status_s _last_report;	///< last published report, used for test()

	orb_advert_t	_batt_topic;	///< uORB battery topic
	orb_id_t		_batt_orb_id;	///< uORB battery topic ID

	uint64_t		_start_time;	///< system time we first attempt to communicate with battery
	uint16_t		_batt_capacity;	///< battery's design capacity in mAh (0 means unknown)
	uint16_t		_batt_startup_capacity;	///< battery's remaining capacity on startup
	char           *_manufacturer_name;  ///< The name of the battery manufacturer
	uint16_t		_cycle_count;	///< number of cycles the battery has experienced
	uint16_t		_serial_number;		///< serial number register
	float 			_crit_thr;	///< Critical battery threshold param
	float 			_low_thr;	///< Low battery threshold param
	float 			_emergency_thr;		///< Emergency battery threshold param
};

namespace
{
BATT_SMBUS *g_batt_smbus;	///< device handle. For now, we only support one BATT_SMBUS device
}

void batt_smbus_usage();

extern "C" __EXPORT int batt_smbus_main(int argc, char *argv[]);

int manufacturer_name();
int manufacture_date();
int serial_number();

BATT_SMBUS::BATT_SMBUS(int bus, uint16_t batt_smbus_addr) :
	I2C("batt_smbus", "/dev/batt_smbus0", bus, batt_smbus_addr, 100000),
	_enabled(false),
	_last_report{},
	_batt_topic(nullptr),
	_batt_orb_id(nullptr),
	_start_time(0),
>>>>>>> 97f14edcbd3ff8526326d26d749656a8e8f309c9
	_batt_capacity(0),
	_batt_startup_capacity(0),
	_manufacturer_name(nullptr),
	_cycle_count(0),
	_serial_number(0),
	_crit_thr(0.0f),
	_low_thr(0.0f),
<<<<<<< HEAD
	_manufacturer_name(nullptr),
	_lifetime_max_delta_cell_voltage(0.0f),
	_cell_undervoltage_protection_status(1)
{
	battery_status_s new_report = {};
	_batt_topic = orb_advertise(ORB_ID(battery_status), &new_report);

	int battsource = 1;
	param_set(param_find("BAT_SOURCE"), &battsource);

	_interface->init();
	// unseal() here to allow an external config script to write to protected flash.
	// This is neccessary to avoid bus errors due to using standard i2c mode instead of SMbus mode.
	// The external config script should then seal() the device.
	unseal();
=======
	_emergency_thr(0.0f)
{
	// capture startup time
	_start_time = hrt_absolute_time();
>>>>>>> 97f14edcbd3ff8526326d26d749656a8e8f309c9
}

BATT_SMBUS::~BATT_SMBUS()
{
<<<<<<< HEAD
	orb_unadvertise(_batt_topic);
	perf_free(_cycle);
=======
	// make sure we are truly inactive
	stop();
>>>>>>> 97f14edcbd3ff8526326d26d749656a8e8f309c9

	if (_manufacturer_name != nullptr) {
		delete[] _manufacturer_name;
	}
}

int
BATT_SMBUS::init()
{
	int ret = ENOTTY;

	// attempt to initialise I2C bus
	ret = I2C::init();

	if (ret != OK) {
		PX4_ERR("failed to init I2C");
		return ret;

	} else {
		//Find the battery on the bus
		search();

		// start work queue
		start();
	}

<<<<<<< HEAD
	if (_interface != nullptr) {
		delete _interface;
	}

	int battsource = 0;
	param_set(param_find("BAT_SOURCE"), &battsource);

	PX4_WARN("Exiting.");
}

int BATT_SMBUS::task_spawn(int argc, char *argv[])
{
	enum BATT_SMBUS_BUS busid = BATT_SMBUS_BUS_ALL;
	int ch;

	while ((ch = getopt(argc, argv, "XTRIA:")) != EOF) {
		switch (ch) {
		case 'X':
			busid = BATT_SMBUS_BUS_I2C_EXTERNAL;
			break;

		case 'T':
			busid = BATT_SMBUS_BUS_I2C_EXTERNAL1;
			break;

		case 'R':
			busid = BATT_SMBUS_BUS_I2C_EXTERNAL2;
			break;

		case 'I':
			busid = BATT_SMBUS_BUS_I2C_INTERNAL;
			break;

		case 'A':
			busid = BATT_SMBUS_BUS_ALL;
			break;

		default:
			print_usage();
			return PX4_ERROR;
		}
	}

	for (unsigned i = 0; i < NUM_BUS_OPTIONS; i++) {

		if (_object == nullptr && (busid == BATT_SMBUS_BUS_ALL || bus_options[i].busid == busid)) {

			SMBus *interface = new SMBus(bus_options[i].busnum, BATT_SMBUS_ADDR);
			BATT_SMBUS *dev = new BATT_SMBUS(interface, bus_options[i].devpath);

			// Successful read of device type, we've found our battery
			_object = dev;
			_task_id = task_id_is_work_queue;

			int result = dev->get_startup_info();

			if (result != PX4_OK) {
				return PX4_ERROR;
			}

			// Throw it into the work queue.
			work_queue(HPWORK, &_work, (worker_t)&BATT_SMBUS::cycle_trampoline, dev, 0);

			return PX4_OK;

		}
	}

	PX4_WARN("Not found.");
	return PX4_ERROR;
}

void BATT_SMBUS::cycle_trampoline(void *arg)
{
	BATT_SMBUS *dev = (BATT_SMBUS *)arg;
	dev->cycle();
}

void BATT_SMBUS::cycle()
=======
	// init orb id
	_batt_orb_id = ORB_ID(battery_status);

	return ret;
}

int
BATT_SMBUS::test()
{
	int sub = orb_subscribe(ORB_ID(battery_status));
	bool updated = false;
	struct battery_status_s status;
	uint64_t start_time = hrt_absolute_time();

	// loop for 3 seconds
	while ((hrt_absolute_time() - start_time) < 3000000) {

		// display new info that has arrived from the orb
		orb_check(sub, &updated);

		if (updated) {
			if (orb_copy(ORB_ID(battery_status), sub, &status) == OK) {
				print_message(status);
			}
		}

		// sleep for 0.2 seconds
		usleep(200000);
	}

	return OK;
}

int
BATT_SMBUS::search()
{
	bool found_slave = false;
	uint16_t tmp;
	int16_t orig_addr = get_device_address();

	// search through all valid SMBus addresses
	for (uint8_t i = BATT_SMBUS_ADDR_MIN; i < BATT_SMBUS_ADDR_MAX; i++) {
		set_device_address(i);

		if (read_reg(BATT_SMBUS_VOLTAGE, tmp) == OK) {
			if (tmp > 0) {
				PX4_INFO("battery found at 0x%x", get_device_address());
				found_slave = true;
				break;
			}
		}

		// short sleep
		usleep(1);
	}

	if (found_slave == false) {
		// restore original i2c address
		set_device_address(orig_addr);
	}

	// display completion message
	if (found_slave) {
		PX4_INFO("smart battery connected");

	} else {
		PX4_INFO("No smart batteries found.");
	}

	return OK;
}

uint8_t
BATT_SMBUS::manufacturer_name(uint8_t *man_name, uint8_t max_length)
{
	uint8_t len = read_block(BATT_SMBUS_MANUFACTURER_NAME, man_name, max_length, false);

	if (len > 0) {
		if (len >= max_length - 1) {
			man_name[max_length - 1] = 0;

		} else {
			man_name[len] = 0;
		}
	}

	return len;
}

uint16_t
BATT_SMBUS::manufacture_date()
{
	uint16_t man_date;

	if (read_reg(BATT_SMBUS_MANUFACTURE_DATE, man_date) == OK) {
		return man_date;
	}

	// Return 0 if could not read the date correctly
	return 0;
}

uint16_t
BATT_SMBUS::serial_number()
{
	uint16_t serial_num;

	if (read_reg(BATT_SMBUS_SERIAL_NUMBER, serial_num) == OK) {
		return serial_num;
	}

	return -1;
}

int
BATT_SMBUS::probe()
{
	// always return OK to ensure device starts
	return OK;
}

void
BATT_SMBUS::start()
{
	// schedule a cycle to start things
	work_queue(HPWORK, &_work, (worker_t)&BATT_SMBUS::cycle_trampoline, this, 1);
}

void
BATT_SMBUS::stop()
{
	work_cancel(HPWORK, &_work);
}

void
BATT_SMBUS::cycle_trampoline(void *arg)
{
	BATT_SMBUS *dev = (BATT_SMBUS *)arg;

	dev->cycle();
}

void
BATT_SMBUS::cycle()
>>>>>>> 97f14edcbd3ff8526326d26d749656a8e8f309c9
{
	// get current time
	uint64_t now = hrt_absolute_time();

	// exit without rescheduling if we have failed to find a battery after 10 seconds
	if (!_enabled && (now - _start_time > BATT_SMBUS_TIMEOUT_US)) {
		PX4_INFO("did not find smart battery");
		return;
	}

	// Try and get battery SBS info
	if (_manufacturer_name == nullptr) {
		char man_name[21];
		uint8_t len = manufacturer_name((uint8_t *)man_name, sizeof(man_name));

		if (len > 0) {
			_manufacturer_name = new char[len + 1];
			strcpy(_manufacturer_name, man_name);
		}
	}

<<<<<<< HEAD
	new_report.connected = true;

	// Temporary variable for storing SMBUS reads.
	uint16_t result;

	int ret = _interface->read_word(BATT_SMBUS_VOLTAGE, &result);
=======
	// read battery serial number on startup
	if (_serial_number == 0) {
		_serial_number = serial_number();
	}

	// temporary variable for storing SMBUS reads
	uint16_t tmp;

	// read battery capacity on startup
	if (_batt_startup_capacity == 0) {
		if (read_reg(BATT_SMBUS_REMAINING_CAPACITY, tmp) == OK) {
			_batt_startup_capacity = tmp;
		}
	}

	// read battery cycle count on startup
	if (_cycle_count == 0) {
		if (read_reg(BATT_SMBUS_CYCLE_COUNT, tmp) == OK) {
			_cycle_count = tmp;
		}
	}

	// read battery design capacity on startup
	if (_batt_capacity == 0) {
		if (read_reg(BATT_SMBUS_FULL_CHARGE_CAPACITY, tmp) == OK) {
			_batt_capacity = tmp;
		}
	}

	// read battery threshold params on startup
	if (_crit_thr < 0.01f) {
		param_get(param_find("BAT_CRIT_THR"), &_crit_thr);
	}

	if (_low_thr < 0.01f) {
		param_get(param_find("BAT_LOW_THR"), &_low_thr);
	}

	if (_emergency_thr < 0.01f) {
		param_get(param_find("BAT_EMERGEN_THR"), &_emergency_thr);
	}

	// read data from sensor
	battery_status_s new_report = {};

	// set time of reading
	new_report.timestamp = now;

	if (read_reg(BATT_SMBUS_VOLTAGE, tmp) == OK) {
>>>>>>> 97f14edcbd3ff8526326d26d749656a8e8f309c9

	ret |= get_cell_voltages();

<<<<<<< HEAD
	// Convert millivolts to volts.
	new_report.voltage_v = ((float)result) / 1000.0f;
	new_report.voltage_filtered_v = new_report.voltage_v;

	// Read current.
	ret |= _interface->read_word(BATT_SMBUS_CURRENT, &result);

	new_report.current_a = (-1.0f * ((float)(*(int16_t *)&result)) / 1000.0f);
	new_report.current_filtered_a = new_report.current_a;

	// Read average current.
	ret |= _interface->read_word(BATT_SMBUS_AVERAGE_CURRENT, &result);

	float average_current = (-1.0f * ((float)(*(int16_t *)&result)) / 1000.0f);

	new_report.average_current_a = average_current;

	// If current is high, turn under voltage protection off. This is neccessary to prevent
	// a battery from cutting off while flying with high current near the end of the packs capacity.
	set_undervoltage_protection(average_current);

	// Read run time to empty.
	ret |= _interface->read_word(BATT_SMBUS_RUN_TIME_TO_EMPTY, &result);
	new_report.run_time_to_empty = result;

	// Read average time to empty.
	ret |= _interface->read_word(BATT_SMBUS_AVERAGE_TIME_TO_EMPTY, &result);
	new_report.average_time_to_empty = result;

	// Read remaining capacity.
	ret |= _interface->read_word(BATT_SMBUS_REMAINING_CAPACITY, &result);

	// Calculate remaining capacity percent with complementary filter.
	new_report.remaining = 0.8f * _last_report.remaining + 0.2f * (1.0f - (float)((float)(_batt_capacity - result) /
			       (float)_batt_capacity));

	// Calculate total discharged amount.
	new_report.discharged_mah = _batt_startup_capacity - result;

	// Check if max lifetime voltage delta is greater than allowed.
	if (_lifetime_max_delta_cell_voltage > BATT_CELL_VOLTAGE_THRESHOLD_FAILED) {
		new_report.warning = battery_status_s::BATTERY_WARNING_CRITICAL;
	}

	// Propagate warning state.
	else {
		if (new_report.remaining > _low_thr) {
			new_report.warning = battery_status_s::BATTERY_WARNING_NONE;

		} else if (new_report.remaining > _crit_thr) {
			new_report.warning = battery_status_s::BATTERY_WARNING_LOW;

		} else if (new_report.remaining > _emergency_thr) {
			new_report.warning = battery_status_s::BATTERY_WARNING_CRITICAL;

		} else {
			new_report.warning = battery_status_s::BATTERY_WARNING_EMERGENCY;
		}
	}

	// Read battery temperature and covert to Celsius.
	ret |= _interface->read_word(BATT_SMBUS_TEMP, &result);
	new_report.temperature = ((float)result / 10.0f) + CONSTANTS_ABSOLUTE_NULL_CELSIUS;

	new_report.capacity = _batt_capacity;
	new_report.cycle_count = _cycle_count;
	new_report.serial_number = _serial_number;
	new_report.cell_count = _cell_count;
	new_report.voltage_cell_v[0] = _cell_voltages[0];
	new_report.voltage_cell_v[1] = _cell_voltages[1];
	new_report.voltage_cell_v[2] = _cell_voltages[2];
	new_report.voltage_cell_v[3] = _cell_voltages[3];

	// Only publish if no errors.
	if (!ret) {
		orb_publish(ORB_ID(battery_status), _batt_topic, &new_report);

		_last_report = new_report;
	}

	if (should_exit()) {
		exit_and_cleanup();

	} else {

		while (_should_suspend) {
			px4_usleep(200000);
=======
		// convert millivolts to volts
		new_report.voltage_v = ((float)tmp) / 1000.0f;
		new_report.voltage_filtered_v = new_report.voltage_v;

		// read current
		if (read_reg(BATT_SMBUS_CURRENT, tmp) == OK) {
			new_report.current_a = ((float)convert_twos_comp(tmp)) / 1000.0f;
			new_report.current_filtered_a = new_report.current_a;
		}

		// read average current
		if (read_reg(BATT_SMBUS_AVERAGE_CURRENT, tmp) == OK) {
			new_report.average_current_a = ((float)convert_twos_comp(tmp)) / 1000.0f;
		}

		// read run time to empty
		if (read_reg(BATT_SMBUS_RUN_TIME_TO_EMPTY, tmp) == OK) {
			new_report.run_time_to_empty = tmp;
		}

		// read average time to empty
		if (read_reg(BATT_SMBUS_AVERAGE_TIME_TO_EMPTY, tmp) == OK) {
			new_report.average_time_to_empty = tmp;
		}

		// read remaining capacity
		if (read_reg(BATT_SMBUS_REMAINING_CAPACITY, tmp) == OK) {

			if (tmp > _batt_capacity) {
				PX4_WARN("Remaining Cap greater than total: Cap:%hu RemainingCap:%hu", (uint16_t)_batt_capacity, (uint16_t)tmp);
				_batt_capacity = (uint16_t)tmp;
			}

			// Calculate remaining capacity percent with complementary filter
			new_report.remaining = (float)(_last_report.remaining * 0.8f) + (float)(0.2f * (float)(1.000f - (((
						       float)_batt_capacity - (float)tmp) / (float)_batt_capacity)));

			// calculate total discharged amount
			new_report.discharged_mah = (float)((float)_batt_startup_capacity - (float)tmp);
		}

		// read battery temperature and covert to Celsius
		if (read_reg(BATT_SMBUS_TEMP, tmp) == OK) {
			new_report.temperature = (float)(((float)tmp / 10.0f) + CONSTANTS_ABSOLUTE_NULL_CELSIUS);
		}

		//Check if remaining % is out of range
		if ((new_report.remaining > 1.00f) || (new_report.remaining <= 0.00f)) {
			new_report.warning = battery_status_s::BATTERY_WARNING_EMERGENCY;
		}

		//Check if discharged amount is greater than the starting capacity
		else if (new_report.discharged_mah > (float)_batt_startup_capacity) {
			new_report.warning = battery_status_s::BATTERY_WARNING_EMERGENCY;
		}

		// propagate warning state
		else {
			if (new_report.remaining > _low_thr) {
				new_report.warning = battery_status_s::BATTERY_WARNING_NONE;

			} else if (new_report.remaining > _crit_thr) {
				new_report.warning = battery_status_s::BATTERY_WARNING_LOW;

			} else if (new_report.remaining > _emergency_thr) {
				new_report.warning = battery_status_s::BATTERY_WARNING_CRITICAL;

			} else {
				new_report.warning = battery_status_s::BATTERY_WARNING_EMERGENCY;
			}
>>>>>>> 97f14edcbd3ff8526326d26d749656a8e8f309c9
		}

		// Schedule a fresh cycle call when the measurement is done.
		work_queue(HPWORK, &_work, (worker_t)&BATT_SMBUS::cycle_trampoline, this,
			   USEC2TICK(BATT_SMBUS_MEASUREMENT_INTERVAL_US));
	}
}

<<<<<<< HEAD
void BATT_SMBUS::suspend()
{
	_should_suspend = true;
}

void BATT_SMBUS::resume()
{
	_should_suspend = false;
}

int BATT_SMBUS::get_cell_voltages()
{
	// Temporary variable for storing SMBUS reads.
	uint16_t result = 0;

	int ret = _interface->read_word(BATT_SMBUS_CELL_1_VOLTAGE, &result);
	// Convert millivolts to volts.
	_cell_voltages[0] = ((float)result) / 1000.0f;

	ret = _interface->read_word(BATT_SMBUS_CELL_2_VOLTAGE, &result);
	// Convert millivolts to volts.
	_cell_voltages[1] = ((float)result) / 1000.0f;
=======
		// publish to orb
		if (_batt_topic != nullptr) {
			orb_publish(_batt_orb_id, _batt_topic, &new_report);

		} else {
			_batt_topic = orb_advertise(_batt_orb_id, &new_report);
>>>>>>> 97f14edcbd3ff8526326d26d749656a8e8f309c9

	ret = _interface->read_word(BATT_SMBUS_CELL_3_VOLTAGE, &result);
	// Convert millivolts to volts.
	_cell_voltages[2] = ((float)result) / 1000.0f;

	ret = _interface->read_word(BATT_SMBUS_CELL_4_VOLTAGE, &result);
	// Convert millivolts to volts.
	_cell_voltages[3] = ((float)result) / 1000.0f;

	//Calculate max cell delta
	_min_cell_voltage = _cell_voltages[0];
	float max_cell_voltage = _cell_voltages[0];

<<<<<<< HEAD
	for (uint8_t i = 1; i < (sizeof(_cell_voltages) / sizeof(_cell_voltages[0])); i++) {
		_min_cell_voltage = math::min(_min_cell_voltage, _cell_voltages[i]);
		max_cell_voltage = math::max(_min_cell_voltage, _cell_voltages[i]);
	}

	// Calculate the max difference between the min and max cells with complementary filter.
	_max_cell_voltage_delta = (0.5f * (max_cell_voltage - _min_cell_voltage)) +
				  (0.5f * _last_report.max_cell_voltage_delta);

	return ret;
}

void BATT_SMBUS::set_undervoltage_protection(float average_current)
{
	// Disable undervoltage protection if armed. Enable if disarmed and cell voltage is above limit.
	if (average_current > BATT_CURRENT_UNDERVOLTAGE_THRESHOLD) {
		if (_cell_undervoltage_protection_status != 0) {
			// Disable undervoltage protection
			uint8_t protections_a_tmp = BATT_SMBUS_ENABLED_PROTECTIONS_A_CUV_DISABLED;
			uint16_t address = BATT_SMBUS_ENABLED_PROTECTIONS_A_ADDRESS;

			if (dataflash_write(address, &protections_a_tmp, 1) == PX4_OK) {
				_cell_undervoltage_protection_status = 0;
				PX4_WARN("Disabled CUV");

			} else {
				PX4_WARN("Failed to disable CUV");
			}
		}

	} else {
		if (_cell_undervoltage_protection_status == 0) {
			if (_min_cell_voltage > BATT_VOLTAGE_UNDERVOLTAGE_THRESHOLD) {
				// Enable undervoltage protection
				uint8_t protections_a_tmp = BATT_SMBUS_ENABLED_PROTECTIONS_A_DEFAULT;
				uint16_t address = BATT_SMBUS_ENABLED_PROTECTIONS_A_ADDRESS;

				if (dataflash_write(address, &protections_a_tmp, 1) == PX4_OK) {
					_cell_undervoltage_protection_status = 1;
					PX4_WARN("Enabled CUV");

				} else {
					PX4_WARN("Failed to enable CUV");
				}
			}
		}
	}

}

//@NOTE: Currently unused, could be helpful for debugging a parameter set though.
int BATT_SMBUS::dataflash_read(uint16_t &address, void *data)
{
	uint8_t code = BATT_SMBUS_MANUFACTURER_BLOCK_ACCESS;

	// address is 2 bytes
	int result = _interface->block_write(code, &address, 2, true);

	if (result != PX4_OK) {
		return result;
	}

	// @NOTE: The data buffer MUST be 32 bytes.
	result = _interface->block_read(code, data, DATA_BUFFER_SIZE + 2, true);

	// When reading a BATT_SMBUS_MANUFACTURER_BLOCK_ACCESS the first 2 bytes will be the command code
	// We will remove these since we do not care about the command code.
	//memcpy(data, &((uint8_t *)data)[2], DATA_BUFFER_SIZE);

	return result;
}

int BATT_SMBUS::dataflash_write(uint16_t &address, void *data, const unsigned length)
=======
		// copy report for test()
		_last_report = new_report;

		// record we are working
		_enabled = true;
	}

	// schedule a fresh cycle call when the measurement is done
	work_queue(HPWORK, &_work, (worker_t)&BATT_SMBUS::cycle_trampoline, this,
		   USEC2TICK(BATT_SMBUS_MEASUREMENT_INTERVAL_US));
}

int
BATT_SMBUS::read_reg(uint8_t reg, uint16_t &val)
{
	uint8_t buff[3];	// 2 bytes of data

	// read from register
	int ret = transfer(&reg, 1, buff, 3);

	if (ret == OK) {
		// check PEC
		uint8_t pec = get_PEC(reg, true, buff, 2);

		if (pec == buff[2]) {
			val = (uint16_t)buff[1] << 8 | (uint16_t)buff[0];

		} else {
			PX4_ERR("BATT_SMBUS PEC Check Failed");
			ret = ENOTTY;
		}
	}

	// return success or failure
	return ret;
}

int
BATT_SMBUS::write_reg(uint8_t reg, uint16_t val)
>>>>>>> 97f14edcbd3ff8526326d26d749656a8e8f309c9
{
	uint8_t buff[4];  // reg + 2 bytes of data + PEC

<<<<<<< HEAD
	uint8_t tx_buf[DATA_BUFFER_SIZE + 2] = {};
=======
	buff[0] = reg;
	buff[2] = uint8_t(val << 8) & 0xff;
	buff[1] = (uint8_t)val;
	buff[3] = get_PEC(reg, false, &buff[1],  2); // Append PEC
>>>>>>> 97f14edcbd3ff8526326d26d749656a8e8f309c9

	// write bytes to register
	int ret = transfer(buff, 3, nullptr, 0);

<<<<<<< HEAD
	// code (1), byte_count (1), addr(2), data(32) + pec
	int result = _interface->block_write(code, tx_buf, length + 2, false);

	return result;
=======
	if (ret != OK) {
		PX4_DEBUG("Register write error");
	}

	// return success or failure
	return ret;
}

uint16_t
BATT_SMBUS::convert_twos_comp(uint16_t val)
{
	if ((val & 0x8000) == 0x8000) {
		uint16_t tmp;
		tmp = ~val;
		tmp = tmp + 1;
		return tmp;
	}

	return val;
>>>>>>> 97f14edcbd3ff8526326d26d749656a8e8f309c9
}

uint8_t
BATT_SMBUS::read_block(uint8_t reg, uint8_t *data, uint8_t max_len, bool append_zero)
{
<<<<<<< HEAD
	int result = 0;
	// The name field is 21 characters, add one for null terminator.
	const unsigned name_length = 22;

	// Try and get battery SBS info.
	if (_manufacturer_name == nullptr) {
		char man_name[name_length] = {};
		result = manufacturer_name((uint8_t *)man_name, sizeof(man_name));

		if (result != PX4_OK) {
			PX4_WARN("Failed to get manufacturer name");
			return PX4_ERROR;
		}

		_manufacturer_name = new char[sizeof(man_name)];
=======
	uint8_t buff[max_len + 2];  // buffer to hold results

	// read bytes including PEC
	int ret = transfer(&reg, 1, buff, max_len + 2);

	// return zero on failure
	if (ret != OK) {
		return 0;
>>>>>>> 97f14edcbd3ff8526326d26d749656a8e8f309c9
	}

	// get length
	uint8_t bufflen = buff[0];

<<<<<<< HEAD
	result = _interface->read_word(BATT_SMBUS_SERIAL_NUMBER, &tmp);
	uint16_t serial_num = tmp;

	result |= _interface->read_word(BATT_SMBUS_REMAINING_CAPACITY, &tmp);
	uint16_t remaining_cap = tmp;

	result |= _interface->read_word(BATT_SMBUS_CYCLE_COUNT, &tmp);
	uint16_t cycle_count = tmp;

	result |= _interface->read_word(BATT_SMBUS_FULL_CHARGE_CAPACITY, &tmp);
	uint16_t full_cap = tmp;

	if (!result) {
		_serial_number = serial_num;
		_batt_startup_capacity = remaining_cap;
		_cycle_count = cycle_count;
		_batt_capacity = full_cap;
	}

	if (lifetime_data_flush() == PX4_OK) {
		// Flush needs time to complete, otherwise device is busy. 100ms not enough, 200ms works.
		px4_usleep(200000);

		if (lifetime_read_block_one() == PX4_OK) {
			if (_lifetime_max_delta_cell_voltage > BATT_CELL_VOLTAGE_THRESHOLD_FAILED) {
				PX4_WARN("Battery Damaged Will Not Fly. Lifetime max voltage difference: %4.2f",
					 (double)_lifetime_max_delta_cell_voltage);
			}
		}

	} else {
		PX4_WARN("Failed to flush lifetime data");
	}
=======
	// sanity check length returned by smbus
	if (bufflen == 0 || bufflen > max_len) {
		return 0;
	}

	// check PEC
	uint8_t pec = get_PEC(reg, true, buff, bufflen + 1);

	if (pec != buff[bufflen + 1]) {
		return 0;
	}

	// copy data
	memcpy(data, &buff[1], bufflen);
>>>>>>> 97f14edcbd3ff8526326d26d749656a8e8f309c9

	// optionally add zero to end
	if (append_zero) {
		data[bufflen] = '\0';
	}

	// return success
	return bufflen;
}

uint8_t
BATT_SMBUS::write_block(uint8_t reg, uint8_t *data, uint8_t len)
{
	uint8_t buff[len + 3];  // buffer to hold results

<<<<<<< HEAD
	if (_interface->read_word(BATT_SMBUS_SERIAL_NUMBER, &serial_num) == PX4_OK) {
		return serial_num;
	}
=======
	usleep(1);
>>>>>>> 97f14edcbd3ff8526326d26d749656a8e8f309c9

	buff[0] = reg;
	buff[1] = len;
	memcpy(&buff[2], data, len);
	buff[len + 2] = get_PEC(reg, false, &buff[1],  len + 1); // Append PEC

<<<<<<< HEAD
int BATT_SMBUS::manufacture_date()
{
	uint16_t date = PX4_ERROR;
	uint8_t code = BATT_SMBUS_MANUFACTURE_DATE;

	int result = _interface->read_word(code, &date);

	if (result != PX4_OK) {
		return result;
	}

	return date;
=======
	// send bytes
	int ret = transfer(buff, len + 3, nullptr, 0);

	// return zero on failure
	if (ret != OK) {
		PX4_DEBUG("Block write error");
		return 0;
	}

	// return success
	return len;
}

uint8_t
BATT_SMBUS::get_PEC(uint8_t cmd, bool reading, const uint8_t buff[], uint8_t len)
{
	// exit immediately if no data
	if (len <= 0) {
		return 0;
	}

	/**
	 *  Note: The PEC is calculated on all the message bytes. See http://cache.freescale.com/files/32bit/doc/app_note/AN4471.pdf
	 *  and http://www.ti.com/lit/an/sloa132/sloa132.pdf for more details
	 */

	// prepare temp buffer for calculating crc
	uint8_t tmp_buff_len;

	if (reading) {
		tmp_buff_len = len + 3;

	} else {
		tmp_buff_len = len + 2;
	}

	uint8_t tmp_buff[tmp_buff_len];
	tmp_buff[0] = (uint8_t)get_device_address() << 1;
	tmp_buff[1] = cmd;

	if (reading) {
		tmp_buff[2] = tmp_buff[0] | (uint8_t)reading;
		memcpy(&tmp_buff[3], buff, len);

	} else {
		memcpy(&tmp_buff[2], buff, len);
	}

	// initialise crc to zero
	uint8_t crc = 0;
	uint8_t shift_reg = 0;
	bool do_invert;

	// for each byte in the stream
	for (uint8_t i = 0; i < sizeof(tmp_buff); i++) {
		// load next data byte into the shift register
		shift_reg = tmp_buff[i];

		// for each bit in the current byte
		for (uint8_t j = 0; j < 8; j++) {
			do_invert = (crc ^ shift_reg) & 0x80;
			crc <<= 1;
			shift_reg <<= 1;

			if (do_invert) {
				crc ^= BATT_SMBUS_PEC_POLYNOMIAL;
			}
		}
	}

	// return result
	return crc;
>>>>>>> 97f14edcbd3ff8526326d26d749656a8e8f309c9
}

uint8_t
BATT_SMBUS::ManufacturerAccess(uint16_t cmd)
{
<<<<<<< HEAD
	uint8_t code = BATT_SMBUS_MANUFACTURER_NAME;
	uint8_t rx_buf[21] = {};

	// Returns 21 bytes, add 1 byte for null terminator.
	int result = _interface->block_read(code, rx_buf, length - 1, true);

	memcpy(man_name, rx_buf, sizeof(rx_buf));

	man_name[21] = '\0';

	return result;
}

void BATT_SMBUS::print_report()
{
	print_message(_last_report);
}

int BATT_SMBUS::manufacturer_read(const uint16_t cmd_code, void *data, const unsigned length)
{
	uint8_t code = BATT_SMBUS_MANUFACTURER_BLOCK_ACCESS;

	uint8_t address[2] = {};
	address[0] = ((uint8_t *)&cmd_code)[0];
	address[1] = ((uint8_t *)&cmd_code)[1];

	int result = _interface->block_write(code, address, 2, false);

	if (result != PX4_OK) {
		return result;
	}

	// returns the 2 bytes of addr + data[]
	result = _interface->block_read(code, data, length + 2, true);
	memcpy(data, &((uint8_t *)data)[2], length);

	return result;
=======
	// write bytes to Manufacturer Access
	int ret = write_reg(BATT_SMBUS_MANUFACTURER_ACCESS, cmd);

	if (ret != OK) {
		PX4_WARN("Manufacturer Access error");
	}

	return ret;
}

///////////////////////// shell functions ///////////////////////

void
batt_smbus_usage()
{
	PX4_INFO("missing command: try 'start', 'test', 'stop', 'search', 'man_name', 'man_date', 'dev_name', 'serial_num', 'dev_chem',  'sbs_info'");
	PX4_INFO("options:");
	PX4_INFO("    -b i2cbus (%d)", BATT_SMBUS_I2C_BUS);
	PX4_INFO("    -a addr (0x%x)", BATT_SMBUS_ADDR);
}

int
manufacturer_name()
{
	uint8_t man_name[21];
	uint8_t len = g_batt_smbus->manufacturer_name(man_name, sizeof(man_name));

	if (len > 0) {
		PX4_INFO("The manufacturer name: %s", man_name);
		return OK;

	} else {
		PX4_INFO("Unable to read manufacturer name.");
	}

	return -1;
>>>>>>> 97f14edcbd3ff8526326d26d749656a8e8f309c9
}

int
manufacture_date()
{
	uint16_t man_date = g_batt_smbus->manufacture_date();

<<<<<<< HEAD
	uint8_t address[2] = {};
	address[0] = ((uint8_t *)&cmd_code)[0];
	address[1] = ((uint8_t *)&cmd_code)[1];

	uint8_t tx_buf[DATA_BUFFER_SIZE + 2] = {};
	memcpy(tx_buf, address, 2);

	if (data != nullptr) {
		memcpy(&tx_buf[2], data, length);
	}

	int result = _interface->block_write(code, tx_buf, length + 2, false);

	return result;
}

int BATT_SMBUS::unseal()
{
	// See bq40z50 technical reference.
	uint16_t keys[2] = {0x0414, 0x3672};

	int ret = _interface->write_word(BATT_SMBUS_MANUFACTURER_ACCESS, &keys[0]);

	ret |= _interface->write_word(BATT_SMBUS_MANUFACTURER_ACCESS, &keys[1]);

	return ret;
}

int BATT_SMBUS::seal()
{
	// See bq40z50 technical reference.
	uint16_t reg = BATT_SMBUS_SEAL;

	return manufacturer_write(reg, nullptr, 0);
}

int BATT_SMBUS::lifetime_data_flush()
{
	uint16_t flush = BATT_SMBUS_LIFETIME_FLUSH;

	return manufacturer_write(flush, nullptr, 0);
}

int BATT_SMBUS::lifetime_read_block_one()
{

	uint8_t lifetime_block_one[32] = {};

	if (PX4_OK != manufacturer_read(BATT_SMBUS_LIFETIME_BLOCK_ONE, lifetime_block_one, 32)) {
		PX4_INFO("Failed to read lifetime block 1.");
		return PX4_ERROR;
	}

	//Get max cell voltage delta and convert from mV to V.
	_lifetime_max_delta_cell_voltage = (float)(lifetime_block_one[17] << 8 | lifetime_block_one[16]) / 1000.0f;

	PX4_INFO("Max Cell Delta: %4.2f", (double)_lifetime_max_delta_cell_voltage);

	return PX4_OK;
}

int BATT_SMBUS::custom_command(int argc, char *argv[])
{
	const char *input = argv[0];
	uint8_t man_name[22];
	int result = 0;

	BATT_SMBUS *obj = get_instance();

	if (!strcmp(input, "man_info")) {

		result = obj->manufacturer_name(man_name, sizeof(man_name));
		PX4_INFO("The manufacturer name: %s", man_name);

		result = obj->manufacture_date();
		PX4_INFO("The manufacturer date: %d", result);

		uint16_t serial_num = 0;
		serial_num = obj->get_serial_number();
		PX4_INFO("The serial number: %d", serial_num);

		return 0;
	}

	if (!strcmp(input, "unseal")) {
		obj->unseal();
		return 0;
	}

	if (!strcmp(input, "seal")) {
		obj->seal();
		return 0;
	}

	if (!strcmp(input, "report")) {
		obj->print_report();
		return 0;
	}

	if (!strcmp(input, "suspend")) {
		obj->suspend();
		return 0;
	}

	if (!strcmp(input, "resume")) {
		obj->resume();
		return 0;
	}

	if (!strcmp(input, "serial_num")) {
		uint16_t serial_num = obj->get_serial_number();
		PX4_INFO("Serial number: %d", serial_num);
		return 0;
	}

	if (!strcmp(input, "write_flash")) {
		if (argv[1] && argv[2]) {
			uint16_t address = atoi(argv[1]);
			unsigned length = atoi(argv[2]);
			uint8_t tx_buf[32] = {};

			if (length > 32) {
				PX4_WARN("Data length out of range: Max 32 bytes");
				return 1;
			}

			// Data needs to be fed in 1 byte (0x01) at a time.
			for (unsigned i = 0; i < length; i++) {
				tx_buf[i] = atoi(argv[3 + i]);
			}

			if (PX4_OK != obj->dataflash_write(address, tx_buf, length)) {
				PX4_INFO("Dataflash write failed: %d", address);
				px4_usleep(100000);
				return 1;

			} else {
				px4_usleep(100000);
				return 0;
=======
	if (man_date > 0) {
		// Convert the uint16_t into human-readable date format
		uint16_t year = ((man_date >> 9) & 0xFF) + 1980;
		uint8_t month = (man_date >> 5) & 0xF;
		uint8_t day = man_date & 0x1F;
		PX4_INFO("The manufacturer date is: %d which is %4d-%02d-%02d", man_date, year, month, day);
		return OK;

	} else {
		PX4_INFO("Unable to read the manufacturer date.");
	}

	return -1;
}

int
serial_number()
{
	uint16_t serial_num = g_batt_smbus->serial_number();
	PX4_INFO("The serial number: 0x%04x (%d in decimal)", serial_num, serial_num);

	return OK;
}

int
batt_smbus_main(int argc, char *argv[])
{
	int i2cdevice = BATT_SMBUS_I2C_BUS;
	int batt_smbusadr = BATT_SMBUS_ADDR; // 7bit address

	int ch;

	// jump over start/off/etc and look at options first
	while ((ch = getopt(argc, argv, "a:b")) != EOF) {
		switch (ch) {
		case 'a':
			batt_smbusadr = strtol(optarg, nullptr, 0);
			break;

		case 'b':
			i2cdevice = strtol(optarg, nullptr, 0);
			break;

		default:
			batt_smbus_usage();
			return 0;
		}
	}

	if (optind >= argc) {
		batt_smbus_usage();
		return 1;
	}

	const char *verb = argv[optind];

	if (!strcmp(verb, "start")) {
		if (g_batt_smbus != nullptr) {
			PX4_ERR("already started");
			return 1;

		} else {
			// create new global object
			g_batt_smbus = new BATT_SMBUS(i2cdevice, batt_smbusadr);

			if (g_batt_smbus == nullptr) {
				PX4_ERR("new failed");
				return 1;
			}

			if (OK != g_batt_smbus->init()) {
				delete g_batt_smbus;
				g_batt_smbus = nullptr;
				PX4_ERR("init failed");
				return 1;
>>>>>>> 97f14edcbd3ff8526326d26d749656a8e8f309c9
			}
		}

		return 0;
	}

<<<<<<< HEAD
	print_usage();

	return PX4_ERROR;
}

int BATT_SMBUS::print_usage()
{
	PRINT_MODULE_DESCRIPTION(
		R"DESCR_STR(
### Description
Smart battery driver for the BQ40Z50 fuel gauge IC.

### Examples
To write to flash to set parameters. address, number_of_bytes, byte0, ... , byteN
$ batt_smbus -X write_flash 19069 2 27 0

)DESCR_STR");

	PRINT_MODULE_USAGE_NAME("batt_smbus", "driver");

	PRINT_MODULE_USAGE_COMMAND("start");
	PRINT_MODULE_USAGE_PARAM_STRING('X', "BATT_SMBUS_BUS_I2C_EXTERNAL", nullptr, nullptr, true);
	PRINT_MODULE_USAGE_PARAM_STRING('T', "BATT_SMBUS_BUS_I2C_EXTERNAL1", nullptr, nullptr, true);
	PRINT_MODULE_USAGE_PARAM_STRING('R', "BATT_SMBUS_BUS_I2C_EXTERNAL2", nullptr, nullptr, true);
	PRINT_MODULE_USAGE_PARAM_STRING('I', "BATT_SMBUS_BUS_I2C_INTERNAL", nullptr, nullptr, true);
	PRINT_MODULE_USAGE_PARAM_STRING('A', "BATT_SMBUS_BUS_ALL", nullptr, nullptr, true);

	PRINT_MODULE_USAGE_COMMAND_DESCR("man_info", "Prints manufacturer info.");
	PRINT_MODULE_USAGE_COMMAND_DESCR("report",  "Prints the last report.");
	PRINT_MODULE_USAGE_COMMAND_DESCR("unseal", "Unseals the devices flash memory to enable write_flash commands.");
	PRINT_MODULE_USAGE_COMMAND_DESCR("seal", "Seals the devices flash memory to disbale write_flash commands.");
	PRINT_MODULE_USAGE_COMMAND_DESCR("suspend", "Suspends the driver from rescheduling the cycle.");
	PRINT_MODULE_USAGE_COMMAND_DESCR("resume", "Resumes the driver from suspension.");

	PRINT_MODULE_USAGE_COMMAND_DESCR("write_flash", "Writes to flash. The device must first be unsealed with the unseal command.");
	PRINT_MODULE_USAGE_ARG("address", "The address to start writing.", true);
	PRINT_MODULE_USAGE_ARG("number of bytes", "Number of bytes to send.", true);
	PRINT_MODULE_USAGE_ARG("data[0]...data[n]", "One byte of data at a time separated by spaces.", true);

	return PX4_OK;
}

int batt_smbus_main(int argc, char *argv[])
{
	return BATT_SMBUS::main(argc, argv);
=======
	// need the driver past this point
	if (g_batt_smbus == nullptr) {
		PX4_INFO("not started");
		batt_smbus_usage();
		return 1;
	}

	if (!strcmp(verb, "test")) {
		g_batt_smbus->test();
		return 0;
	}

	if (!strcmp(verb, "stop")) {
		delete g_batt_smbus;
		g_batt_smbus = nullptr;
		return 0;
	}

	if (!strcmp(verb, "search")) {
		g_batt_smbus->search();
		return 0;
	}

	if (!strcmp(verb, "man_name")) {
		manufacturer_name();
		return 0;
	}

	if (!strcmp(verb, "man_date")) {
		manufacture_date();
		return 0;
	}

	if (!strcmp(verb, "serial_num")) {
		serial_number();
		return 0;
	}

	if (!strcmp(verb, "sbs_info")) {
		manufacturer_name();
		manufacture_date();
		serial_number();
		return 0;
	}

	batt_smbus_usage();
	return 0;
>>>>>>> 97f14edcbd3ff8526326d26d749656a8e8f309c9
}
