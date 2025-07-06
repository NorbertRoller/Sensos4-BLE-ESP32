
// Manufacturer Homepage: https://www.senso4s.com/

/* The Senso4s is a gauge. It weighs the gas bottle. It reports the filling level in % as Int (0-100).
The weight of the bottle and other parameters can be easily set by an app from the manufacturer.
The below code extracts the % filling level. It only connects for a short time, 
so that the app can be used as well. As with most BLE devices, only one device can connect at a time.

For this characteristic("00007082-A20B-4D4D-A4DE-7F071DBBC1D8"), the device supports BLE read and notify for the %-filling.
Both are configured in this test setup. Most other BLE-devices reveal their values only via notify and therefore "read" is for reference and commented out(!).

This is a fully functioning test program that will be imported into a Camper-Monitoring-Display project.

Important:
The gauge only updates every 15 minutes if there has been no change(!)
With changes it updates more frequently.
If the bottle has been exchanged, refilled or moved, the app needs to be used to make the necessary adjustments. 
Only then will the reading reflect the new filling level correctly.
As the 2.4GHz Frequenz is used by a lot of devices, it may happen that a connection / reconnection fails. Therefore it is advisable to try a few times. 
This has been added to re-connect and connect.
The device requests connection paramters that I have set to the same value during initialisation [updateConnParams(80,160,0,400)]. This will save a little time while connecting.

The NimBLE-Arduino BLE stack (2.3.2) is used for the connection. https://github.com/h2zero/NimBLE-Arduino
The ESP-32 Dev Kit 4 is used as a hardware platform.
Arduino-IDE 2.3.6 compiled it nicely..

*/
//----------------------------------------------------------------------------

#include <Arduino.h>
#include <NimBLEDevice.h>
NimBLEScan *pBLEScan;
NimBLEAddress MAC_Senso4s("fe:2b:c1:ce:75:a0", 1); // 0==Public, 1== Random --> BLE_ADDR_PUBLIC (0) BLE_ADDR_RANDOM (1)

uint lastAdv = millis();
static bool doConnect_Senso4s = false;
#define nullptr NULL
unsigned int Gas_Proz=0;           // Gas bottle filling level in %
static const NimBLEAdvertisedDevice* advDevice_Senso4s;

NimBLEClient* pClient_Senso4s = nullptr;
NimBLERemoteService* pSvc_Senso4s = nullptr;
NimBLERemoteCharacteristic* pChr_Senso4s = nullptr;
NimBLERemoteDescriptor* pDsc_Senso4s = nullptr;       // Not in use

/**  None of these are required as they will be handled by the library with defaults. **
 **                       Remove as you see fit for your needs                        */
class ClientCallbacks : public NimBLEClientCallbacks 
  {
  void onConnect(NimBLEClient* pClient) 
    {
    Serial.println("Connected");
    /** After connection we should change the parameters if we don't need fast response times.
    *  These settings are 150ms interval, 0 latency, 450ms timout.
    *  Timeout should be a multiple of the interval, minimum is 100ms.
    *  I find a multiple of 3-5 * the interval works best for quick response/reconnect.
    *  Min interval: 120 * 1.25ms = 150, Max interval: 120 * 1.25ms = 150, 0 latency, 60 * 10ms = 600ms timeout
    */
    pClient->updateConnParams(80,160,0,400);    // Exactly what the Device request
    };

  void onDisconnect(NimBLEClient* pClient, int reason) 
    {
    Serial.printf("%s Disconnected, reason = %d \n",
    pClient->getPeerAddress().toString().c_str(), reason);
    };

  /** Called when the peripheral requests a change to the connection parameters.
   *  Return true to accept and apply them or false to reject and keep
   *  the currently used parameters. Default will return true.
   */
  bool onConnParamsUpdateRequest(NimBLEClient* pClient, const ble_gap_upd_params* params)   // Only once declared for all Devices
    {
    // Accept all changes the client requested
    Serial.println("ClientCallback: onConnParamsUpdateRequest");
    Serial.printf("Device MAC address: %s\n", pClient->getPeerAddress().toString().c_str());
     
    Serial.print("onConnParamsUpdateRequest itvl_min, New Value: ");    //requested Minimum value for connection interval 1.25ms units
    Serial.println(params->itvl_min,DEC);

    Serial.print("onConnParamsUpdateRequest itvl_max, New Value: ");    //requested Minimum value for connection interval 1.25ms units
    Serial.println(params->itvl_max,DEC);

    Serial.print("onConnParamsUpdateRequest latency, New Value: ");
    Serial.println(params->latency,DEC);

    Serial.print("onConnParamsUpdateRequest supervision_timeout, New Value:");
    Serial.println(params->supervision_timeout,DEC);

    Serial.printf("NimBLE: onConnParamsUpdateRequest accepted. MAC address: %s\n", pClient->getPeerAddress().toString().c_str());
    return true; // True
    }
  };

