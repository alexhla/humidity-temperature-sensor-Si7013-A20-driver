# humidity-temperature-sensor-Si7013-A20-driver
Driver for Silicon Labs Si7013-A20 I2C Humidity and Two-Zone Temperature Sensor

<p align="center">
	<img src="pinout.jpg">
</p>

##### Download Source Files
$ curl -LO https://github.com/alexhla/humidity-temperature-sensor-Si7013-A20-driver/archive/master.zip
##### Unzip
$ unzip master.zip
##### Navigate to Project
$ cd humidity-temperature-sensor-Si7013-A20-driver-master
##### Compile
$ g++ main.cpp
##### Run
$ ./a.out

##### Read Humidity & Temperature
<p align="center">
	<img src="demo-default.jpg">
	<img src="demo-read.jpg">
</p>

##### Read Detail Registers
<p align="center">
	<img src="demo-detail.jpg">
</p>

##### Turn Heater ON with current (mA) setting 0-9
<p align="center">
	<img src="demo-heater-on.jpg">
</p>

##### Turn Heater OFF
<p align="center">
	<img src="demo-heater-off.jpg">
</p>