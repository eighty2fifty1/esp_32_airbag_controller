#include <EEPROM.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <BLECharacteristic.h>

#define PRESSURE_SERVICE_UUID "e4a838e1-5307-424d-a288-e1c66365a215"
#define LEFT_PRESS_CHAR_UUID "64a4c480-1910-4059-a642-44515115ebc1"
#define RIGHT_PRESS_CHAR_UUID "384b9e03-ad97-4549-98a2-c4479318263c"

#define SETTING_SERVICE_UUID "b01d4b71-6794-415f-aee8-7edd21ce4eec"
#define LEFT_SETTING_CHAR_UUID "c017926e-77ec-4286-8f40-af49b0d63ff8"
#define RIGHT_SETTING_CHAR_UUID "27b67d88-8da0-4bc5-a689-6fe3c6cb9133"
#define MODE_CHAR_UUID "8bd8c597-f228-4f52-a6ee-194a76d0bd01"

#define STATUS_SERVICE_UUID "ef4462fb-996b-416b-b7db-d753f99a712b"
#define PUMP_CONT_UUID "ee0a9c1a-6326-4e12-9158-f95726fbf3f1"
#define LEFT_DUMP_UUID "99bb0d87-5285-4e9d-b892-ece4c6b4cc09"
#define LEFT_FILL_UUID "6bf9c462-afe7-4635-aed4-f3015ac1b285"
#define RIGHT_DUMP_UUID "c30450fa-45ad-4a16-96a4-d49dbb17e47c"
#define RIGHT_FILL_UUID "621d4344-380a-4f88-badd-584fee712f1b"

#define EEPROM_SIZE 2

using namespace std;

//EEPROM placeholders
int lPlace;
int rtPlace;
int eeAddr = 0;

//pin setup
int leftDump = 15;  //purple, relay 1
int relay = 2;  //blue, relay 2
int leftFill = 4; //green, relay 4
int rtFill = 16;  //yellow, relay 7
int rtDump = 17;  //orange, relay 8

int leftRaw;  //pin 32
int rtRaw;  //pin 33

//pressure variables
int leftSet = 5;
int rtSet = 5;
int leftPress, rtPress;
bool leftPumpNeeded;
bool rightPumpNeeded;
int sol;

//manual mode
int manualMode = 0;
int manualPump = 0;

//BLE characteristics
BLECharacteristic *leftPressChar;
BLECharacteristic *rightPressChar;
BLECharacteristic *leftSettingChar;
BLECharacteristic *rightSettingChar;
BLECharacteristic *modeSettingChar;

BLECharacteristic *pumpStatusChar;
BLECharacteristic *leftDumpChar;
BLECharacteristic *leftFillChar;
BLECharacteristic *rightDumpChar;
BLECharacteristic *rightFillChar;

bool deviceConnected = false;

//function prototypes
void saveSettings(int, int);

//BLE callbacks
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
    }
};

class CharacteristicCallback : public BLECharacteristicCallbacks {
    String l_setting, r_setting;

    void onWrite(BLECharacteristic *characteristic_) {
      Serial.println("characteristic changed");
      l_setting = leftSettingChar->getValue().c_str();
      r_setting = rightSettingChar->getValue().c_str();
      leftSet = l_setting.toInt();
      rtSet = r_setting.toInt();
      Serial.println(leftSet);
      Serial.println(rtSet);
      saveSettings(leftSet, rtSet);
    }
};

class ModeCharacteristicCallback : public BLECharacteristicCallbacks {
    String modeSetting;


    void onWrite(BLECharacteristic *characteristic_) {
      Serial.println("characteristic changed");
      modeSetting = characteristic_->getValue().c_str();
      int manualModeTest = modeSetting.toInt();

      if (manualModeTest == 1) {
        manualMode = 1;
      }
      else {
        manualMode = 0;
      }

    }
};

