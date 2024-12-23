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

Piping

Temperature sensor placement ds18p20

RPi GPIO connections


Components

o NIBE F2026 ari/water heat pump

o LunnaTeknik slingtank 100+100 liters

o Grundfos UPM PWM controlled cisculation pumps

o Myson MSDV322 switch valve

o 80 liters hot water boiler

o NIBE EVC 13 electric boiler

o RPi 3b

o CMOS 4049 Hex buffer

o Opto isolated 2 channel relay card

