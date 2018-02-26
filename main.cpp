#define addr 0x40       // slave address (AD0 tied to GND)
#define measureRH 0xE5  // measure relative humidity, hold master mode
#define readTemp 0xE0   // read temperature from previous RH measurement
#define writeReg1 0xE6  // write user register #1 (RH/T measurement setup)
#define writeReg2 0x50  // write user register #2 (voltage measurement setup)
#define writeReg3 0x51  // write user register #3 (heater setup)
#define readReg1 0xE7   // read user register #1 (RH/T measurement setup)
#define readReg2 0x10   // read user register #2 (voltage measurement setup)
#define readReg3 0x11   // read user register #3 (heater setup)
#define heaterEnableBit 2      // BIT2 of user register 1
#define readFirmwareHigh 0x84  // read high byte of firmware revision
#define readFirmwareLow 0xB8   // read low  byte of firmware revision
#define readID1High 0xFA  // read high byte of the 1st word of the electronic ID
#define readID1Low 0x0F   // read low  byte of the 1st word of the electronic ID
#define readID2High 0xFC  // read high byte of the 2nd word of the electronic ID
#define readID2Low 0xC9   // read low  byte of the 2nd word of the electronic ID

#include <linux/i2c-dev.h>  // I2C_SLAVE
#include <sys/ioctl.h>  // ioctl()
#include <fcntl.h>      // open()
#include <unistd.h>     // read(), write(), usleep()
#include <stdio.h>      // printf()
#include <stdint.h>     // uint8_t
#include <string>       // string
#include <iostream>     // cout
#include <iomanip>      // setfill, setw
#include <bitset>       // bitset
#include <cstdlib>      // EXIT_SUCCESS, EXIT_FAILURE
using namespace std;

int i2cFileDescriptor;  // upon opening a device file the kernel returns a file descriptor i.e. an integer that is used as reference for subsequent system calls

class Si7013A20{
		public:
		char txBuffer[2];  // transmit buffer
		char rxBuffer[3];  // receive buffer
		float relativeHumidity;
		float tempCelsius;
		float tempFahrenheit;

		void transmitByte(int cmd, const string &message);
		void transmitWord(int cmdHigh, int cmdLow, const string &message);
		void receiveData(int numberOfBytesToReceive, const string &message);
		void readRHT();
		void heaterControls(const string &state, int current);
		void readDetail();
};

	void Si7013A20::transmitByte(int cmd, const string &message){  // function for sending a single byte
		txBuffer[0] = cmd;                                         // load transmit buffer with command code
		if(write(i2cFileDescriptor, txBuffer, 1) != 1){            // send command code
			cout << "Error: Failed " << message << '\n' << endl;
			exit(EXIT_FAILURE);
		}
	}

	void Si7013A20::transmitWord(int cmdHigh, int cmdLow, const string &message){  // function for sending a 2-byte word
		txBuffer[0] = cmdHigh;                                                     // load transmit buffer with command code
		txBuffer[1] = cmdLow;
		if(write(i2cFileDescriptor, txBuffer, 2) != 2){                            // send command code
			cout << "Error: Failed " << message << '\n' << endl;
			exit(EXIT_FAILURE);
		}
	}

	void Si7013A20::receiveData(int numberOfBytesToReceive, const string &message){
		if(read(i2cFileDescriptor, rxBuffer, numberOfBytesToReceive) != numberOfBytesToReceive){  // read response from slave
			cout << "Error: Failed " << message << '\n' << endl;
			exit(EXIT_FAILURE);
		}
	}

	void Si7013A20::readRHT(){
		transmitByte(measureRH, "Relative Humidity Measure Request\n");  // send request for relative humidity measurement
		usleep(25000);                                     // conversion time = tCONV(RH) + tCONV(T) = 12ms + 10.8ms = 22.8ms ~ 25ms 
		receiveData(3, "Relative Humidity Read\n");        // read RH code MSB, LSB, checksum (3-bytes)
		uint16_t rhCode = rxBuffer[0] << 8 | rxBuffer[1];  // merge high and low RH code readings
		relativeHumidity = ((125*(float)rhCode)/65536)-6;  // RH equation per datasheet

		uint8_t calcCRC, i ,j = 0;
		for (i = 0; i < 2; i++){                           // calculate RH measurement CRC checksum
			calcCRC ^= rxBuffer[i];
			for (j = 8; j > 0; j--){
				if (calcCRC & 0x80)
					calcCRC = (calcCRC << 1) ^ 0x131;
				else
					calcCRC = (calcCRC << 1);
			}
		}
		cout << "RH Code             0x" << setfill('0') << setw(2) << hex << uppercase << (int)rhCode << endl;
		cout << "RH Checksum (RX)    0x" << setfill('0') << setw(2) << hex << uppercase << (int)rxBuffer[2] << endl;
		cout << "RH Checksum (Calc)  0x" << setfill('0') << setw(2) << hex << uppercase << (int)calcCRC << endl;
		cout << "Relative Humidity   " << std::fixed << std::setprecision(2) << relativeHumidity << '%' << endl;

		transmitByte(readTemp, "Temperature Read Request");    // send request for temperature reading from previous RH measurement
		receiveData(2, "Temperature Read");                    // read temperature code MSB, LSB (2-bytes)
		uint16_t tempCode = rxBuffer[0] << 8 | rxBuffer[1];    // merge high and low temp code readings	
		tempCelsius = ((175.72*(float)tempCode)/65536)-46.85;  // temperature (Celsius) equation per datasheet
		tempFahrenheit = (tempCelsius*1.8) + 32;
		cout << "\nTemperature Code    0x" << setfill('0') << setw(2) << hex << uppercase << (int)tempCode << endl;	// unicode degree symbol: \u00B0
		cout << "Temperature         " << std::fixed << std::setprecision(2) << tempCelsius << "\u00B0C | " << tempFahrenheit << "\u00B0F" << endl;
	}

	void Si7013A20::heaterControls(const string &state, int current = 0){
		// Heater Enable
		transmitByte(readReg1, "User Register-1 Read Request (Heater Enable)");  // send request to read user register-1
		receiveData(1, "User Register-1 Read (Heater Enable)");                  // read user register-1 (BIT2 = Heater Enable)
		if(state == "on")                                                        // if user requests heater on
			rxBuffer[0] |= (1 << heaterEnableBit);                               // set BIT2
		else                                                                     // else turn heater off
			rxBuffer[0] &= ~(1 << heaterEnableBit);                              // clear BIT2
		transmitWord(writeReg1, rxBuffer[0], "Update Register-1 Write (Heater Enable)");  // write updated user register-1 (BIT2 = Heater Enable)
		cout << "Updated User Register #1   0b" << bitset<8>(rxBuffer[0]) << endl;        // bitset to output binary representation
		// Heater Current
		transmitByte(readReg3, "User Register-3 Read Request (Heater Current)");  // send request to read user register-3
		receiveData(1, "User Register-3 Read (Heater Current)");                  // read user register-3 (BITS[3:0] = Heater Current)
		rxBuffer[0] &= 0xF0;                                                      // clear heater current bits
		if(current >= 0 && current <= 15)                                         // if user requests heater current in valid range (0-15)
			rxBuffer[0] |= current;                                               // set BITS[3:0]
		transmitWord(writeReg3, rxBuffer[0], "Update Register-3 Write (Heater Current)");  // write updated user register-3 (BITS[3:0] = Heater Current)
		cout << "Updated User Register #3   0b" << bitset<8>(rxBuffer[0]) << endl;
	}

	void Si7013A20::readDetail(){
		transmitByte(readReg1, "User Register-1 Read Request");              // send request to read user register-1
		receiveData(1, "User Register-1 Read");                              // read user register-1 (1-byte)
		cout << "User Register #1    0b" << bitset<8>(rxBuffer[0]) << endl;  // bitset to output binary representation

		transmitByte(readReg2, "User Register-2 Read Request");              // send request to read user register-2
		receiveData(1, "User Register-2 Read");                              // read user register-2 (1-byte)
		cout << "User Register #2    0b" << bitset<8>(rxBuffer[0]) << endl;

		transmitByte(readReg3, "User Register-3 Read Request");              // send request to read user register-3
		receiveData(1, "User Register-3 Read");                              // read user register-3 (1-byte)
		cout << "User Register #3    0b" << bitset<8>(rxBuffer[0]) << endl;
		
		transmitWord(readFirmwareHigh, readFirmwareLow, "Firmware Revision Read Request");  // send request for firmware revision
		receiveData(2, "Firmware Revision Read");                                           // read firmware revision MSB, LSB (2 bytes)
		cout << "Firmware Revision   0x" << setfill('0') << setw(2) << hex << uppercase << (int)rxBuffer[0] << endl;
	}


