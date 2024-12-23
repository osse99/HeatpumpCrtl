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
set_temp ${OUTPUT_TO_FLOOR} 20000
set_temp ${RETURN_FROM_FLOOR} 23500
set_temp ${HOT_WATER} 45000
set_temp ${PUMP_OUTPUT} 20000
set_temp ${INDOOR} 20000

current_temps
