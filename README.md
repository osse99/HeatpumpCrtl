# HeatpumpCrtl
Heatpump controller for RaspberryPi

Functionality
Measure return temperature from pipes embedded in the floor and the hot water tank.
If temperatures are below threshold start heat pump and run until:
o Floor - max temperature to floor
o Hot water tank - until max heat pump temperature

The target floor temperature is calculated based on the outdoor temperature and a simple y=kx+m equation.

Might be working now so v0.1 :-)

Some documentation in the pdf
o Piping
o Temperature sensor placement ds18p20
o RPi GPIO connections

Components
o NIBE F2026 ari/water heat pump
0 LunnaTeknik slingtank 100+100 liters
0 Grundfos UPM PWM controlled cisculation pumps
0 Myson MSDV322 switch valve
o 80 liters hot water boiler
o NIBE electric boiler
