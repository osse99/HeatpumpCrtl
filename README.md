# HeatpumpCrtl
Heatpump controller for RaspberryPi

## Functionality
* Measure return temperature from pipes embedded in the floor and the hot water tank.
* If temperatures are below threshold start heat pump and run until:
* * Floor - max temperature to floor
* * Hot water tank - until max heat pump temperature

## Guardrails
* If pump does not provide any heat within X minutes abort
* If pump is running for longer than Z minutes abort


### The target floor temperature is calculated based on the outdoor temperature and a simple y=kx+m equation.

### Might be working now so v0.3 :-)


## Some documentation in the pdf
* Piping
* Temperature sensor placement ds18b20
* RPi GPIO connections

## Components
* NIBE F2026 air/water heat pump
* LunnaTeknik slingtank 100+100 liters
* Grundfos UPM PWM controlled circulation pumps
* Myson MSDV322 switch valve
* 80 liter hot water boiler
* NIBE EVC 13 electric boiler
* RPi 3b
* CMOS 4049 Hex buffer
* Opto isolated 2 channel relay card


##Dependencies

#wiringpi library
    5  wget https://github.com/WiringPi/WiringPi/releases/download/3.14/wiringpi_3.14_armhf.deb
    6  ls
    7  dpkg -i wiringpi_3.14_armhf.deb

#libconfig
apt install libconfig-dev

##Autostart
Tailor pump_controller.service to your needs
Copy pump_controller.service to /etc/systemd/system/
Run systemctl daemon-reload

##Statistics
Provided "as is", publishes temp data to a javascript page using mqtt in  real time and populates diagram with history on reload
MQTT server and database on othe host
Page is at tapmuk1.tapmuk.se/history.html

