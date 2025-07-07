# Sensos4-BLE-ESP32
BLE Device Senso4s connected to ESP32 to read basic values

Manufacturer Homepage: https://www.senso4s.com/

The Senso4s is a gauge. It weighs the gas bottle. It reports the filling level in % as Int (0-100).
The weight of the bottle and other parameters can be easily set by an app from the manufacturer.
The below code extracts the % filling level. It only connects for a short time, 
so that the app can be used as well. As with most BLE devices, only one device can connect at a time.

For this characteristic("00007082-A20B-4D4D-A4DE-7F071DBBC1D8"), the device supports BLE read and notify for the %-filling.
Both are configured in this test setup. Most other BLE-devices reveal their values only via notify and therefore "read" is for reference and commented out(!).

**This is a fully functioning test program that will be imported into a Camper-Monitoring-Display project.**

**Important:**
The gauge only updates every 15 minutes if there has been no change(!)
With changes it updates more frequently.
If the bottle has been exchanged, refilled or moved, the app needs to be used to make the necessary adjustments. 
Only then will the reading reflect the new filling level correctly.
As the 2.4GHz Frequenz is used by a lot of devices, it may happen that a connection / reconnection fails. Therefore it is advisable to try a few times. 
This has been added to re-connect and connect.
The device requests connection parameters that I have set to the same value during initialisation [updateConnParams(80,160,0,400)]. This will save a little time while connecting.

The NimBLE-Arduino BLE stack (2.3.2) is used for the connection. https://github.com/h2zero/NimBLE-Arduino

The ESP-32 Dev Kit 4 is used as a hardware platform.

Arduino-IDE 2.3.6 compiled it nicely.

