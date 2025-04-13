// ESP32C3FN4 SuperMini Board
// ===============================================================
// Arduino IDE settings:
//   - Board: ESP32C3 Dev Module
//   - ESP CDC On Boot: Enabled
//   - CPU Frequency: 80MHz (WiFi)
//   - Core Debug Level: None
//   - Erase All Flash Before Sketch Upload: Disabled
//   - Flash frequency: 80Mhz
//   - Flash Mode: QIO
//   - Flash Size: 4MB (32Mb)
//   - JTAG Adapter: Disabled
//   - Partition Scheme: Huge APP (3MB No OTA/1MB SPIFFS)
//   - Upload Speed: 921600
//   - Zigbee Mode: Disabled
//   - Programmer: Esptool
// ===============================================================

#include <HomeSpan.h>


/**************************************************************************************/
/*                                        Pins                                        */

// HomeSpan status LED pin.
#define STATUS_LED_PIN                  GPIO_NUM_8
// HomeSpan control button pin.
#define CONTROL_PIN                     GPIO_NUM_9

// The doorbell button input pin (radio signal)
#define BELL_BUTTON_PIN                 GPIO_NUM_3
// The doorbell signal pin (wired to the sound chip)
#define BELL_SIGNAL_PIN                 GPIO_NUM_10

// The doorbell button signal duration in milliseconds.
#define BELL_BUTTON_SIGNAL_DURATION     500
// The doorbell play signal duration in milliseconds.
#define BELL_SIGNAL_DURATION            250

/**************************************************************************************/


/**************************************************************************************/
/*                                  Global variables                                  */

// The doorbell state. True if the doorbell is enabled and should play a sound.
// False if the doorbell is disabled and should not play any sound.
bool _DoorbellEnabled = false;
// Indicates when the ring button was pressed.
volatile bool _Ring = false;

/**************************************************************************************/


/**************************************************************************************/
/*                              Doorbell signal interrup                              */

void IRAM_ATTR RingInterrupt()
{
    static bool WasHigh = false;
    static uint32_t LastMillis = 0;

    uint32_t Level = digitalRead(BELL_BUTTON_PIN);
    if (Level == HIGH && !WasHigh)
    {
        WasHigh = true;
        LastMillis = millis();
        return;
    }

    if (Level == LOW && WasHigh)
    {
        WasHigh = false;

        uint32_t CurrentMillis = millis();
        if (CurrentMillis - LastMillis >= BELL_BUTTON_SIGNAL_DURATION)
            _Ring = true;
        return;
    }
}
/**************************************************************************************/


/**************************************************************************************/
/*                               Virtual Door Bell switch                             */

struct DoorbellSwitch : Service::Switch
{
    SpanCharacteristic* Power;
    
    DoorbellSwitch() : Service::Switch()
    {
        // Default is false (bell is turned off) and we store current value in NVS.
        Power = new Characteristic::On(false, true);
        // Get current state.
        _DoorbellEnabled = Power->getVal();
    }
    
    bool update()
    {
        _DoorbellEnabled = Power->getNewVal();
        return true;
    }
};

/**************************************************************************************/


/**************************************************************************************/
/*                                   Door Bell service                                */

struct DoorBell : Service::Doorbell
{
    // Reference to the ProgrammableSwitchEvent Characteristic
    SpanCharacteristic* SwitchEvent;
    
    DoorBell() : Service::Doorbell()
    {
        // Programmable Switch Event Characteristic.
        SwitchEvent = new Characteristic::ProgrammableSwitchEvent();  
    }
};

DoorBell* _DoorBell = nullptr;
/**************************************************************************************/


// Arduino initialization routine.
void setup()
{
    // Initialize debug serial port.
    Serial.begin(115200);
    
    // Initialize pins door bell pins.
    pinMode(BELL_SIGNAL_PIN, OUTPUT);
    pinMode(BELL_BUTTON_PIN, INPUT_PULLDOWN);
    digitalWrite(BELL_SIGNAL_PIN, LOW);

    // Initialize HomeSpan pins
    pinMode(CONTROL_PIN, INPUT);
    pinMode(STATUS_LED_PIN, OUTPUT);
    digitalWrite(STATUS_LED_PIN, HIGH);

    // Initialize HomeSpan.
    homeSpan.setControlPin(CONTROL_PIN);
    homeSpan.setStatusPin(STATUS_LED_PIN);
    homeSpan.setPairingCode("63005612");
    homeSpan.begin(Category::Other, "DroneTales Doorbell");

    // Build device's serial number.
    char Sn[24];
    snprintf(Sn, 24, "DRONETALES-%llX", ESP.getEfuseMac());

    // Configure doorbell.
    new SpanAccessory();
    new Service::AccessoryInformation();
    new Characteristic::Identify();
    new Characteristic::Manufacturer("DroneTales");
    new Characteristic::SerialNumber(Sn);
    new Characteristic::Model("DroneTales Doorbell");
    new Characteristic::FirmwareRevision("1.0.0.0");
    new Characteristic::Name("Doorbell");
    _DoorBell = new DoorBell();

    // Configure doorbell switch.
    new SpanAccessory();
    new Service::AccessoryInformation();
    new Characteristic::Identify();
    new Characteristic::Manufacturer("DroneTales");
    new Characteristic::SerialNumber(Sn);
    new Characteristic::Model("DroneTales Doorbell Switch");
    new Characteristic::FirmwareRevision("1.0.0.0");
    new DoorbellSwitch();

    // Attached the interrupt to the ring signal pin.
    attachInterrupt(BELL_BUTTON_PIN, RingInterrupt, CHANGE);
}

// Arduino main loop.
void loop()
{
    if (_Ring)
    {
        _Ring = false;

        if (_DoorbellEnabled)
        {
            _DoorBell->SwitchEvent->setVal(SpanButton::SINGLE);
            
            digitalWrite(BELL_SIGNAL_PIN, HIGH);
            delay(BELL_SIGNAL_DURATION);
            digitalWrite(BELL_SIGNAL_PIN, LOW);
        }
    }

    homeSpan.poll();
}