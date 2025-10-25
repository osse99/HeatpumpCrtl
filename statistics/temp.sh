while [ 1 ]
do
echo "Retur"
cat /sys/bus/w1/devices/w1_bus_master1/28-000009ff2514/w1_slave
echo "Framledning"
cat /sys/bus/w1/devices/w1_bus_master1/28-00000b5450f9/w1_slave
echo "Värmepump"
cat /sys/bus/w1/devices/w1_bus_master1/28-00000b5b88a3/w1_slave
echo "Varmvatten"
cat /sys/bus/w1/devices/w1_bus_master2/28-000000384a6d/w1_slave
echo "Innetemp"
cat /sys/bus/w1/devices/w1_bus_master2/28-00000039122b/w1_slave
echo "Utomhus"
cat /sys/bus/w1/devices/w1_bus_master1/28-00000b5c6f2a/w1_slave
echo
sleep 10
done
