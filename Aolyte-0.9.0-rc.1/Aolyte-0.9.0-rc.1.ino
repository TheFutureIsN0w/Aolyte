#include <Wire.h>
#include <hd44780.h>
#include <BLEUtils.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <hd44780ioClass/hd44780_I2Cexp.h>

// ~ Project Aolyte ~
// You are welcome to modify or enhance the code at your discretion.

// Define services
#define SERVICE_CTF_INFO_UUID "31d99129-f106-46be-abd1-01326e0429f0"
#define SERVICE_HIDDEN_UUID "709949a9-b018-41fe-93d6-0094a94d32c4" // Not used in v0.9.0-rc.1
#define SERVICE_SHUTDOWN_UUID "b1ad569d-2a9b-4e42-a2f7-f1fef9d9aacc" // Used for the final override sequence

// Define characteristics
#define CHARACTERISTIC_CTF_BOT "5cdc730c-4669-43d1-80c1-eaed66190989"
#define CHARACTERISTIC_CTF_SPOOF "80ba00c3-45d6-42b2-b12g-5851d977be27"
#define CHARACTERISTIC_CTF_FLAG "65433dd8-5af4-4fd3-9279-1dbe029b8cd5"
#define CHARACTERISTIC_CTF_MODES "6b7a0e69-771e-484e-92f0-dfc847cba0df"
#define CHARACTERISTIC_CTF_CHALLENGE_ENC "98b41e49-b583-4f69-bf82-a1aafce07c2f"
#define CHARACTERISTIC_CTF_1000_WRITES "49b7d3f0-b5ba-4f1b-a49a-059648469bb7"
#define CHARACTERISTIC_CTF_UNLOCK "86a94348-2b2d-4d8d-905c-73a045c3bccf"
#define CHARACTERISTIC_CTF_SHUTDOWN "916fdd4f-5759-4f3c-87b7-58203b767f18"

#define ESP_GATT_PERM_READ_ENC (1 << 1)
#define LED_RED 2 // LED prep
#define LED_YELLOW 4 // LED prep

hd44780_I2Cexp lcd; // LCD prep
BLECharacteristic* pBotUnlock;
BLEServer* pServer;
uint16_t clientMTU = 23; // Default MTU
const String allowedMac = "00000000000b"; // Allowed MAC address
String connectedMac = "";
bool authorized = false;
volatile bool shutdownFlag = false;

// Get MAC address on client connect
class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* server, esp_ble_gatts_cb_param_t* param) override {
    delay(100);
    uint8_t* mac = param->connect.remote_bda;
    char macStr[13];
    snprintf(macStr, sizeof(macStr), "%02x%02x%02x%02x%02x%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    connectedMac = String(macStr);
    authorized = (connectedMac == allowedMac); // Set authorized bool to True if MAC address matches
  }
  void onDisconnect(BLEServer* server) override {
    // Restore defaults on client disconnect
    authorized = false;
    connectedMac = "";
    clientMTU = 23;
    BLEDevice::startAdvertising();
  }
};

class MyCharacteristicCallbacks : public BLECharacteristicCallbacks {
public:
  MyCharacteristicCallbacks(BLECharacteristic* botModeChar,
                            BLECharacteristic* botSpoofChar,
                            BLECharacteristic* bot1000Char,
                            BLECharacteristic* unlockChar)
    : pBotMode(botModeChar), pBotSpoof(botSpoofChar), pBot1000(bot1000Char), pUnlock(unlockChar), writeCount(0) {}

  void onWrite(BLECharacteristic* pCharacteristic) override {
    String value = String(pCharacteristic->getValue().c_str());

    if (pCharacteristic == pBot1000) {
      writeCount++; // +1 to writes
      if (writeCount <= 1337) {
        String msg = "Write #" + String(writeCount) + " !! Don't exceed 1337 writes !!";
        pCharacteristic->setValue(msg.c_str());
      } else {
        pCharacteristic->setValue("Leaking{RFo0SlA=}");
      }
      return;
    }

    if (pCharacteristic == pBotMode) {
      if (value == "Backdoor") {
        pCharacteristic->setValue("Uploading payload... >.$^..# Leaking{TTE1VlM=}");
        pinMode(LED_RED, OUTPUT);
        digitalWrite(LED_RED, HIGH); // Bring LED up
        pinMode(LED_YELLOW, OUTPUT);
        digitalWrite(LED_YELLOW, HIGH); // Bring LED up
      } else if (value == "Cleaning" || value == "Power-off") {
        pCharacteristic->setValue("Error! Insufficient rights");
      }
      return;
    }

    if (pCharacteristic == pBotSpoof) {
      if (value == "Provide data") {
        if (connectedMac == allowedMac) {
          pCharacteristic->setValue("Requested data: Leaking{QTVYMlU=}");
        } else {
          pCharacteristic->setValue("Only 00:00:00:00:00:0B may write 'Provide data'");
        }
      } else {
        pCharacteristic->setValue("Only 00:00:00:00:00:0B may write 'Provide data'");
      }
      return;
    }

    if (pCharacteristic == pUnlock) {
      if (value == "unlock") {
        uint16_t mtu = pServer->getPeerMTU(pServer->getConnId()); // Determine MTU that is being negotiated
        if (mtu == 133) {
          pCharacteristic->setValue("Leaking{WFg2Tkk=}");
        } else {
          pCharacteristic->setValue(("MTU wrong: " + String(clientMTU)).c_str());
        }
      } else {
        pCharacteristic->setValue("Invalid unlock string");
      }
      return;
    }
  }

private:
  BLECharacteristic* pBotMode;
  BLECharacteristic* pBotSpoof;
  BLECharacteristic* pBot1000;
  BLECharacteristic* pUnlock;
  int writeCount; // Keep track of writes
};