class scanCallbacks: public NimBLEScanCallbacks
  {
  void onResult(const NimBLEAdvertisedDevice* advertisedDevice) override 
    {
    Serial.printf("NimBLE_scan: Discovered Device: %s Type: %d \n", advertisedDevice->toString().c_str(),advertisedDevice->getAddressType());

    //Check for Senso4s MAC Adresse
    if (advertisedDevice->getAddress().equals(MAC_Senso4s)) // Discovered MAC == Target MAC ?
      {Serial.println("NimBLE_scan: Senso4s an MAC-Adresse detected.");
      if(advertisedDevice->isAdvertisingService(NimBLEUUID("7081"))) //SENSO4S
        {
        Serial.println("NimBLE_scan: Found Senso4s - BLE Service");
        advDevice_Senso4s = advertisedDevice;
        doConnect_Senso4s = true;
        Serial.println("NimBLE_scan: doConnect_Senso4s = true");
        }
      }
    }

  /** Callback to process the results of the completed scan or restart it */
  void onScanEnd(const NimBLEScanResults& results, int reason) 
    {
    Serial.println("NimBLE_scan: Scan Ended; reason = ");
    Serial.println(reason);
    }
  } scanCallbacks;

/** Notification / Indication receiving handler callback */
void notifyCB_Senso4s(NimBLERemoteCharacteristic* pRemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify)
  {
  std::string str = (isNotify == true) ? "Notification" : "Indication";
  str += " from ";
  str += std::string(pRemoteCharacteristic->getClient()->getPeerAddress());
  str += ": Service = " + std::string(pRemoteCharacteristic->getRemoteService()->getUUID());
  str += ", Characteristic = " + std::string(pRemoteCharacteristic->getUUID());
  str += ", Value = " + std::string((char*)pData, length);
  Serial.println(str.c_str());
  Serial.printf("Value in Hex %x Dec %d\n",pData[0],pData[0]);
  Gas_Proz=pData[0];

  if(pClient_Senso4s)                       // Only 1 reading required, Disconnect if active
    {pClient_Senso4s->disconnect();
    Serial.println("Senso4S-BLE disconnect");}
  }

/** Create a single global instance of the callback class to be used by all clients */
static ClientCallbacks clientCB;

