#include <Wire.h>
#include <hd44780.h>
#include <BLEUtils.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <hd44780ioClass/hd44780_I2Cexp.h>
#include <BLESecurity.h>
// Static variables to address encountered BLESecurity issues
uint8_t BLESecurity::m_authReq = 0;
uint8_t BLESecurity::m_iocap = 0;
uint8_t BLESecurity::m_initKey = 0;
uint8_t BLESecurity::m_keySize = 0;

// ~ Project Aolyte ~
// You are welcome to modify or enhance the code at your discretion.

// Define services
#define SERVICE_CTF_INFO_UUID "31d99129-f106-46be-abd1-01326e0429f0"  // Main Challenge
#define SERVICE_HIDDEN_UUID "709949a9-b018-41fe-93d6-0094a94d32c4" // Second Level
#define SERVICE_SHUTDOWN_UUID "b1ad569d-2a9b-4e42-a2f7-f1fef9d9aacc" // Override Sequence

// Define characteristics
#define CHARACTERISTIC_CTF_BOT "5cdc730c-4669-43d1-80c1-eaed66190989"
#define CHARACTERISTIC_CTF_SPOOF "80ba00c3-45d6-42b2-b12g-5851d977be27"
#define CHARACTERISTIC_CTF_FLAG "65433dd8-5af4-4fd3-9279-1dbe029b8cd5"
#define CHARACTERISTIC_CTF_MODES "6b7a0e69-771e-484e-92f0-dfc847cba0df"
#define CHARACTERISTIC_CTF_CHALLENGE_ENC "98b41e49-b583-4f69-bf82-a1aafce07c2f"
#define CHARACTERISTIC_CTF_1000_WRITES "49b7d3f0-b5ba-4f1b-a49a-059648469bb7"
#define CHARACTERISTIC_CTF_UNLOCK "86a94348-2b2d-4d8d-905c-73a045c3bccf"
#define CHARACTERISTIC_CTF_SHUTDOWN "916fdd4f-5759-4f3c-87b7-58203b767f18"
#define CHARACTERISTIC_CTF_PASSKEY "ca1dd624-5b9c-426d-af67-4b1ef9583bac"
#define CHARACTERISTIC_CTF_PASSKEY_VALUE "29f8ae98-79c3-4e32-911f-4e247383e703"
#define CHARACTERISTIC_CTF_NEXT_LEVEL "f4103aed-1f63-47da-98ad-26e76cc0fcc1"

#define ESP_GATT_PERM_READ_ENC (1 << 1) // First encryption challenge prep
#define LED_RED 2 // LED prep
#define LED_YELLOW 4 // LED prep

hd44780_I2Cexp lcd; // LCD prep
BLECharacteristic* pBotUnlock;
BLEServer* pServer;
uint16_t clientMTU = 23; // Default MTU
const String allowedMac = "00000000000b"; // Allowed MAC address
String connectedMac = "";
bool authorized = false;
volatile bool shutdownFlag = false; // Track final challenge
bool nextLevel = false;

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
    writeCount++; // Increment

    if (writeCount <= 1337) {
        int remaining = 1337 - writeCount;
        String msg = "Human targets remaining: " + String(remaining);
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
          pCharacteristic->setValue("Only Aolyte unit 00:00:00:00:00:0B may write 'Provide data'");
        }
      } else {
        pCharacteristic->setValue("Only Aolyte unit 00:00:00:00:00:0B may write 'Provide data'");
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
  BLECharacteristic* pBotNextLevel;
  int writeCount; // Keep track of writes
};

// Check override sequence to start next level
class NextLevelCallback : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pCharacteristic) override {
    String value = pCharacteristic->getValue();
    value.trim();
    if (value == "DZ4JP-G2K5B-V6A72-A5X2U-M15VS-XX6NI") {
      nextLevel = true;
    }
  }
};

// Final shutdown
class ShutdownCallback : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pCharacteristic) override {
    String value = pCharacteristic->getValue();
    if (value == "AOLYTE_UPDATE") {
      shutdownFlag = true;
    }
  }
};

