#include <stdint.h>

#include "MPU9150.h"
#include "hw_ints.h"
#include "i2c_if.h"
#include "rom_map.h"
#include "utils.h"

// I2C address 0x69 could be 0x68 depends on your wiring.
int MPU9150_I2C_ADDRESS = 0x68;

//   //! \param ucDevAddr is the device I2C slave address
//! \param pucWrDataBuf is the pointer to the data to be written (reg addr)
//! \param ucWrLen is the length of data to be written
//! \param pucRdDataBuf is the pointer to the read data to be placed
//! \param ucRdLen is the length of data to be read
//!
//! This function works in a polling mode,
//!    1. Writes the data over I2C (device register address to be read from).
//!    2. In a loop, reads all the bytes over I2C
//!
//! \return 0: Success, < 0: Failure.
//
//****************************************************************************


void MPU9150_init()
{
  // Clear the 'sleep' bit to start the sensor.
  MPU9150_writeSensor(MPU9150_PWR_MGMT_1, 0);
  
  MAP_UtilsDelay(80000000); // 3 cycles per unit. for 1 second, at 80 MHz

  // Dump 8 readings
  uint8_t i;
  for(i = 0; i < 8; i++)
  {
    MPU9150_readSensor_2byte(MPU9150_ACCEL_ZOUT_L, MPU9150_ACCEL_ZOUT_H);
    MPU9150_readSensor_2byte(MPU9150_GYRO_XOUT_L, MPU9150_GYRO_XOUT_H);
  }
}

int MPU9150_readSensor_2byte(unsigned char addrL, unsigned char addrH)
{
	unsigned char value1, value2;

	/* From TI CC3200 i2c_if.h API */
	I2C_IF_ReadFrom(MPU9150_I2C_ADDRESS, &addrL, 1, &value1, 1);
	I2C_IF_ReadFrom(MPU9150_I2C_ADDRESS, &addrH, 1, &value2, 1);

	return (int16_t)((value2<<8)+value1);
}



int MPU9150_readSensor(unsigned char addr)
{
	unsigned char value;

	/* From TI CC3200 i2c_if.h API */
	I2C_IF_ReadFrom(MPU9150_I2C_ADDRESS, &addr, 1, &value, 1);

	return (int)value;
}

int MPU9150_writeSensor(unsigned char addr,unsigned char data)
{
	unsigned char write_data[2];
	write_data[0] = addr;
	write_data[1] = data;
	/* From TI CC3200 i2c_if.h API */
	I2C_IF_Write(MPU9150_I2C_ADDRESS, write_data, 2, 1);

	return 1;
}