/** Handles the provisioning of clients and connects / interfaces with the server */
bool connectToServer_Senso4s()
  {
  /** Check if we have a client we should reuse first **/
  if(NimBLEDevice::getCreatedClientCount()) 
    {
    /** Special case when we already know this device, we send false as the
     *  second argument in connect() to prevent refreshing the service database.
     *  This saves considerable time and power.
    */
    pClient_Senso4s = NimBLEDevice::getClientByPeerAddress(advDevice_Senso4s->getAddress());
    if(pClient_Senso4s)
      {
      if(!pClient_Senso4s->connect(advDevice_Senso4s, false)) 
        {
        Serial.println("Senso4S-1. Reconnect failed");
        if(!pClient_Senso4s->connect(advDevice_Senso4s, false))     // Try again
          {
          Serial.println("Senso4S-2. Reconnect failed");
          if(!pClient_Senso4s->connect(advDevice_Senso4s, false))     // Try again
            {
            Serial.println("Senso4S-3. Reconnect failed");
            return false;
            }
          }
        }
      Serial.println("Reconnected client");
      }
      /** We don't already have a client that knows this device,
       *  we will check for a client that is disconnected that we can use.
      */
    else 
      {
      pClient_Senso4s = NimBLEDevice::getDisconnectedClient();
      }
    }

    /** No client to reuse? Create a new one. */
    if(!pClient_Senso4s)
      {
      if(NimBLEDevice::getCreatedClientCount() >= NIMBLE_MAX_CONNECTIONS) 
        {
        Serial.println("Max clients reached - no more connections available");
        return false;
        }

      pClient_Senso4s = NimBLEDevice::createClient();

      Serial.println("New client created");

      pClient_Senso4s->setClientCallbacks(&clientCB, false);
      /** Set initial connection parameters: These settings are 15ms interval, 0 latency, 120ms timout.
       *  These settings are safe for 3 clients to connect reliably, can go faster if you have less
       *  connections. Timeout should be a multiple of the interval, minimum is 100ms.
       *  Min interval: 12 * 1.25ms = 15, Max interval: 12 * 1.25ms = 15, 0 latency, 51 * 10ms = 510ms timeout
      */
      pClient_Senso4s->setConnectionParams(80,160,0,400);   // Exactly what the Device will request

      /** Set how long we are willing to wait for the connection to complete (milliseconds), default is 30000. */
      pClient_Senso4s->setConnectTimeout(5 * 1000);


      if(!pClient_Senso4s->connect(advDevice_Senso4s)) // Created a client but failed to connect, don't need to keep it as it has no data
        {
        Serial.println("Senso4s-1. Connect failed");
        if(!pClient_Senso4s->connect(advDevice_Senso4s))  // Try again
          {
          NimBLEDevice::deleteClient(pClient_Senso4s);
          Serial.println("Senso4s-2. Connect failed, deleted client");
          return false;
          }
        }
      }

    if(!pClient_Senso4s->isConnected()) 
      {
      if (!pClient_Senso4s->connect(advDevice_Senso4s))
        {
        Serial.println("Senso4s-1. Connect failed");
        if (!pClient_Senso4s->connect(advDevice_Senso4s))   // Try again
          {
          Serial.println("Senso4s-2. Connect failed");
          return false;
          }
        }
      }
      Serial.print("Connected to: ");
      Serial.println(pClient_Senso4s->getPeerAddress().toString().c_str());
      Serial.print("RSSI: ");
      Serial.println(pClient_Senso4s->getRssi());

    /** Now we can read/write/subscribe the charateristics of the services we are interested in */
      pSvc_Senso4s = pClient_Senso4s->getService("00007081-A20B-4D4D-A4DE-7F071DBBC1D8");   // SENSOS4S
      if(pSvc_Senso4s)     // make sure it's not null
        {
        pChr_Senso4s = pSvc_Senso4s->getCharacteristic("00007082-A20B-4D4D-A4DE-7F071DBBC1D8"); // Filling Level in %

        if(pChr_Senso4s) 
          {     // make sure it's not null 
/*          if(pChr_Senso4s->canRead()) 
            {
            Serial.print(pChr_Senso4s->getUUID().toString().c_str());
            Serial.print(" Value: ");
            Serial.println(pChr_Senso4s->readValue().c_str());  //println ASCII

            std::string rxValue = pChr_Senso4s->readValue().c_str();    //c_Str = null terminated
	          if (rxValue.length() > 0)
              {
		          Serial.println(rxValue.length(),DEC);
              Serial.println("*********");
		          Serial.print("canRead received Value: ");
		          for (int i = 0; i < rxValue.length(); i++)
		            {
                Serial.print(rxValue[i],DEC);
                Gas_Proz=rxValue[i];
                Serial.println("\n*********");
                Serial.print("Gas_Proz=");
                Serial.println(Gas_Proz);
                //int value = std::stoi(record[i]);
                }
              Serial.println("--------------")
              }
            }
*/
          /** registerForNotify() has been removed and replaced with subscribe() / unsubscribe().
          *  Subscribe parameter defaults are: notifications=true, notifyCallback=nullptr.
          *  Unsubscribe takes no parameters.
          */
          if(pChr_Senso4s->canNotify())
            {
            if(!pChr_Senso4s->subscribe(true, notifyCB_Senso4s))
              {
              // Disconnect if subscribe failed 
              pClient_Senso4s->disconnect();
              return false;
              }
            }
          else
            {
            if(pChr_Senso4s->canIndicate()) 
              {
              // Send false as first argument to subscribe to indications instead of notifications 
              if(!pChr_Senso4s->subscribe(false, notifyCB_Senso4s)) 
                {
                // Disconnect if subscribe failed 
                pClient_Senso4s->disconnect();
                return false;
                }
              }
            }

          }
        } 
    else 
      {
      Serial.println("Service 7081 SENSO4S service not found.");
      }

    Serial.println("Done with this device!");
    return true;
}

void setup()
{
    Serial.begin(115200);
    NimBLEDevice::init("");
    pBLEScan = NimBLEDevice::getScan(); // Create the scan object.
    pBLEScan->setScanCallbacks(&scanCallbacks, false); // Set the callback for when devices are discovered, no duplicates.
    pBLEScan->setActiveScan(true);          // Set active scanning, this will get more data from the advertiser.
    pBLEScan->setFilterPolicy(0);           // 1=BLE_HCI_SCAN_FILT_USE_WL
}

void loop()
{
  Serial.println("Scanning for BLE Devices ...");
  pBLEScan->start(10000, nullptr, false);       // 0 == scan until infinity 

  while(pBLEScan->isScanning() == true)     // Scanned x Sec = Blocking
    {if(doConnect_Senso4s == true)          // Device found-> stop scan
	    {pBLEScan->stop();
      Serial.println("Senso4s found");}
    }
  Serial.println("Scan finished");
  pBLEScan->stop();
  delay(100);              // wait 100ms, so scan can finish and clean up

  if(doConnect_Senso4s) // MAC+Service found
    {doConnect_Senso4s = false;                  // Reset for next loop
    Serial.println("Connect to Senso4s and enable notifications");
    if(connectToServer_Senso4s()) 
      {
      Serial.println("Success! we should now be getting Senso4s notifications!");
      }
    }

  while(pClient_Senso4s->isConnected())     // wait until disconnected
    {delay(10);}

  Serial.println("Wait 20 sec befor connecting, again"); 
  delay(20000);

}