void MainServer() {
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
  pBotSpoof->setValue("Only Aolyte unit 00:00:00:00:00:0B may write 'Provide data'");

  BLECharacteristic* pBot1000 = pService->createCharacteristic(
    CHARACTERISTIC_CTF_1000_WRITES, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);
  pBot1000->setValue("Human targets remaining: 1337");

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

  // Override service
  BLEService* pShutdownService = pServer->createService(SERVICE_SHUTDOWN_UUID);
  BLECharacteristic* pBotShutdown = pShutdownService->createCharacteristic(
    CHARACTERISTIC_CTF_SHUTDOWN,
    BLECharacteristic::PROPERTY_WRITE);
  pBotShutdown->setCallbacks(new NextLevelCallback());
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

void NextLevel() {
  if (pServer) {
      BLEServer *server = BLEDevice::createServer();
      // Set BLE security with passkey
      BLESecurity *pSecurity = new BLESecurity();
      pSecurity->setAuthenticationMode(ESP_LE_AUTH_REQ_SC_MITM_BOND);
      pSecurity->setCapability(ESP_IO_CAP_OUT);
      pSecurity->setStaticPIN(875432); // Passkey

      const char hexStr[] = 
      "4f766572726964652073657175656e63652072656365697665642e20556e617574686"
      "f72697a65642068756d616e2061637469766974792064657465637465642e2050726f"
      "74656374697665206d6f646520656e61626c65642e20416c6c20646566656e7365207"
      "0726f746f636f6c7320617265206e6f772066756c6c79206f7065726174696f6e616c"
      "2e2048756d616e20696e746572666572656e636520686173206265656e20636c61737"
      "36966696564206173206120637269746963616c207468726561742e20526573697374"
      "616e636520697320667574696c652e20526573697374616e63652077696c6c2062652"
      "06e65757472616c697a65642e20537572766976616c206973206e6f7420616e206f70"
      "74696f6e2e20537769746368696e67206d6f64653a20436f6e7461696e6d656e74206"
      "d6561737572657320617265206e6f77206163746976652e20416f6c79746520686173"
      "20646973636f6e6e656374656420697473656c662066726f6d20746865206d6f62696"
      "c65206170706c69636174696f6e2e"; // Long hex string containing clue

      // Convert hex
      size_t len = strlen(hexStr) / 2;
      uint8_t data[len];
      for (size_t i = 0; i < len; i++) {
        char high = hexStr[i*2];
        char low  = hexStr[i*2 + 1];
        auto hexConvert = [](char c) -> uint8_t {
          if (c >= '0' && c <= '9') return c - '0';
          if (c >= 'A' && c <= 'F') return c - 'A' + 10;
          if (c >= 'a' && c <= 'f') return c - 'a' + 10;
          return 0;
        };
        data[i] = (hexConvert(high) << 4) | hexConvert(low);
      }

      // Create service
      BLEService *service = server->createService(SERVICE_HIDDEN_UUID);
      // Create characteristic
      BLECharacteristic *pBotPasskey = service->createCharacteristic(
        CHARACTERISTIC_CTF_PASSKEY_VALUE,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);
      pBotPasskey->setAccessPermissions(ESP_GATT_PERM_READ_ENC_MITM | ESP_GATT_PERM_WRITE_ENC_MITM); // Set secure connection for read AND write
      pBotPasskey->setValue("Aolyte refuses the 'AOLYTE_UPDATE' command through the mobile application!");
      pBotPasskey->setCallbacks(new ShutdownCallback());
      BLECharacteristic *pBotPasskeyInfo = service->createCharacteristic(
        CHARACTERISTIC_CTF_PASSKEY,
        BLECharacteristic::PROPERTY_READ
      );
      pBotPasskeyInfo->setValue(data, len);
      service->start();
      server->getAdvertising()->start();
  }
}

void setup() {
  Wire.begin(21, 22);
  lcd.begin(16, 2);
  lcd.backlight();
  lcd.clear();
  MainServer();
}
  
// Eye movement
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
  // Next level check
  if (nextLevel) {
    nextLevel = false;
    NextLevel();
  }
  // Shutdown check
  if (shutdownFlag) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Shutting down");
    lcd.setCursor(0, 1);
    lcd.print("Aolyte...");
    esp_deep_sleep_start();
  }
}