void setup() {
  //Bluetooth Setup
  BLEDevice::init("Airbag Controller");
  BLEServer *pServer = BLEDevice:: createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  //pressure service and chars
  BLEService *pressService = pServer->createService(PRESSURE_SERVICE_UUID);
  leftPressChar = pressService->createCharacteristic(LEFT_PRESS_CHAR_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY);
  rightPressChar = pressService->createCharacteristic(RIGHT_PRESS_CHAR_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY);

  //pressure setting service and chars
  BLEService *settingService = pServer->createService(SETTING_SERVICE_UUID);
  leftSettingChar = settingService->createCharacteristic(LEFT_SETTING_CHAR_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);
  rightSettingChar = settingService->createCharacteristic(RIGHT_SETTING_CHAR_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);
  modeSettingChar = settingService->createCharacteristic(MODE_CHAR_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);


  //status service and chars
  BLEService *statusService = pServer->createService(STATUS_SERVICE_UUID);
  pumpStatusChar = statusService->createCharacteristic(PUMP_CONT_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY);
  leftDumpChar = statusService->createCharacteristic(LEFT_DUMP_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY);
  leftFillChar = statusService->createCharacteristic(LEFT_FILL_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY);
  rightDumpChar = statusService->createCharacteristic(RIGHT_DUMP_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY);
  rightFillChar = statusService->createCharacteristic(RIGHT_FILL_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY);

  pumpStatusChar->addDescriptor(new BLE2902());
  leftDumpChar->addDescriptor(new BLE2902());
  leftFillChar->addDescriptor(new BLE2902());
  rightDumpChar->addDescriptor(new BLE2902());
  rightFillChar->addDescriptor(new BLE2902());
  leftPressChar->addDescriptor(new BLE2902());
  rightPressChar->addDescriptor(new BLE2902());


  leftSettingChar->setCallbacks(new CharacteristicCallback());
  rightSettingChar->setCallbacks(new CharacteristicCallback());
  modeSettingChar->setCallbacks(new ModeCharacteristicCallback());

  pressService->start();
  settingService->start();
  statusService->start();

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(PRESSURE_SERVICE_UUID);
  pAdvertising->addServiceUUID(SETTING_SERVICE_UUID);
  pAdvertising->addServiceUUID(STATUS_SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();

  //pin settings
  pinMode(relay, OUTPUT);
  pinMode(leftFill, OUTPUT);
  pinMode(rtFill, OUTPUT);
  pinMode(leftDump, OUTPUT);
  pinMode(rtDump, OUTPUT);

  //serial setup
  Serial.begin(115200);        //for debugging, won't be used in normal ops  //MySerial.begin(19200);

  //EEPROM setup
  EEPROM.begin(EEPROM_SIZE);
  lPlace = EEPROM.read(0);
  rtPlace = EEPROM.read(1);
  Serial.print("EEPROM values: ");
  Serial.print(lPlace);
  Serial.println(rtPlace);

  //input validation of eeprom values
  if ((lPlace > 5) && (lPlace < 90)) {
    leftSet = lPlace;
  }

  if ((rtPlace > 5) && (rtPlace < 90)) {
    rtSet = rtPlace;
  }
}

void loop() {
  //pressure read
  leftRaw = analogRead(32);
  rtRaw = analogRead(33);
  Serial.println(leftRaw);
  Serial.println(rtRaw);
  leftPress = pressureConvert(leftRaw);
  rtPress = pressureConvert(rtRaw);
  Serial.println(leftPress);
  Serial.println(rtPress);
  leftPressChar->setValue(leftPress);
  rightPressChar->setValue(rtPress);
  leftPressChar->notify();
  delay(5);
  rightPressChar->notify();
  delay(5);
  if (manualMode == 0) {
    Serial.println("auto mode");
    //relays energize when LOW signal (grounded out)
    //fill left
    if (leftPress < (leftSet + 5)) {
      if (leftPress < (leftSet - 5)) {
        leftPumpNeeded = true;
        //automatic pressure control

        digitalWrite(leftFill, LOW);
        sol = 1;
        leftFillChar->setValue(sol);
        leftFillChar->notify();
        delay(5);
        digitalWrite(leftDump, HIGH);
        sol = 0;
        leftDumpChar->setValue(sol);
        leftDumpChar->notify();
        delay(5);
        Serial.println("Left airbag filling");
      }
    }
    //dump left
    else if (leftPress > (leftSet + 5)) {
      if (leftPress > (leftSet - 5)) {
        digitalWrite(leftDump, LOW);
        sol = 1;
        leftDumpChar->setValue(sol);
        leftDumpChar->notify();
        delay(5);
        digitalWrite(leftFill, HIGH);
        sol = 0;
        leftFillChar->setValue(sol);
        leftFillChar->notify();
        delay(5);
        leftPumpNeeded = false;
        Serial.println("Left airbag dumping");
      }
    }
    //idle
    else {
      digitalWrite(leftDump, HIGH);
      digitalWrite(leftFill, HIGH);
      sol = 0;
      leftDumpChar->setValue(sol);
      leftFillChar->setValue(sol);
      leftDumpChar->notify();
      delay(5);
      leftFillChar->notify();
      delay(5);
      leftPumpNeeded = false;
      Serial.println("left solenoids closed");
    }

    //fill right
    if (rtPress < (rtSet + 5)) {
      if (rtPress < (rtSet - 5)) {
        rightPumpNeeded = true;
        digitalWrite(rtFill, LOW);
        sol = 1;
        rightFillChar->setValue(sol);
        rightFillChar->notify();
        delay(5);
        digitalWrite(rtDump, HIGH);
        sol = 0;
        rightDumpChar->setValue(sol);
        rightDumpChar->notify();
        delay(5);
        Serial.println("Right airbag filling");
      }
    }
    //dump right
    else if (rtPress > (rtSet + 5)) {
      if (rtPress > (rtSet - 5)) {
        digitalWrite(rtDump, LOW);
        sol = 1;
        rightDumpChar->setValue(sol);
        rightDumpChar->notify();
        delay(5);
        digitalWrite(rtFill, HIGH);
        sol = 0;
        rightFillChar->setValue(sol);
        rightFillChar->notify();
        delay(5);
        rightPumpNeeded = false;
        Serial.println("Right airbag dumping");
      }
    }

    //idle
    else {
      digitalWrite(rtDump, HIGH);
      digitalWrite(rtFill, HIGH);
      sol = 0;
      rightFillChar->setValue(sol);
      rightDumpChar->setValue(sol);
      rightFillChar->notify();
      delay(5);
      rightDumpChar->notify();
      delay(5);
      rightPumpNeeded = false;
      Serial.println("right solenoids closed");
    }
    //turn off pump
    pumpNeeded(leftPumpNeeded, rightPumpNeeded);
  }

  //manual pump control for filling tires
  else if (manualMode == 1) {
    Serial.println("manual mode");
    //closes all solenoids
    digitalWrite(leftFill, HIGH);
    digitalWrite(leftDump, HIGH);
    digitalWrite(rtFill, HIGH);
    digitalWrite(rtDump, HIGH);

    //pump on
    if (manualPump == 1) {
      digitalWrite(relay, LOW);
      pumpStatusChar->setValue(manualPump);
      pumpStatusChar->notify();
      delay(5);      
    }
    else {
      digitalWrite(relay, HIGH);
      pumpStatusChar->setValue(manualPump);
      pumpStatusChar->notify();
      delay(5);
    }
  }
  delay(200);
}
///////////////////////////////////////////////
//                 FUNCTIONS                 //
///////////////////////////////////////////////

//converts analog signal to psi.  requires testing
float pressureConvert(float in) {
  /*
    float psi;
    psi = (in - 409) * (200/10000);
    return psi;
  */
  int pressureValue;

  const int pressureZero = 360; //analog reading of pressure transducer at 0psi -- this is 360/4096 counts; therefore, ~0.09V at 0PSI
  const int pressureMax = 4096; //analog reading of pressure transducer at 100psi -- therefore 4096/4096 = 3.3v and 100PSI
  const int pressuretransducermaxPSI = 200; //psi value of transducer being used - 0-100PSI
  //float pressureValue1 = 0; // used for Arduino analogRead

  if (in > 4096) {
    in = 4096;
  }

  if (in < 360) {
    in = 360;
  }

  pressureValue = ((in - pressureZero) * pressuretransducermaxPSI) / (pressureMax - pressureZero); //conversion equation to convert analog reading to psi
  return pressureValue;
}


//===========

void saveSettings(int leftSetting, int rightSetting) {
  EEPROM.write(0, leftSetting);
  EEPROM.write(1, rightSetting);
  EEPROM.commit();
  Serial.println("Saved in memory");
}

void pumpNeeded(bool left, bool right) {
  int pumpSol;
  if (left || right) {
    digitalWrite(relay, LOW);
    pumpSol = 1;
  }

  else {
    digitalWrite(relay, HIGH);
    pumpSol = 0;
  }
  pumpStatusChar->setValue(pumpSol);
  pumpStatusChar->notify();
  delay(5);
}