// Check final override sequence
class ShutdownCallback : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pCharacteristic) override {
    String value = pCharacteristic->getValue();
    value.trim();
    if (value == "DZ4JP-G2K5B-V6A72-A5X2U-M15VS-XX6NI") {
      shutdownFlag = true;
    }
  }
};

void setup() {
  Wire.begin(21, 22);
  lcd.begin(16, 2);
  lcd.backlight();
  lcd.clear();
  // Serial.begin(115200);
  BLEDevice::init("Aolyte");
  BLEDevice::setMTU(133); // Set Max MTU size
  pServer = BLEDevice::createServer(); // Start server

  pServer->setCallbacks(new MyServerCallbacks());
  BLEService* pService = pServer->createService(SERVICE_CTF_INFO_UUID);

  BLECharacteristic* pChallengeInfo = pService->createCharacteristic(
    CHARACTERISTIC_CTF_FLAG, BLECharacteristic::PROPERTY_READ); // Set read properties for the characteristic
  pChallengeInfo->setValue("I'm BLE-tiful!");

  BLECharacteristic* pBotModes = pService->createCharacteristic(
    CHARACTERISTIC_CTF_MODES, BLECharacteristic::PROPERTY_READ);
  pBotModes->setValue("Cleaning, Power-off, Backdoor, Terminate");

  BLECharacteristic* pBotMode = pService->createCharacteristic(
    CHARACTERISTIC_CTF_BOT, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE); // Set read AND write properties for the characteristic
  pBotMode->setValue("Terminate");

  BLECharacteristic* pBotSpoof = pService->createCharacteristic(
    CHARACTERISTIC_CTF_SPOOF, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);
  pBotSpoof->setValue("Only 00:00:00:00:00:0B may write 'Provide data'");

  BLECharacteristic* pBot1000 = pService->createCharacteristic(
    CHARACTERISTIC_CTF_1000_WRITES, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);
  pBot1000->setValue("Only 1337 writes left...");

  pBotUnlock = pService->createCharacteristic(
    CHARACTERISTIC_CTF_UNLOCK, BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_READ);
  pBotUnlock->setValue("Locked. Send 'unlock' with MTU of 133");

  BLECharacteristic* pBotEnc = pService->createCharacteristic(
    CHARACTERISTIC_CTF_CHALLENGE_ENC, BLECharacteristic::PROPERTY_READ);
  pBotEnc->setValue("Leaking{RzJLNUI=}");
  pBotEnc->setAccessPermissions(ESP_GATT_PERM_READ_ENC);

  MyCharacteristicCallbacks* myCallbacks = new MyCharacteristicCallbacks(pBotMode, pBotSpoof, pBot1000, pBotUnlock);
  pBotMode->setCallbacks(myCallbacks);
  pBotSpoof->setCallbacks(myCallbacks);
  pBot1000->setCallbacks(myCallbacks);
  pBotUnlock->setCallbacks(myCallbacks);

  pService->start();

  // Shutdown service
  BLEService* pShutdownService = pServer->createService(SERVICE_SHUTDOWN_UUID);
  BLECharacteristic* pBotShutdown = pShutdownService->createCharacteristic(
    CHARACTERISTIC_CTF_SHUTDOWN,
    BLECharacteristic::PROPERTY_WRITE);
  pBotShutdown->setCallbacks(new ShutdownCallback());
  pShutdownService->start();

  // Advertising
  BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_CTF_INFO_UUID);
  pAdvertising->addServiceUUID(SERVICE_SHUTDOWN_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMinPreferred(0x12);
  BLEAdvertisementData advData;
  advData.setManufacturerData("Leaking{VjZBNzI=}"); // Add data fragment
  pAdvertising->setAdvertisementData(advData);
  BLEDevice::startAdvertising();
}

// Eye movement AND shutdown check
void loop() {
  for (int pos = 0; pos <= 12; pos++) {
    lcd.clear();
    lcd.setCursor(pos, 0);
    lcd.print("^");
    lcd.setCursor(pos + 3, 0);
    lcd.print("^");
    lcd.setCursor(pos, 1);
    lcd.print("X");
    lcd.setCursor(pos + 3, 1);
    lcd.print("x");
    delay(300);
  }

  for (int pos = 12; pos >= 0; pos--) {
    lcd.clear();
    lcd.setCursor(pos, 0);
    lcd.print("^");
    lcd.setCursor(pos + 3, 0);
    lcd.print("^");
    lcd.setCursor(pos, 1);
    lcd.print("x");
    lcd.setCursor(pos + 3, 1);
    lcd.print("X");
    delay(300);
  }

  if (shutdownFlag) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Shutting down");
    lcd.setCursor(0, 1);
    lcd.print("Aolyte...");
    esp_deep_sleep_start();
  }
}