int main(int argc, char** argv){

	if((i2cFileDescriptor = open("/dev/i2c-1", O_RDWR)) < 0){                // open the i2c device adapter with access mode read/write (O_RDWR)
		cout << "Error: Unable Open to I2C Device Adapter" << '\n' << endl;  // $ls /sys/class/i2c-dev (to determine adapter name)
		exit(EXIT_FAILURE);
	}
	if(ioctl(i2cFileDescriptor, I2C_SLAVE, addr) < 0){  // set address of the slave via i2c-dev request code I2C_SLAVE
		cout << "Error: No Response from Slave" << '\n' << endl;
		exit(EXIT_FAILURE);
	}

	Si7013A20 humidityTemperatureSensor;  // instantiate class object
	string commandLineArgument;

	if(argc >= 2)                         // if the user has passed in an argument use it as the script option
		commandLineArgument = argv[1];

	if(commandLineArgument == "read" || commandLineArgument.empty()){  // read relative humidity (RH) and temperature (T)
		humidityTemperatureSensor.readRHT();
		exit(EXIT_SUCCESS);
	}
	else if(commandLineArgument == "heater"){   // turn heater on/off, set heater current
		if(argc == 4)                           // if 3 arguments have been passed in e.g. (heater on 10)
			humidityTemperatureSensor.heaterControls(argv[2], atoi(argv[3]));  // set heater state, heater current per user input, string to int with atoi()
		else if(argc == 3)                                      // else if 2 arguments have been passed in e.g. (heater off)
			humidityTemperatureSensor.heaterControls(argv[2]);  // set heater state per user input, heater current will default to 0
		else                                                    // else only (heater) has been passed in
			humidityTemperatureSensor.heaterControls("off");    // set heater state to off, heater current will default to 0
		exit(EXIT_SUCCESS);
	}
	else if(commandLineArgument == "detail"){  // read Si7013-A20 details (user registers #1-3, firmware revision)
		humidityTemperatureSensor.readDetail();
		exit(EXIT_SUCCESS);
	}
	else{
		cout << "Error: Unkown Command Line Argument" << endl;
		exit(EXIT_FAILURE);
	}
}