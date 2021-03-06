/**
 *   ___  ___ ___ | |_| |_ ______ _  ___| |__ / |
 *  / __|/ __/ _ \| __| __|_  / _` |/ __| '_ \| |
 *  \__ \ (_| (_) | |_| |_ / / (_| | (__| | | | |
 *  |___/\___\___/ \__|\__/___\__,_|\___|_| |_|_|
 *
 *       Zac Scott (github.com/scottzach1)
 *
 * M5StackTemperature - BLE Server for Temperature Sensor
 */
#include <BLE2902.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <M5Stack.h>

#include "debug.h"

/**
 * BLE Related stuff
 * 
 * <https://btprodspecificationrefs.blob.core.windows.net/assigned-values/16-bit%20UUID%20Numbers%20Document.pdf>
 */
static BLEUUID serviceUUID = BLEUUID("224c9411-d6cb-4b2e-b4cb-ab687eb7de23");
static BLEUUID tempCharacteristicUUID = ((uint16_t)0x2A6E);

BLEDescriptor tempDescriptor(BLEUUID((uint16_t)0x2901));
BLECharacteristic tempCharacteristic(tempCharacteristicUUID, BLECharacteristic::PROPERTY_READ);

BLEServer *pServer = NULL;
BLEService *pService = NULL;

bool deviceConnected = false;

/**
 * Duty Cycling Timeouts
 */
const int DUTY_CYCLE_AWAKE = 2;  // seconds awake
const int DUTY_CYCLE_SLEEP = 2;  // seconds asleep
const int ACTIVITY_TIMEOUT = 8;  // seconds after BLE activity

/**
 * Safe memory (persistent through deepSleeps).
 */
RTC_DATA_ATTR time_t timestamp = 0;
RTC_DATA_ATTR time_t sleepTarget = 0;
RTC_DATA_ATTR bool dutyCycle = false;

// Safe previous reading buffers for persistent readings.
RTC_DATA_ATTR int8_t curTemp = 0;

void prolongSleep(int seconds) {
    time(&sleepTarget);
    sleepTarget += seconds;
}

/**
 * Callbacks for when we connect/disconnect from client.
 */
class MyServerCallbacks : public BLEServerCallbacks {
    /**
     * Upon connection prolong activity timeout and remember connected state.
     */
    void onConnect(BLEServer *pServer) {
        prolongSleep(ACTIVITY_TIMEOUT);
        DEBUG_MSG_LN(2, "client connected");
        deviceConnected = true;
    };

    /**
     * Upon disconnection restart the server advertising and update connected state.
     */
    void onDisconnect(BLEServer *pServer) {
        DEBUG_MSG_LN(2, "client disconnected");
        deviceConnected = false;
        pServer->startAdvertising();
    }
};

/**
 * Generate a random temperature within boundaries, then update and return temp buffer address.
 */
int8_t *updateRandTemp() {
    prolongSleep(ACTIVITY_TIMEOUT);
    curTemp = (int8_t)(rand() % 40) - 10;
    DEBUG_MSG_LN(2, curTemp);
    return &curTemp;
}

/**
 * Callback invoked when the Temp charactersitic is read.
 */
class TempCallBacks : public BLECharacteristicCallbacks {
    /**
     * Generate random temperature and respond to client.
     */
    void onRead(BLECharacteristic *pCharacteristic) {
        pCharacteristic->setValue((uint8_t *)updateRandTemp(), 2);
    }
};

/**
 * Configures the critical sensor node peripherals such as screen and BLE server.
 */
void setup() {
    // Initialize device
    Serial.begin(115200);
    M5.begin();
    M5.Power.begin();
    if (DEBUG) {
        M5.Lcd.clear();
        M5.Lcd.setBrightness(75);
    }
    DEBUG_MSG_LN(1, "Temperature node starting...");

    // Create BLE server with callbacks.
    BLEDevice::init("m5-temperature-1");
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());
    pService = pServer->createService(serviceUUID);

    // Add descriptor values.
    tempDescriptor.setValue("Temp: [-10,40]??C");

    // Update characteristics with descriptors.
    tempCharacteristic.addDescriptor(&tempDescriptor);

    // Add callback handlers to characteristics.
    tempCharacteristic.setCallbacks(new TempCallBacks());

    // Display advertised UUIDs for debbugging.
    DEBUG_MSG_F(1, "- Serv-UUID: %s\n", serviceUUID.toString().c_str());
    DEBUG_MSG_F(1, "- Temp-UUID: %s\n", tempCharacteristic.getUUID().toString().c_str());

    // Add characteristics to service.
    pService->addCharacteristic(&tempCharacteristic);

    // Start service and begin advertising.
    pService->start();
    pServer->getAdvertising()->addServiceUUID(serviceUUID);
    pServer->startAdvertising();

    prolongSleep(DUTY_CYCLE_AWAKE);
}

/**
 * Clears the display and resets cursor position.
 */
void clearDisplay() {
    M5.Lcd.clear(BLACK);
    M5.Lcd.setCursor(0, 0);
}

/**
 * Toggles duty cycle, notifying Lcd and updating activity timeout.
 */
void toggleDutyCycle() {
    dutyCycle = !dutyCycle;
    DEBUG_MSG_LN(1, "SET DUTY_CYCLE " + String(dutyCycle));
    prolongSleep(DUTY_CYCLE_AWAKE);
}

/**
 * Main event loop, listens for buttons and checks duty cycle based on timestamps.
 */
void loop() {
    M5.update();

    // Handle button presses.
    if (M5.BtnA.wasReleasefor(5)) clearDisplay();
    if (M5.BtnB.wasReleasefor(5)) toggleDutyCycle();
    if (M5.BtnC.wasReleasefor(5)) M5.Power.reset();

    time(&timestamp);
    // Trigger duty cycle sleep only after threshold.
    if (dutyCycle && timestamp > sleepTarget) {
        M5.Power.deepSleep(SLEEP_SEC(DUTY_CYCLE_SLEEP));
    }
}
