#!/bin/bash
#
# 
function get_temp() {
	tail -n1 ${SIMDIR}/${1} | cut -d '=' -f2
}

function set_temp() {
	echo -e "95 00 7f 80 7f ff 0b 10 b9 : crc=b9 YES\n95 00 7f 80 7f ff 0b 10 b9  t=${2}" > ${SIMDIR}/${1}
}

function current_temps() {
echo
echo -n -e "Outdoor:\t\t"
	get_temp ${OUTDOOR}
echo -n -e "Output to floor:\t"
	get_temp ${OUTPUT_TO_FLOOR}
echo -n -e "Return_from_floor:\t"
	get_temp ${RETURN_FROM_FLOOR}
echo -n -e "Hot_water:\t\t"
	get_temp ${HOT_WATER}
echo -n -e "Pump output:\t\t"
	get_temp ${PUMP_OUTPUT}
echo -n -e "Indoor:\t\t\t"
	get_temp ${INDOOR}
echo
}

#
# Script starting
#
#
SIMDIR=/tmp/simulate


# Check args
if [ -z "$1" ]
then
	echo "Usage: ${0} configfile"
	exit -1
fi
if [ ! -f "$1" ]
then
	echo "${0}: configfile not found"
	exit -1
fi
#
CONFIGFILE=$1
#
OUTDOOR=`grep '^OUTDOOR' ${CONFIGFILE} | sed -e 's/.*=//' -e 's/"//g'`
OUTPUT_TO_FLOOR=`grep '^OUTPUT_TO_FLOOR' ${CONFIGFILE} | sed -e 's/.*=//' -e 's/"//g'`
RETURN_FROM_FLOOR=`grep '^RETURN_FROM_FLOOR' ${CONFIGFILE} | sed -e 's/.*=//' -e 's/"//g'`
HOT_WATER=`grep '^HOT_WATER' ${CONFIGFILE} | sed -e 's/.*=//' -e 's/"//g'`
PUMP_OUTPUT=`grep '^PUMP_OUTPUT' ${CONFIGFILE} | sed -e 's/.*=//' -e 's/"//g'`
INDOOR=`grep '^INDOOR' ${CONFIGFILE} | sed -e 's/.*=//' -e 's/"//g'`
#
echo "Sensor adresses"
echo ${OUTDOOR}
echo ${OUTPUT_TO_FLOOR}
echo ${RETURN_FROM_FLOOR}
echo ${HOT_WATER}
echo ${PUMP_OUTPUT}
echo ${INDOOR}
#
# Baseline temperatures
set_temp ${OUTDOOR} 0
set_temp ${OUTPUT_TO_FLOOR} 23500
set_temp ${RETURN_FROM_FLOOR} 23500
set_temp ${HOT_WATER} 45000
set_temp ${PUMP_OUTPUT} 17000
set_temp ${INDOOR} 20000

current_temps
echo "Start pump_controller with simulate attribute"
sleep 20

date
echo "Test 1 Heat and hot water needed"
echo "Setting Return from floor and hot water to below heat needed theshold"
set_temp ${RETURN_FROM_FLOOR} 18000
set_temp ${HOT_WATER} 37000
current_temps
echo "Waiting 60s"
sleep 60
date
echo "Setting pump output higher than out to floor"
set_temp ${PUMP_OUTPUT} 28000
current_temps
echo "Waiting 120s to allow pump output hotter than output to floor validation to complete"
sleep 120
date
echo "Setting Output_to_floor above max"
set_temp ${OUTPUT_TO_FLOOR} 30250
set_temp ${RETURN_FROM_FLOOR} 29000
current_temps
date
echo "Pumpcontroller should switch to HW porduction, Waiting 60s"
sleep 60
date
echo "Setting pump output above max, pumpcontroller should stop"
set_temp ${PUMP_OUTPUT} 51000
current_temps
echo "done"


echo "Wait longer than pump restart delay, 1850 seconds "
sleep 1850


date
echo "Test 2 Only hot water needed"
echo "Setting hot water to below heat needed theshold"
set_temp ${HOT_WATER} 37000
set_temp ${PUMP_OUTPUT} 18000
current_temps
echo "Pumpcontroller should start producing HW Waiting 60s"
sleep 60
date
echo "Setting pump output higher"
set_temp ${PUMP_OUTPUT} 38000
current_temps
echo "Waiting 120s to allow pump output hotter than hotwater validation to complete"
sleep 120
date
echo "Setting pump output above max, pumpcontroller should stop"
set_temp ${HOT_WATER} 45000
set_temp ${PUMP_OUTPUT} 51000
current_temps
echo "done"


echo "Wait longer than pump restart delay, 1850 seconds "
sleep 900
set_temp ${PUMP_OUTPUT} 21000
set_temp ${RETURN_FROM_FLOOR} 24000
sleep 950


date
echo "Test 3 Heat and hot water needed but pump never starts up"
echo "Setting Return from floor and hot water to below heat needed theshold"
set_temp ${PUMP_OUTPUT} 17000
set_temp ${RETURN_FROM_FLOOR} 18000
set_temp ${OUTPUT_TO_FLOOR} 18000
set_temp ${HOT_WATER} 37000
current_temps
sleep 240
date
echo "pumpcontroller should terminate"
echo "done"
