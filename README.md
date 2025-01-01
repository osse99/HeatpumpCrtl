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

### Might be working now so v0.1 :-)


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

