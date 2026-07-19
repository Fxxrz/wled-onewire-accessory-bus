#include "wled.h"
#include <OneWire.h>
#include "onewire_accessory_protocol.h"

#ifndef ONEWIRE_ACCESSORY_PIN
#define ONEWIRE_ACCESSORY_PIN -1
#endif

#ifndef ONEWIRE_ACCESSORY_FORCE_PIN
#define ONEWIRE_ACCESSORY_FORCE_PIN ONEWIRE_ACCESSORY_PIN
#endif

#ifndef ONEWIRE_ACCESSORY_SERIAL_DEBUG
#define ONEWIRE_ACCESSORY_SERIAL_DEBUG 0
#endif

#ifndef ONEWIRE_ACCESSORY_INTERNAL_PULLUP
#define ONEWIRE_ACCESSORY_INTERNAL_PULLUP 0
#endif

class OneWireAccessoryBusUsermod : public Usermod {
private:
  static const char PAGE_ONEWIRE[];

  static constexpr uint8_t MAX_DEVICES = 8;
  static constexpr uint8_t MAX_PROFILES = 8;
  static constexpr uint8_t MAX_DESCRIPTOR_BYTES = 96;
  static constexpr uint8_t ONEWIRE_ACCESSORY_FAMILY = 0x7A;
  static constexpr uint8_t CMD_READ_DESCRIPTOR = 0xA1;
  static constexpr uint8_t CMD_READ_TELEMETRY = 0xB1;
  static constexpr uint16_t DEFAULT_SCAN_INTERVAL_MS = 3000;
  static constexpr uint16_t DEFAULT_MQTT_INTERVAL_MS = 30000;
  static constexpr uint16_t SERIAL_DEBUG_INTERVAL_MS = 30000;
  static constexpr uint32_t DEFAULT_DESCRIPTOR_INTERVAL_MS = 60000;
  static constexpr uint32_t DESCRIPTOR_RETRY_INTERVAL_MS = 10000;
  static constexpr uint32_t DESCRIPTOR_STALE_MS = 300000;
  static constexpr uint32_t DEFAULT_TELEMETRY_INTERVAL_MS = 10000;
  static constexpr uint32_t DEFAULT_TELEMETRY_STALE_MS = 60000;
  static constexpr uint32_t SMART_PROFILE_WRITE_INTERVAL_MS = 300000;
  static constexpr uint8_t SMART_DESCRIPTOR_CONFIRMATIONS = 3;
  static constexpr uint16_t SMART_SERVICE_INTERVAL_MS = 250;
  static constexpr uint16_t SMART_RESPONSE_DELAY_US = 1000;
  static constexpr uint8_t SMART_READY_RETRIES = 5;
  static constexpr uint8_t SMART_BUSY_BYTE = 0xFF;
  static constexpr uint16_t MANUAL_SCAN_RATE_LIMIT_MS = 1000;
  static constexpr uint8_t MAX_SEARCH_ITERATIONS = MAX_DEVICES * 4 + 8;
#ifdef ARDUINO_ARCH_ESP32
  static constexpr uint16_t WIRE_WORKER_STACK_BYTES = 4096;
  static constexpr TickType_t STATE_LOCK_WAIT = portMAX_DELAY;
#else
  static constexpr unsigned long STATE_LOCK_WAIT = 0;
#endif
  static constexpr uint16_t APPLY_RETRY_INTERVAL_MS = 250;
  static constexpr uint16_t APPLY_TIMEOUT_MS = 5000;
  static constexpr uint16_t APPLY_VERIFY_TIMEOUT_MS = 3000;
  static constexpr uint16_t APPLY_HEAP_MARGIN_BYTES = 8192;
  static constexpr uint8_t DEFAULT_STABLE_SCANS = 2;
  static constexpr uint16_t BUTTON_DEBOUNCE_MS = 50;
  static constexpr uint16_t BUTTON_LONG_PRESS_MS = 600;
  static constexpr uint16_t BUTTON_DOUBLE_PRESS_MS = 350;
  static constexpr uint16_t BUTTON_LONG_REPEAT_MS = 400;
  static constexpr uint16_t BUTTON_AP_MS = 5000;
  static constexpr uint16_t BUTTON_FACTORY_RESET_MS = 10000;

  enum DeviceKind : uint8_t {
    KIND_PASSIVE = 0,
    KIND_SMART_SHADE = 1,
    KIND_BATTERY = 2,
    KIND_SENSOR = 3,
    KIND_UNKNOWN = 255
  };

  enum InputMode : uint8_t {
    INPUT_INHERIT = 0,
    INPUT_DISABLED = 1,
    INPUT_BUTTON_LOW = 2,
    INPUT_BUTTON_HIGH = 3,
    INPUT_TOUCH = 4,
    INPUT_TOUCH_SWITCH = 5
  };

  enum BusHealth : uint8_t {
    BUS_UNKNOWN = 0,
    BUS_EMPTY_CLEAN,
    BUS_DEVICES,
    BUS_STUCK_LOW,
    BUS_PRESENCE_INVALID,
    BUS_CRC_ERROR,
    BUS_OVERFLOW,
    BUS_SEARCH_LIMIT
  };

  enum ApplyPhase : uint8_t {
    APPLY_IDLE = 0,
    APPLY_WAIT_TARGET,
    APPLY_WAIT_ROLLBACK
  };

  enum WireOperation : uint8_t {
    WIRE_NONE = 0,
    WIRE_SCAN,
    WIRE_READ_DESCRIPTOR,
    WIRE_READ_TELEMETRY
  };

  struct WireJob {
    WireOperation operation = WIRE_NONE;
    uint8_t generation = 0;
    int8_t pin = -1;
    bool internalPullup = false;
    uint8_t rom[8] = {0};
  };

  struct WireResult {
    WireOperation operation = WIRE_NONE;
    uint8_t generation = 0;
    bool success = false;
    bool presence = false;
    int8_t idleLevel = -1;
    uint8_t romCount = 0;
    bool overflow = false;
    bool iterationLimit = false;
    uint8_t crcErrors = 0;
    uint8_t roms[MAX_DEVICES][8] = {};
    uint8_t rom[8] = {0};
    uint8_t payloadLength = 0;
    uint8_t payload[MAX_DESCRIPTOR_BYTES] = {0};
    uint32_t durationUs = 0;
  };

  struct BatteryTelemetry {
    uint8_t validFields = 0;
    uint16_t millivolts = 0;
    uint8_t percent = 0;
    bool charging = false;
    int16_t temperatureDeciC = 0;
    uint16_t status = 0;
    unsigned long millivoltsUpdated = 0;
    unsigned long percentUpdated = 0;
    unsigned long chargingUpdated = 0;
    unsigned long temperatureUpdated = 0;
    unsigned long statusUpdated = 0;
  };

  struct Device {
    uint8_t rom[8] = {0};
    char romStr[17] = {0};
    uint8_t family = 0;
    DeviceKind kind = KIND_PASSIVE;
    uint16_t ledCount = 0;
    bool touch = true;
    char name[33] = {0};
    char hardwareVersion[17] = {0};
    char firmwareVersion[17] = {0};
    bool descriptorValid = false;
    bool telemetryFramed = false;
    uint8_t telemetrySequence = 0;
    unsigned long descriptorLastAttempt = 0;
    unsigned long descriptorLastSuccess = 0;
    unsigned long telemetryLastAttempt = 0;
    unsigned long telemetryLastSuccess = 0;
    uint16_t descriptorFailures = 0;
    uint16_t telemetryFailures = 0;
    uint32_t descriptorFingerprint = 0;
    uint8_t descriptorStableReads = 0;
    bool descriptorProfileMismatch = false;
    bool descriptorProfileUpdated = false;
    char lastError[25] = {0};
    BatteryTelemetry battery;
  };

  struct BusSnapshot {
    bool valid = false;
    uint8_t type = TYPE_NONE;
    uint8_t pins[OUTPUT_MAX_PINS] = {255, 255, 255, 255, 255};
    uint16_t start = 0;
    uint16_t length = 0;
    uint8_t colorOrder = COL_ORDER_GRB;
    bool reversed = false;
    uint8_t skip = 0;
    uint8_t autoWhite = RGBW_MODE_MANUAL_ONLY;
    uint16_t frequency = 0;
    uint8_t milliAmpsPerLed = LED_MILLIAMPS_DEFAULT;
    uint16_t milliAmpsMax = ABL_MILLIAMPS_DEFAULT;
    uint8_t driverType = 0;
    String text;
    uint8_t ledmap = 0;
  };

  // AI: below section was generated by an AI
  // Serializes state access between AsyncTCP callbacks and WLED's main loop on ESP32.
  struct StateGuard {
#ifdef ARDUINO_ARCH_ESP32
    SemaphoreHandle_t mutex;
    bool locked;
    StateGuard(SemaphoreHandle_t stateMutex, TickType_t waitTicks, bool allowMissingMutex = false)
      : mutex(stateMutex), locked(stateMutex ? xSemaphoreTake(stateMutex, waitTicks) == pdTRUE : allowMissingMutex) {}
    ~StateGuard() { if (mutex && locked) xSemaphoreGive(mutex); }
#else
    StateGuard(void *, unsigned long, bool = false) {}
#endif
    explicit operator bool() const {
#ifdef ARDUINO_ARCH_ESP32
      return locked;
#else
      return true;
#endif
    }
  };
  // AI: end

  struct ShadeProfile {
    char romStr[17] = {0};
    char name[33] = {0};
    uint16_t ledCount = 0;
    int8_t ledmap = -1;
    bool touch = true;
    InputMode inputMode = INPUT_INHERIT;
    int8_t buttonIndex = 0;
  };

  bool enabled = true;
  bool initDone = false;
  bool pinAllocated = false;
  bool scanRequested = false;
  bool learnRequested = false;
  bool clearProfilesRequested = false;
  bool applyRequested = false;
  bool autoApplyProfile = false;
  bool configDirty = false;
  bool smartProfilePersistPending = false;
  bool blockButtonWhenNoTouch = true;
  bool mqttPublish = true;
  bool mqttHomeAssistant = true;
  bool mqttDiscoveryPending = true;
  bool debugEnabled = false;
  bool internalPullup = ONEWIRE_ACCESSORY_INTERNAL_PULLUP;
  bool allowLegacyTelemetry = false;
  bool autoEnrollSmartShades = false;
  bool autoUpdateSmartProfiles = false;
  bool rebootRequired = false;
  bool hardwareReinitRequested = false;

  int8_t oneWirePin = ONEWIRE_ACCESSORY_PIN;
  int8_t buttonIndex = 0;
  uint8_t smartFamilyCode = ONEWIRE_ACCESSORY_FAMILY;
  uint16_t scanIntervalMs = DEFAULT_SCAN_INTERVAL_MS;
  uint16_t mqttIntervalMs = DEFAULT_MQTT_INTERVAL_MS;
  uint16_t fallbackLedCount = 0;
  int8_t fallbackLedmap = -2;
  uint8_t stableScansRequired = DEFAULT_STABLE_SCANS;

  unsigned long lastScanMs = 0;
  unsigned long lastManualScanRequestMs = 0;
  unsigned long lastSmartServiceMs = 0;
  unsigned long lastMqttPublishMs = 0;
  unsigned long lastSmartProfilePersistMs = 0;
  unsigned long applyFirstAttemptMs = 0;
  unsigned long lastApplyAttemptMs = 0;
  uint8_t stableScanCount = 0;
  uint32_t scanCount = 0;
  uint32_t crcErrorCount = 0;
  uint32_t applyCount = 0;
  uint32_t wireQueueFailures = 0;
  uint32_t stateLockFailures = 0;
  uint32_t maxWireTransactionUs = 0;
  uint16_t configInvalidValues = 0;
  uint16_t configSkippedProfiles = 0;
  ApplyPhase applyPhase = APPLY_IDLE;
  unsigned long applyPhaseStartedMs = 0;
  BusSnapshot rollbackBus;
  uint16_t applyTargetLedCount = 0;
  int8_t applyTargetLedmap = -1;
  bool applyTargetFallback = false;
  unsigned long debugLastScanPrintMs = 0;
  bool debugLastPresence = false;
  int debugLastIdleLevel = -1;
  uint8_t debugLastFoundCount = 255;
  uint8_t debugLastRoms[MAX_DEVICES][8] = {};

  OneWire *bus = nullptr;
#ifdef ARDUINO_ARCH_ESP32
  SemaphoreHandle_t stateMutex = nullptr;
  QueueHandle_t wireJobQueue = nullptr;
  QueueHandle_t wireResultQueue = nullptr;
  TaskHandle_t wireWorkerTaskHandle = nullptr;
#else
  void *stateMutex = nullptr;
#endif
  int8_t activeOneWirePin = -1;
  bool activeInternalPullup = false;
  bool activeEnabled = false;
  bool wireJobOutstanding = false;
  uint8_t wireGeneration = 0;

  Device devices[MAX_DEVICES];
  Device pendingDevices[MAX_DEVICES];
  char smartProfileUpdatedRoms[MAX_DEVICES][17] = {};
  uint8_t smartProfileUpdatedRomCount = 0;
  uint8_t deviceCount = 0;
  uint8_t pendingDeviceCount = 0;
  bool deviceOverflow = false;
  bool pendingDeviceOverflow = false;
  BusHealth busHealth = BUS_UNKNOWN;
  BusHealth pendingBusHealth = BUS_UNKNOWN;
  bool busStateKnown = false;
  uint8_t smartCursor = 0;

  ShadeProfile profiles[MAX_PROFILES];
  uint8_t profileCount = 0;
  bool profileConfigOverflow = false;
  uint8_t profileConfigDuplicates = 0;
  int8_t activeProfile = -1;
  uint8_t activeProfileMatches = 0;

  uint16_t learnLedCount = 0;
  int8_t learnLedmap = -1;
  bool learnTouch = true;
  InputMode learnInputMode = INPUT_TOUCH;
  int8_t learnButtonIndex = 0;
  char learnName[33] = {0};
  char applyStatus[49] = "idle";
  char inputStatus[49] = "idle";
  char commandCode[25] = "ok";
  char commandMessage[65] = "idle";
  int8_t configuredButtonIndex = -1;
  int8_t configuredProfileIndex = -2;
  InputMode configuredInputMode = INPUT_INHERIT;
  bool managedInputServiced = false;

#ifndef WLED_DISABLE_MQTT
  StaticJsonDocument<4096> mqttStatusDocument;
#endif

  static const char _name[];
  static const char _enabled[];
  static const char _pin[];
  static const char _scanInterval[];
  static const char _stableScans[];
  static const char _smartFamily[];
  static const char _mqttPublish[];
  static const char _mqttHomeAssistant[];
  static const char _mqttInterval[];
  static const char _debugEnabled[];
  static const char _internalPullup[];
  static const char _allowLegacyTelemetry[];
  static const char _autoEnrollSmartShades[];
  static const char _autoUpdateSmartProfiles[];
  static const char _profiles[];
  static const char _rom[];
  static const char _ledCount[];
  static const char _ledmap[];
  static const char _touch[];
  static const char _inputMode[];
  static const char _buttonIndex[];
  static const char _blockNoTouch[];
  static const char _autoApply[];
  static const char _fallbackLedCount[];
  static const char _fallbackLedmap[];
  static const char _profileSet[];
  static const char _profileDelete[];

  bool writeAllowed() const {
    return correctPIN || strlen(settingsPIN) == 0;
  }

  bool writeAllowed(JsonObject root) {
    if (writeAllowed()) return true;
    const char *pin = root["pin"] | nullptr;
    if (pin && pin[0]) {
      checkSettingsPIN(pin);
      return writeAllowed();
    }
    return false;
  }

  bool writeLocked() const {
    return !writeAllowed();
  }

  bool hasWriteCommand(JsonObject top) const {
    return (top[F("scan")] | false)
      || (top[F("apply")] | false)
      || (top[F("learn")] | false)
      || (top[F("clearProfiles")] | false)
      || (top[F("debugInject")] | false)
      || (top[F("debugClear")] | false)
      || !top[FPSTR(_profileSet)].isNull()
      || !top[FPSTR(_profileDelete)].isNull();
  }

  bool requireWriteAllowed(JsonObject root, JsonObject top) {
    if (!hasWriteCommand(top)) return true;
    if (writeAllowed(root)) return true;
    strlcpy(applyStatus, "settings PIN required", sizeof(applyStatus));
    setCommandResult("settings_pin_required", applyStatus);
    return false;
  }

  static char hexNibble(uint8_t v) {
    v &= 0x0F;
    return v < 10 ? char('0' + v) : char('A' + v - 10);
  }

  static void romToString(const uint8_t rom[8], char out[17]) {
    for (uint8_t i = 0; i < 8; i++) {
      out[i * 2] = hexNibble(rom[i] >> 4);
      out[i * 2 + 1] = hexNibble(rom[i]);
    }
    out[16] = 0;
  }

  static bool romsEqual(const uint8_t a[8], const uint8_t b[8]) {
    for (uint8_t i = 0; i < 8; i++) if (a[i] != b[i]) return false;
    return true;
  }

  static bool validRomString(const char *s) {
    if (!s || strlen(s) != 16) return false;
    for (uint8_t i = 0; i < 16; i++) {
      const char c = s[i];
      const bool ok = (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
      if (!ok) return false;
    }
    return true;
  }

  static uint8_t hexValue(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return 0;
  }

  static bool parseRomString(const char *in, uint8_t rom[8], char normalized[17]) {
    if (!validRomString(in)) return false;
    normalizeRomString(in, normalized);
    for (uint8_t i = 0; i < 8; i++) {
      rom[i] = (hexValue(normalized[i * 2]) << 4) | hexValue(normalized[i * 2 + 1]);
    }
    return OneWire::crc8(rom, 7) == rom[7];
  }

  static void normalizeRomString(const char *in, char out[17]) {
    for (uint8_t i = 0; i < 16; i++) {
      char c = in[i];
      if (c >= 'a' && c <= 'f') c = char(c - 'a' + 'A');
      out[i] = c;
    }
    out[16] = 0;
  }

  static void copyName(char *dst, const char *src) {
    if (!src || !src[0]) {
      strlcpy(dst, "Shade", 33);
      return;
    }
    const size_t sourceLength = strlen(src);
    if (!owab::isValidUtf8Text(reinterpret_cast<const uint8_t *>(src), sourceLength)) {
      strlcpy(dst, "Shade", 33);
      return;
    }
    size_t copyLength = min<size_t>(sourceLength, 32);
    while (copyLength > 0 && (static_cast<uint8_t>(src[copyLength]) & 0xC0) == 0x80) copyLength--;
    memcpy(dst, src, copyLength);
    dst[copyLength] = 0;
  }

  static const char *kindName(DeviceKind kind) {
    switch (kind) {
      case KIND_PASSIVE: return "passive";
      case KIND_SMART_SHADE: return "smart_shade";
      case KIND_BATTERY: return "battery";
      case KIND_SENSOR: return "sensor";
      default: return "unknown";
    }
  }

  static const char *busHealthName(BusHealth health) {
    switch (health) {
      case BUS_EMPTY_CLEAN: return "empty_clean";
      case BUS_DEVICES: return "devices";
      case BUS_STUCK_LOW: return "stuck_low";
      case BUS_PRESENCE_INVALID: return "presence_invalid";
      case BUS_CRC_ERROR: return "crc_error";
      case BUS_OVERFLOW: return "overflow";
      case BUS_SEARCH_LIMIT: return "search_limit";
      default: return "unknown";
    }
  }

  const char *familyName(uint8_t family) const {
    if (family == smartFamilyCode) return "WLED accessory";
    switch (family) {
      case 0x01: return "DS1990/DS2401";
      case 0x09: return "DS2502/DS1982";
      case 0x28: return "DS18B20";
      default: return "unknown";
    }
  }

  static const char *inputModeName(InputMode mode) {
    switch (mode) {
      case INPUT_DISABLED: return "disabled";
      case INPUT_BUTTON_LOW: return "buttonLow";
      case INPUT_BUTTON_HIGH: return "buttonHigh";
      case INPUT_TOUCH: return "touch";
      case INPUT_TOUCH_SWITCH: return "touchSwitch";
      default: return "inherit";
    }
  }

  static InputMode legacyTouchInputMode(bool touch) {
    return touch ? INPUT_TOUCH : INPUT_DISABLED;
  }

  static bool inputModeIsTouch(InputMode mode) {
    return mode == INPUT_TOUCH || mode == INPUT_TOUCH_SWITCH;
  }

  static uint8_t inputModeToButtonType(InputMode mode) {
    switch (mode) {
      case INPUT_BUTTON_LOW: return BTN_TYPE_PUSH;
      case INPUT_BUTTON_HIGH: return BTN_TYPE_PUSH_ACT_HIGH;
      case INPUT_TOUCH: return BTN_TYPE_TOUCH;
      case INPUT_TOUCH_SWITCH: return BTN_TYPE_TOUCH_SWITCH;
      default: return BTN_TYPE_NONE;
    }
  }

  static InputMode parseInputModeString(const char *value, InputMode fallback = INPUT_INHERIT) {
    if (!value || !value[0]) return fallback;
    if (!strcmp(value, "inherit")) return INPUT_INHERIT;
    if (!strcmp(value, "disabled")) return INPUT_DISABLED;
    if (!strcmp(value, "buttonLow")) return INPUT_BUTTON_LOW;
    if (!strcmp(value, "buttonHigh")) return INPUT_BUTTON_HIGH;
    if (!strcmp(value, "touch")) return INPUT_TOUCH;
    if (!strcmp(value, "touchSwitch")) return INPUT_TOUCH_SWITCH;
    return fallback;
  }

  static InputMode parseInputMode(JsonVariant value, InputMode fallback = INPUT_INHERIT) {
    if (value.is<const char*>()) return parseInputModeString(value.as<const char*>(), fallback);
    if (value.is<int>()) {
      const int mode = value.as<int>();
      if (mode >= INPUT_INHERIT && mode <= INPUT_TOUCH_SWITCH) return static_cast<InputMode>(mode);
    }
    return fallback;
  }

  static bool parseInputModeStrict(JsonVariant value, InputMode &mode) {
    if (value.is<const char*>()) {
      const char *text = value.as<const char*>();
      if (!text) return false;
      if (!strcmp(text, "inherit")) mode = INPUT_INHERIT;
      else if (!strcmp(text, "disabled")) mode = INPUT_DISABLED;
      else if (!strcmp(text, "buttonLow")) mode = INPUT_BUTTON_LOW;
      else if (!strcmp(text, "buttonHigh")) mode = INPUT_BUTTON_HIGH;
      else if (!strcmp(text, "touch")) mode = INPUT_TOUCH;
      else if (!strcmp(text, "touchSwitch")) mode = INPUT_TOUCH_SWITCH;
      else return false;
      return true;
    }
    if (!value.is<int>()) return false;
    const int parsed = value.as<int>();
    if (parsed < INPUT_INHERIT || parsed > INPUT_TOUCH_SWITCH) return false;
    mode = static_cast<InputMode>(parsed);
    return true;
  }

  static uint16_t parseLedCount(JsonVariant value, uint16_t fallback = 0) {
    if (value.isNull()) return fallback;
    const int32_t parsed = value.as<int32_t>();
    if (parsed < 0 || parsed > MAX_LEDS_PER_BUS) return 0;
    return static_cast<uint16_t>(parsed);
  }

  static int8_t clampButtonIndex(int value) {
    if (value < 0) return 0;
    if (value >= WLED_MAX_BUTTONS) return WLED_MAX_BUTTONS - 1;
    return static_cast<int8_t>(value);
  }

  static int8_t clampLedmap(int value) {
    if (value < -2) return -1;
    if (value >= WLED_MAX_LEDMAPS) return WLED_MAX_LEDMAPS - 1;
    return static_cast<int8_t>(value);
  }

  void setCommandResult(const char *code, const char *message) {
    strlcpy(commandCode, code, sizeof(commandCode));
    strlcpy(commandMessage, message, sizeof(commandMessage));
  }

  void sortDevices(Device *list, uint8_t count) {
    for (uint8_t i = 0; i < count; i++) {
      for (uint8_t j = i + 1; j < count; j++) {
        if (strcmp(list[j].romStr, list[i].romStr) < 0) {
          Device tmp = list[i];
          list[i] = list[j];
          list[j] = tmp;
        }
      }
    }
  }

  bool sameDeviceList(const Device *a, uint8_t ac, const Device *b, uint8_t bc) const {
    if (ac != bc) return false;
    for (uint8_t i = 0; i < ac; i++) {
      if (!romsEqual(a[i].rom, b[i].rom)) return false;
    }
    return true;
  }

  void copyDeviceList(Device *dst, const Device *src, uint8_t count) {
    for (uint8_t i = 0; i < count; i++) dst[i] = src[i];
  }

#ifdef ARDUINO_ARCH_ESP32
  static bool workerReadPayload(OneWire &workerBus, WireOperation operation, const uint8_t rom[8], WireResult &result) {
    if (!workerBus.reset()) return false;
    workerBus.select(rom);
    workerBus.write(operation == WIRE_READ_DESCRIPTOR ? CMD_READ_DESCRIPTOR : CMD_READ_TELEMETRY);

    uint8_t length = SMART_BUSY_BYTE;
    for (uint8_t attempt = 0; attempt < SMART_READY_RETRIES && length == SMART_BUSY_BYTE; attempt++) {
      delayMicroseconds(SMART_RESPONSE_DELAY_US);
      length = workerBus.read();
    }
    const uint8_t maximumLength = operation == WIRE_READ_DESCRIPTOR ? MAX_DESCRIPTOR_BYTES : 48;
    if (length == 0 || length > maximumLength) return false;
    for (uint8_t i = 0; i < length; i++) result.payload[i] = workerBus.read();
    result.payloadLength = length;
    return true;
  }

  static void wireWorkerTask(void *parameter) {
    OneWireAccessoryBusUsermod *self = static_cast<OneWireAccessoryBusUsermod *>(parameter);
    WireJob job;
    for (;;) {
      if (xQueueReceive(self->wireJobQueue, &job, portMAX_DELAY) != pdTRUE) continue;

      WireResult result;
      result.operation = job.operation;
      result.generation = job.generation;
      memcpy(result.rom, job.rom, sizeof(result.rom));
      const uint32_t startedUs = micros();

      OneWire workerBus(job.pin);
      pinMode(job.pin, job.internalPullup ? INPUT_PULLUP : INPUT);
      if (job.operation == WIRE_SCAN) {
        result.presence = workerBus.reset();
        result.idleLevel = digitalRead(job.pin);
        workerBus.reset_search();
        uint8_t rom[8] = {0};
        uint8_t iterations = 0;
        while (workerBus.search(rom)) {
          if (++iterations > MAX_SEARCH_ITERATIONS) {
            result.iterationLimit = true;
            break;
          }
          if (OneWire::crc8(rom, 7) != rom[7]) {
            if (result.crcErrors < UINT8_MAX) result.crcErrors++;
            continue;
          }
          if (result.romCount >= MAX_DEVICES) {
            result.overflow = true;
            break;
          }
          memcpy(result.roms[result.romCount++], rom, sizeof(rom));
          taskYIELD();
        }
        result.success = !result.iterationLimit;
      } else if (job.operation == WIRE_READ_DESCRIPTOR || job.operation == WIRE_READ_TELEMETRY) {
        result.success = workerReadPayload(workerBus, job.operation, job.rom, result);
      }

      result.durationUs = uint32_t(micros() - startedUs);
      xQueueOverwrite(self->wireResultQueue, &result);
    }
  }

  bool initializeWireWorker() {
    wireJobQueue = xQueueCreate(1, sizeof(WireJob));
    wireResultQueue = xQueueCreate(1, sizeof(WireResult));
    if (!wireJobQueue || !wireResultQueue) {
      if (wireJobQueue) vQueueDelete(wireJobQueue);
      if (wireResultQueue) vQueueDelete(wireResultQueue);
      wireJobQueue = nullptr;
      wireResultQueue = nullptr;
      return false;
    }
    if (xTaskCreatePinnedToCore(wireWorkerTask, "owab-wire", WIRE_WORKER_STACK_BYTES, this, 1,
                                &wireWorkerTaskHandle, 0) == pdPASS) return true;
    vQueueDelete(wireJobQueue);
    vQueueDelete(wireResultQueue);
    wireJobQueue = nullptr;
    wireResultQueue = nullptr;
    wireWorkerTaskHandle = nullptr;
    return false;
  }

  bool queueWireJob(const WireJob &job) {
    if (!wireJobQueue || wireJobOutstanding) return false;
    if (xQueueSend(wireJobQueue, &job, 0) != pdTRUE) {
      wireQueueFailures++;
      return false;
    }
    wireJobOutstanding = true;
    return true;
  }
#endif

  bool discoveryShapeChanged(const Device &a, const Device &b) const {
    if (a.kind != b.kind) return true;
    return a.battery.validFields != b.battery.validFields;
  }

  static bool batteryFieldValid(const Device &device, uint8_t field) {
    return (device.battery.validFields & field) != 0;
  }

  static bool batteryValid(const Device &device) {
    return device.battery.validFields != 0;
  }

  static unsigned long batteryFieldUpdated(const Device &device, uint8_t field) {
    switch (field) {
      case owab::TELEMETRY_MILLIVOLTS: return device.battery.millivoltsUpdated;
      case owab::TELEMETRY_PERCENT: return device.battery.percentUpdated;
      case owab::TELEMETRY_CHARGING: return device.battery.chargingUpdated;
      case owab::TELEMETRY_TEMPERATURE: return device.battery.temperatureUpdated;
      case owab::TELEMETRY_STATUS: return device.battery.statusUpdated;
      default: return 0;
    }
  }

  uint32_t telemetryFieldAgeMs(const Device &device, uint8_t field, unsigned long now) const {
    if (!batteryFieldValid(device, field)) return UINT32_MAX;
    const unsigned long updated = batteryFieldUpdated(device, field);
    return uint32_t(now - updated);
  }

  bool telemetryFieldStale(const Device &device, uint8_t field, unsigned long now) const {
    return !batteryFieldValid(device, field)
      || telemetryFieldAgeMs(device, field, now) > DEFAULT_TELEMETRY_STALE_MS;
  }

  uint32_t telemetryAgeMs(const Device &device, unsigned long now) const {
    uint32_t youngest = UINT32_MAX;
    const uint8_t fields[] = {
      owab::TELEMETRY_MILLIVOLTS, owab::TELEMETRY_PERCENT, owab::TELEMETRY_CHARGING,
      owab::TELEMETRY_TEMPERATURE, owab::TELEMETRY_STATUS
    };
    for (uint8_t field : fields) {
      if (!batteryFieldValid(device, field)) continue;
      youngest = min(youngest, telemetryFieldAgeMs(device, field, now));
    }
    return youngest == UINT32_MAX ? 0 : youngest;
  }

  bool telemetryStale(const Device &device, unsigned long now) const {
    return batteryValid(device) && telemetryAgeMs(device, now) > DEFAULT_TELEMETRY_STALE_MS;
  }

  bool telemetryAnyStale(const Device &device, unsigned long now) const {
    const uint8_t fields[] = {
      owab::TELEMETRY_MILLIVOLTS, owab::TELEMETRY_PERCENT, owab::TELEMETRY_CHARGING,
      owab::TELEMETRY_TEMPERATURE, owab::TELEMETRY_STATUS
    };
    for (uint8_t field : fields) {
      if (batteryFieldValid(device, field) && telemetryFieldStale(device, field, now)) return true;
    }
    return false;
  }

  bool descriptorStale(const Device &device) const {
    return device.descriptorValid && device.descriptorLastSuccess != 0
      && millis() - device.descriptorLastSuccess > DESCRIPTOR_STALE_MS;
  }

  int8_t findProfile(const char *romStr) const {
    for (uint8_t i = 0; i < profileCount; i++) {
      if (strncmp(profiles[i].romStr, romStr, 17) == 0) return i;
    }
    return -1;
  }

  bool saveProfile(const char *rom, const char *name, uint16_t ledCount, bool touch, bool ledCountProvided, bool touchProvided,
                   InputMode inputMode, bool inputModeProvided, int8_t profileButtonIndex, bool buttonIndexProvided,
                   int8_t ledmap, bool ledmapProvided) {
    uint8_t parsedRom[8];
    char normalized[17];
    if (!parseRomString(rom, parsedRom, normalized)) {
      strlcpy(applyStatus, "invalid profile ROM", sizeof(applyStatus));
      setCommandResult("invalid_rom", applyStatus);
      return false;
    }

    int8_t profile = findProfile(normalized);
    if (profile < 0) {
      if (profileCount >= MAX_PROFILES) {
        strlcpy(applyStatus, "profile storage full", sizeof(applyStatus));
        setCommandResult("profile_storage_full", applyStatus);
        return false;
      }
      profile = profileCount++;
      strlcpy(profiles[profile].romStr, normalized, sizeof(profiles[profile].romStr));
      copyName(profiles[profile].name, name && name[0] ? name : "Shade");
      profiles[profile].ledCount = 0;
      profiles[profile].ledmap = -1;
      profiles[profile].touch = true;
      profiles[profile].inputMode = INPUT_INHERIT;
      profiles[profile].buttonIndex = buttonIndex;
    }

    if (name && name[0]) copyName(profiles[profile].name, name);
    if (ledCountProvided) profiles[profile].ledCount = ledCount;
    if (touchProvided) profiles[profile].touch = touch;
    if (inputModeProvided) {
      profiles[profile].inputMode = inputMode;
      if (inputMode != INPUT_INHERIT) profiles[profile].touch = inputModeIsTouch(inputMode);
    }
    if (buttonIndexProvided) profiles[profile].buttonIndex = clampButtonIndex(profileButtonIndex);
    if (ledmapProvided) profiles[profile].ledmap = clampLedmap(ledmap);

    updateActiveProfile();
    configDirty = true;
    mqttDiscoveryPending = true;
    strlcpy(applyStatus, "profile saved", sizeof(applyStatus));
    return true;
  }

  bool saveProfile(const char *rom, const char *name, uint16_t ledCount, bool touch, bool ledCountProvided, bool touchProvided) {
    return saveProfile(rom, name, ledCount, touch, ledCountProvided, touchProvided,
                       legacyTouchInputMode(touch), touchProvided, buttonIndex, false, -1, false);
  }

  bool deleteProfile(const char *rom) {
    uint8_t parsedRom[8];
    char normalized[17];
    if (!parseRomString(rom, parsedRom, normalized)) {
      strlcpy(applyStatus, "invalid profile ROM or CRC", sizeof(applyStatus));
      setCommandResult("invalid_rom", applyStatus);
      return false;
    }

    const int8_t profile = findProfile(normalized);
    if (profile < 0) {
      strlcpy(applyStatus, "profile not found", sizeof(applyStatus));
      setCommandResult("profile_not_found", applyStatus);
      return false;
    }

    for (uint8_t i = profile; i + 1 < profileCount; i++) profiles[i] = profiles[i + 1];
    profiles[profileCount - 1] = ShadeProfile();
    profileCount--;

    updateActiveProfile();
    configDirty = true;
    mqttDiscoveryPending = true;
    strlcpy(applyStatus, "profile deleted", sizeof(applyStatus));
    setCommandResult("ok", applyStatus);
    return true;
  }

  void addProfileToJson(JsonObject profile, uint8_t index) const {
    profile[FPSTR(_rom)] = profiles[index].romStr;
    profile[F("name")] = profiles[index].name;
    profile[FPSTR(_ledCount)] = profiles[index].ledCount;
    profile[FPSTR(_ledmap)] = profiles[index].ledmap;
    profile[FPSTR(_touch)] = profiles[index].touch;
    profile[FPSTR(_inputMode)] = inputModeName(profiles[index].inputMode);
    profile[FPSTR(_buttonIndex)] = profiles[index].buttonIndex;
    profile[F("active")] = index == static_cast<uint8_t>(activeProfile);
  }

  void addDeviceToJson(JsonObject dev, const Device &device) const {
    const unsigned long now = millis();
    dev[F("rom")] = device.romStr;
    dev[F("family")] = device.family;
    dev[F("familyName")] = familyName(device.family);
    dev[F("kind")] = device.kind;
    dev[F("kindName")] = kindName(device.kind);
    if (device.name[0]) dev[F("name")] = device.name;
    if (device.hardwareVersion[0]) dev[F("hw")] = device.hardwareVersion;
    if (device.firmwareVersion[0]) dev[F("fw")] = device.firmwareVersion;
    if (device.family == smartFamilyCode) {
      dev[F("descriptorValid")] = device.descriptorValid;
      dev[F("descriptorStale")] = descriptorStale(device);
      dev[F("descriptorAgeMs")] = device.descriptorLastSuccess == 0 ? 0 : millis() - device.descriptorLastSuccess;
      dev[F("descriptorFailures")] = device.descriptorFailures;
      dev[F("descriptorStableReads")] = device.descriptorStableReads;
      dev[F("descriptorProfileMismatch")] = device.descriptorProfileMismatch;
      dev[F("descriptorProfileUpdated")] = device.descriptorProfileUpdated;
      dev[F("telemetryFramed")] = device.telemetryFramed;
      dev[F("telemetrySequence")] = device.telemetrySequence;
      dev[F("telemetryFailures")] = device.telemetryFailures;
      if (device.lastError[0]) dev[F("lastError")] = device.lastError;
      if (batteryValid(device)) {
        dev[F("telemetryAgeMs")] = telemetryAgeMs(device, now);
        dev[F("telemetryStale")] = telemetryStale(device, now);
        dev[F("telemetryAnyStale")] = telemetryAnyStale(device, now);
      }
    }

    const int8_t profile = findProfile(device.romStr);
    dev[F("profileIndex")] = profile;
    if (profile >= 0) {
      dev[F("profile")] = profiles[profile].name;
      dev[FPSTR(_ledCount)] = profiles[profile].ledCount;
      dev[FPSTR(_ledmap)] = profiles[profile].ledmap;
      dev[FPSTR(_touch)] = profiles[profile].touch;
      dev[FPSTR(_inputMode)] = inputModeName(profiles[profile].inputMode);
      dev[FPSTR(_buttonIndex)] = profiles[profile].buttonIndex;
    }

    if (batteryValid(device)) {
      JsonObject battery = dev.createNestedObject(F("battery"));
      if (batteryFieldValid(device, owab::TELEMETRY_MILLIVOLTS)) {
        battery[F("mv")] = device.battery.millivolts;
        battery[F("mvAgeMs")] = telemetryFieldAgeMs(device, owab::TELEMETRY_MILLIVOLTS, now);
        battery[F("mvStale")] = telemetryFieldStale(device, owab::TELEMETRY_MILLIVOLTS, now);
      }
      if (batteryFieldValid(device, owab::TELEMETRY_PERCENT)) {
        battery[F("percent")] = device.battery.percent;
        battery[F("percentAgeMs")] = telemetryFieldAgeMs(device, owab::TELEMETRY_PERCENT, now);
        battery[F("percentStale")] = telemetryFieldStale(device, owab::TELEMETRY_PERCENT, now);
      }
      if (batteryFieldValid(device, owab::TELEMETRY_CHARGING)) {
        battery[F("charging")] = device.battery.charging;
        battery[F("chargingAgeMs")] = telemetryFieldAgeMs(device, owab::TELEMETRY_CHARGING, now);
        battery[F("chargingStale")] = telemetryFieldStale(device, owab::TELEMETRY_CHARGING, now);
      }
      if (batteryFieldValid(device, owab::TELEMETRY_TEMPERATURE)) {
        battery[F("temperature")] = device.battery.temperatureDeciC / 10.0f;
        battery[F("temperatureAgeMs")] = telemetryFieldAgeMs(device, owab::TELEMETRY_TEMPERATURE, now);
        battery[F("temperatureStale")] = telemetryFieldStale(device, owab::TELEMETRY_TEMPERATURE, now);
      }
      if (batteryFieldValid(device, owab::TELEMETRY_STATUS)) {
        battery[F("status")] = device.battery.status;
        battery[F("statusAgeMs")] = telemetryFieldAgeMs(device, owab::TELEMETRY_STATUS, now);
        battery[F("statusStale")] = telemetryFieldStale(device, owab::TELEMETRY_STATUS, now);
      }
    }
  }

  void updateActiveProfile() {
    const int8_t previous = activeProfile;
    activeProfile = -1;
    activeProfileMatches = 0;

    for (uint8_t i = 0; i < deviceCount; i++) {
      const int8_t profile = findProfile(devices[i].romStr);
      if (profile >= 0) {
        if (activeProfileMatches == 0) activeProfile = profile;
        activeProfileMatches++;
      }
    }

    if (activeProfileMatches > 1) {
      activeProfile = -1;
      strlcpy(applyStatus, "multiple matching profiles", sizeof(applyStatus));
      return;
    }

    if (autoApplyProfile && activeProfile >= 0 && activeProfile != previous) applyRequested = true;
    if (autoApplyProfile && activeProfile < 0 && busStateKnown && busHealth == BUS_EMPTY_CLEAN
        && deviceCount == 0 && fallbackLedCount > 0) applyRequested = true;
  }

  bool currentBusLengthIs(uint16_t ledCount) const {
    if (BusManager::getNumBusses() != 1) return false;
    Bus *current = BusManager::getBus(0);
    return current && current->isOk() && !current->isVirtual() && !current->isPlaceholder()
      && current->isDigital() && current->getLength() == ledCount
      && uint32_t(current->getStart()) + ledCount <= MAX_LEDS;
  }

  bool targetStateMatches(uint16_t ledCount, int8_t ledmap) const {
    if (!currentBusLengthIs(ledCount)) return false;
    if (ledmap == -1) return true;
    const uint8_t expectedLedmap = ledmap >= 0 ? static_cast<uint8_t>(ledmap) : 0;
    return currentLedmap == expectedLedmap;
  }

  void requestLedmap(int8_t ledmap) const {
    if (ledmap == -1) return;
    loadLedmap = ledmap >= 0 ? ledmap : WLED_MAX_LEDMAPS;
  }

  // AI: below section was generated by an AI
  // Verifies the asynchronous WLED bus rebuild and restores the previous bus if the target failed.
  void refreshApplyStatus() {
    if (applyPhase == APPLY_IDLE || doInitBusses || !busConfigs.empty()) return;

    if (applyPhase == APPLY_WAIT_TARGET) {
      if (targetStateMatches(applyTargetLedCount, applyTargetLedmap)) {
        strlcpy(applyStatus, applyTargetFallback ? "fallback applied" : "applied", sizeof(applyStatus));
        rollbackBus.valid = false;
        applyPhase = APPLY_IDLE;
        return;
      }

      if (millis() - applyPhaseStartedMs < APPLY_VERIFY_TIMEOUT_MS) return;
      if (!rollbackBus.valid) {
        strlcpy(applyStatus, "apply failed; no rollback", sizeof(applyStatus));
        applyPhase = APPLY_IDLE;
        return;
      }

      busConfigs.clear();
      busConfigs.emplace_back(
        rollbackBus.type,
        rollbackBus.pins,
        rollbackBus.start,
        rollbackBus.length,
        rollbackBus.colorOrder,
        rollbackBus.reversed,
        rollbackBus.skip,
        rollbackBus.autoWhite,
        rollbackBus.frequency,
        rollbackBus.milliAmpsPerLed,
        rollbackBus.milliAmpsMax,
        rollbackBus.driverType,
        rollbackBus.text
      );
      if (busConfigs.empty()) {
        strlcpy(applyStatus, "rollback allocation failed", sizeof(applyStatus));
        applyPhase = APPLY_IDLE;
        return;
      }
      doInitBusses = true;
      loadLedmap = rollbackBus.ledmap;
      applyPhase = APPLY_WAIT_ROLLBACK;
      applyPhaseStartedMs = millis();
      strlcpy(applyStatus, "apply failed; rolling back", sizeof(applyStatus));
      return;
    }

    if (targetStateMatches(rollbackBus.length, rollbackBus.ledmap)) {
      strlcpy(applyStatus, "apply failed; rolled back", sizeof(applyStatus));
      rollbackBus.valid = false;
      applyPhase = APPLY_IDLE;
    } else if (millis() - applyPhaseStartedMs >= APPLY_VERIFY_TIMEOUT_MS) {
      strlcpy(applyStatus, "apply and rollback failed", sizeof(applyStatus));
      applyPhase = APPLY_IDLE;
    }
  }
  // AI: end

  void debugPrintDeviceList() {
#if ONEWIRE_ACCESSORY_SERIAL_DEBUG
    Serial.printf_P(PSTR("[OneWireAccessory] stable devices: %u\n"), deviceCount);
    for (uint8_t i = 0; i < deviceCount; i++) {
      Serial.printf_P(PSTR("[OneWireAccessory] #%u rom=%s family=0x%02X kind=%s(%u)\n"),
                      i, devices[i].romStr, devices[i].family, kindName(devices[i].kind), devices[i].kind);
    }
#endif
  }

  void debugPrintScanResult(const Device *found, uint8_t foundCount, bool presence, int idleLevel) {
#if ONEWIRE_ACCESSORY_SERIAL_DEBUG
    bool devicesChanged = foundCount != debugLastFoundCount;
    if (!devicesChanged) {
      for (uint8_t i = 0; i < foundCount; i++) {
        if (!romsEqual(found[i].rom, debugLastRoms[i])) {
          devicesChanged = true;
          break;
        }
      }
    }

    const bool changed = devicesChanged
      || presence != debugLastPresence
      || idleLevel != debugLastIdleLevel;
    if (!changed && millis() - debugLastScanPrintMs < SERIAL_DEBUG_INTERVAL_MS) return;

    debugLastScanPrintMs = millis();
    debugLastPresence = presence;
    debugLastIdleLevel = idleLevel;
    debugLastFoundCount = foundCount;
    for (uint8_t i = 0; i < foundCount; i++) memcpy(debugLastRoms[i], found[i].rom, 8);

    Serial.printf_P(PSTR("[OneWireAccessory] scan=%lu presence=%u idle=%d found=%u crcErrors=%lu\n"),
                    scanCount, presence, idleLevel, foundCount, crcErrorCount);
    if (!changed) return;

    for (uint8_t i = 0; i < foundCount; i++) {
      Serial.printf_P(PSTR("[OneWireAccessory] scan #%u rom=%s family=0x%02X kind=%s(%u)\n"),
                      i, found[i].romStr, found[i].family, kindName(found[i].kind), found[i].kind);
    }
#endif
  }

  // AI: below section was generated by an AI
  // Keeps observed smart descriptors separate from accepted profile configuration.
  uint32_t descriptorFingerprint(const Device &device) const {
    uint32_t hash = 2166136261UL;
    const auto addByte = [&hash](uint8_t value) {
      hash ^= value;
      hash *= 16777619UL;
    };
    addByte(static_cast<uint8_t>(device.kind));
    addByte(device.ledCount & 0xFF);
    addByte(device.ledCount >> 8);
    addByte(device.touch ? 1 : 0);
    for (const char *p = device.name; *p; p++) addByte(static_cast<uint8_t>(*p));
    return hash;
  }

  bool profileMatchesDescriptor(const ShadeProfile &profile, const Device &device) const {
    return profile.ledCount == device.ledCount
      && profile.touch == device.touch
      && strncmp(profile.name, device.name[0] ? device.name : "Smart shade", sizeof(profile.name)) == 0;
  }

  bool smartProfileUpdatedThisBoot(const char *rom) const {
    for (uint8_t i = 0; i < smartProfileUpdatedRomCount; i++) {
      if (!strcmp(smartProfileUpdatedRoms[i], rom)) return true;
    }
    return false;
  }

  void markSmartProfileUpdatedThisBoot(const char *rom) {
    if (smartProfileUpdatedThisBoot(rom) || smartProfileUpdatedRomCount >= MAX_DEVICES) return;
    strlcpy(smartProfileUpdatedRoms[smartProfileUpdatedRomCount++], rom, 17);
  }

  void applySmartShadeAsProfile(Device &device) {
    if (!device.descriptorValid || device.kind != KIND_SMART_SHADE || device.ledCount == 0) return;

    int8_t profile = findProfile(device.romStr);
    bool created = false;
    if (profile < 0) {
      device.descriptorProfileMismatch = true;
      if (!autoEnrollSmartShades || device.descriptorStableReads < SMART_DESCRIPTOR_CONFIRMATIONS) return;
      if (profileCount >= MAX_PROFILES) return;
      profile = profileCount++;
      created = true;
    } else {
      device.descriptorProfileMismatch = !profileMatchesDescriptor(profiles[profile], device);
      if (!device.descriptorProfileMismatch) return;
      if (!autoUpdateSmartProfiles || device.descriptorStableReads < SMART_DESCRIPTOR_CONFIRMATIONS
          || smartProfileUpdatedThisBoot(device.romStr)) return;
    }

    const bool wasActive = profile == activeProfile;
    const uint16_t previousLedCount = profiles[profile].ledCount;
    strlcpy(profiles[profile].romStr, device.romStr, sizeof(profiles[profile].romStr));
    copyName(profiles[profile].name, device.name[0] ? device.name : "Smart shade");
    profiles[profile].ledCount = device.ledCount;
    profiles[profile].touch = device.touch;
    if (created) {
      profiles[profile].inputMode = legacyTouchInputMode(device.touch);
      profiles[profile].buttonIndex = buttonIndex;
    } else {
      markSmartProfileUpdatedThisBoot(device.romStr);
      device.descriptorProfileUpdated = true;
    }
    device.descriptorProfileMismatch = false;
    if (autoApplyProfile && wasActive && previousLedCount != device.ledCount) applyRequested = true;
    updateActiveProfile();
    const unsigned long now = millis();
    if (created || lastSmartProfilePersistMs == 0 || now - lastSmartProfilePersistMs >= SMART_PROFILE_WRITE_INTERVAL_MS) {
      configDirty = true;
      lastSmartProfilePersistMs = now;
      smartProfilePersistPending = false;
    } else {
      smartProfilePersistPending = true;
    }
    mqttDiscoveryPending = true;
  }

  bool parseDescriptor(Device &device, const uint8_t *payload, uint8_t len) {
    owab::DescriptorData descriptor;
    owab::ParseError error = owab::PARSE_OK;
    if (!owab::parseDescriptor(payload, len, MAX_LEDS_PER_BUS, descriptor, error)) {
      if (error == owab::PARSE_BAD_CRC) crcErrorCount++;
      strlcpy(device.lastError, owab::parseErrorName(error), sizeof(device.lastError));
      return false;
    }

    device.kind = static_cast<DeviceKind>(descriptor.moduleClass);
    device.ledCount = descriptor.hasLedCount ? descriptor.ledCount : 0;
    device.touch = descriptor.hasTouch ? descriptor.touch : true;
    strlcpy(device.name, descriptor.name, sizeof(device.name));
    strlcpy(device.hardwareVersion, descriptor.hardwareVersion, sizeof(device.hardwareVersion));
    strlcpy(device.firmwareVersion, descriptor.firmwareVersion, sizeof(device.firmwareVersion));
    const uint32_t fingerprint = descriptorFingerprint(device);
    if (fingerprint == device.descriptorFingerprint) {
      if (device.descriptorStableReads < UINT8_MAX) device.descriptorStableReads++;
    } else {
      device.descriptorFingerprint = fingerprint;
      device.descriptorStableReads = 1;
    }
    device.descriptorProfileUpdated = smartProfileUpdatedThisBoot(device.romStr);
    device.lastError[0] = 0;
    return true;
  }

  bool parseTelemetryPayload(Device &device, const uint8_t *payload, uint8_t len, unsigned long now) {
    owab::TelemetryData telemetry;
    owab::ParseError error = owab::PARSE_OK;
    if (!owab::parseTelemetry(payload, len, allowLegacyTelemetry, telemetry, error)) {
      if (error == owab::PARSE_BAD_CRC) crcErrorCount++;
      strlcpy(device.lastError, owab::parseErrorName(error), sizeof(device.lastError));
      return false;
    }
    if (telemetry.framed && device.telemetryFramed && device.telemetryLastSuccess != 0
        && telemetry.sequence == device.telemetrySequence) {
      strlcpy(device.lastError, "duplicate_sequence", sizeof(device.lastError));
      return false;
    }

    device.telemetryFramed = telemetry.framed;
    device.telemetrySequence = telemetry.sequence;
    if (telemetry.fields & owab::TELEMETRY_MILLIVOLTS) {
      device.battery.millivolts = telemetry.millivolts;
      device.battery.millivoltsUpdated = now;
    }
    if (telemetry.fields & owab::TELEMETRY_PERCENT) {
      device.battery.percent = telemetry.percent;
      device.battery.percentUpdated = now;
    }
    if (telemetry.fields & owab::TELEMETRY_CHARGING) {
      device.battery.charging = telemetry.charging;
      device.battery.chargingUpdated = now;
    }
    if (telemetry.fields & owab::TELEMETRY_TEMPERATURE) {
      device.battery.temperatureDeciC = telemetry.temperatureDeciC;
      device.battery.temperatureUpdated = now;
    }
    if (telemetry.fields & owab::TELEMETRY_STATUS) {
      device.battery.status = telemetry.status;
      device.battery.statusUpdated = now;
    }
    device.battery.validFields |= telemetry.fields;
    device.lastError[0] = 0;
    return true;
  }

#ifndef ARDUINO_ARCH_ESP32
  bool readAccessoryPayload(const uint8_t rom[8], uint8_t command, uint8_t *payload, uint8_t &payloadLen) {
    payloadLen = 0;
    if (!bus->reset()) return false;
    bus->select(rom);
    bus->write(command);

    uint8_t len = SMART_BUSY_BYTE;
    for (uint8_t attempt = 0; attempt < SMART_READY_RETRIES && len == SMART_BUSY_BYTE; attempt++) {
      delayMicroseconds(SMART_RESPONSE_DELAY_US);
      len = bus->read();
    }
    const uint8_t maximumLength = command == CMD_READ_TELEMETRY ? 48 : MAX_DESCRIPTOR_BYTES;
    if (len == 0 || len > maximumLength) return false;
    for (uint8_t i = 0; i < len; i++) payload[i] = bus->read();
    payloadLen = len;
    return true;
  }
#endif

  void copyCachedSmartDetails(Device &device) const {
    for (uint8_t i = 0; i < deviceCount; i++) {
      if (strcmp(devices[i].romStr, device.romStr) != 0) continue;
      Device cached = devices[i];
      memcpy(device.rom, cached.rom, sizeof(device.rom));
      device.family = cached.family;
      strlcpy(device.romStr, cached.romStr, sizeof(device.romStr));
      device.kind = cached.kind;
      device.ledCount = cached.ledCount;
      device.touch = cached.touch;
      strlcpy(device.name, cached.name, sizeof(device.name));
      strlcpy(device.hardwareVersion, cached.hardwareVersion, sizeof(device.hardwareVersion));
      strlcpy(device.firmwareVersion, cached.firmwareVersion, sizeof(device.firmwareVersion));
      device.descriptorValid = cached.descriptorValid;
      device.telemetryFramed = cached.telemetryFramed;
      device.telemetrySequence = cached.telemetrySequence;
      device.descriptorLastAttempt = cached.descriptorLastAttempt;
      device.descriptorLastSuccess = cached.descriptorLastSuccess;
      device.telemetryLastAttempt = cached.telemetryLastAttempt;
      device.telemetryLastSuccess = cached.telemetryLastSuccess;
      device.descriptorFailures = cached.descriptorFailures;
      device.telemetryFailures = cached.telemetryFailures;
      device.descriptorFingerprint = cached.descriptorFingerprint;
      device.descriptorStableReads = cached.descriptorStableReads;
      device.descriptorProfileMismatch = cached.descriptorProfileMismatch;
      device.descriptorProfileUpdated = cached.descriptorProfileUpdated;
      strlcpy(device.lastError, cached.lastError, sizeof(device.lastError));
      device.battery = cached.battery;
      return;
    }
  }

  void handleSmartResult(const WireResult &result, unsigned long now) {
    int8_t deviceIndex = -1;
    for (uint8_t i = 0; i < deviceCount; i++) {
      if (romsEqual(devices[i].rom, result.rom)) {
        deviceIndex = i;
        break;
      }
    }
    if (deviceIndex < 0) return;

    Device &device = devices[deviceIndex];
    const Device before = device;
    if (result.operation == WIRE_READ_DESCRIPTOR) {
      if (result.success && parseDescriptor(device, result.payload, result.payloadLength)) {
        device.descriptorValid = true;
        device.descriptorLastSuccess = now;
        device.descriptorFailures = 0;
        applySmartShadeAsProfile(device);
      } else {
        if (!result.success) strlcpy(device.lastError, "descriptor_read", sizeof(device.lastError));
        if (device.descriptorFailures < UINT16_MAX) device.descriptorFailures++;
        if (device.descriptorLastSuccess == 0 || now - device.descriptorLastSuccess > DESCRIPTOR_STALE_MS) {
          device.descriptorValid = false;
          device.kind = KIND_UNKNOWN;
        }
      }
    } else if (result.operation == WIRE_READ_TELEMETRY) {
      if (result.success && parseTelemetryPayload(device, result.payload, result.payloadLength, now)) {
        device.telemetryLastSuccess = now;
        device.telemetryFailures = 0;
      } else {
        if (!result.success) strlcpy(device.lastError, "telemetry_read", sizeof(device.lastError));
        if (device.telemetryFailures < UINT16_MAX) device.telemetryFailures++;
      }
    }
    if (discoveryShapeChanged(before, device)) mqttDiscoveryPending = true;
  }

#ifndef ARDUINO_ARCH_ESP32
  bool serviceSmartDevice(Device &device, unsigned long now) {
    if (device.family != smartFamilyCode) return false;
    uint8_t payload[MAX_DESCRIPTOR_BYTES];
    uint8_t payloadLen = 0;

    if (descriptorStale(device)) device.descriptorValid = false;
    const uint32_t descriptorInterval = device.descriptorValid && device.descriptorStableReads >= SMART_DESCRIPTOR_CONFIRMATIONS
      ? DEFAULT_DESCRIPTOR_INTERVAL_MS : DESCRIPTOR_RETRY_INTERVAL_MS;
    if (owab::intervalDue(now, device.descriptorLastAttempt, descriptorInterval)) {
      device.descriptorLastAttempt = now;
      const bool descriptorRead = readAccessoryPayload(device.rom, CMD_READ_DESCRIPTOR, payload, payloadLen);
      if (descriptorRead && parseDescriptor(device, payload, payloadLen)) {
        device.descriptorValid = true;
        device.descriptorLastSuccess = now;
        device.descriptorFailures = 0;
        applySmartShadeAsProfile(device);
      } else {
        if (!descriptorRead) strlcpy(device.lastError, "descriptor_read", sizeof(device.lastError));
        if (device.descriptorFailures < UINT16_MAX) device.descriptorFailures++;
        if (device.descriptorLastSuccess == 0 || now - device.descriptorLastSuccess > DESCRIPTOR_STALE_MS) {
          device.descriptorValid = false;
          device.kind = KIND_UNKNOWN;
        }
      }
      return true;
    }

    if (device.descriptorValid && owab::intervalDue(now, device.telemetryLastAttempt, DEFAULT_TELEMETRY_INTERVAL_MS)) {
      device.telemetryLastAttempt = now;
      if (readAccessoryPayload(device.rom, CMD_READ_TELEMETRY, payload, payloadLen)) {
        if (parseTelemetryPayload(device, payload, payloadLen, now)) {
          device.telemetryLastSuccess = now;
          device.telemetryFailures = 0;
        } else if (device.telemetryFailures < UINT16_MAX) {
          device.telemetryFailures++;
        }
      } else {
        strlcpy(device.lastError, "telemetry_read", sizeof(device.lastError));
        if (device.telemetryFailures < UINT16_MAX) device.telemetryFailures++;
      }
      return true;
    }

    return false;
  }
#endif

  void serviceSmartDevices() {
    if (!activeEnabled || !busStateKnown || busHealth != BUS_DEVICES || deviceCount == 0) return;
    const unsigned long now = millis();
    if (now - lastSmartServiceMs < SMART_SERVICE_INTERVAL_MS) return;
#ifdef ARDUINO_ARCH_ESP32
    if (wireJobOutstanding) return;
#endif
    lastSmartServiceMs = now;

    for (uint8_t offset = 0; offset < deviceCount; offset++) {
      const uint8_t index = (smartCursor + offset) % deviceCount;
#ifdef ARDUINO_ARCH_ESP32
      Device &device = devices[index];
      if (device.family != smartFamilyCode) continue;
      if (descriptorStale(device)) device.descriptorValid = false;
      const uint32_t descriptorInterval = device.descriptorValid && device.descriptorStableReads >= SMART_DESCRIPTOR_CONFIRMATIONS
        ? DEFAULT_DESCRIPTOR_INTERVAL_MS : DESCRIPTOR_RETRY_INTERVAL_MS;

      WireJob job;
      job.generation = wireGeneration;
      job.pin = activeOneWirePin;
      job.internalPullup = activeInternalPullup;
      memcpy(job.rom, device.rom, sizeof(job.rom));
      if (owab::intervalDue(now, device.descriptorLastAttempt, descriptorInterval)) {
        job.operation = WIRE_READ_DESCRIPTOR;
        if (!queueWireJob(job)) return;
        device.descriptorLastAttempt = now;
      } else if (device.descriptorValid
                 && owab::intervalDue(now, device.telemetryLastAttempt, DEFAULT_TELEMETRY_INTERVAL_MS)) {
        job.operation = WIRE_READ_TELEMETRY;
        if (!queueWireJob(job)) return;
        device.telemetryLastAttempt = now;
      } else {
        continue;
      }
      smartCursor = (index + 1) % deviceCount;
      return;
#else
      const Device before = devices[index];
      if (!serviceSmartDevice(devices[index], now)) continue;
      smartCursor = (index + 1) % deviceCount;
      if (discoveryShapeChanged(before, devices[index])) mqttDiscoveryPending = true;
      return;
#endif
    }
  }
  // AI: end

  // AI: below section was generated by an AI
  // Stabilizes bounded scan results and preserves cached smart state only for known ROMs.
  void commitPendingDevices() {
    busHealth = pendingBusHealth;
    busStateKnown = true;
    deviceOverflow = pendingDeviceOverflow;
    if (busHealth == BUS_DEVICES || busHealth == BUS_EMPTY_CLEAN) {
      deviceCount = pendingDeviceCount;
      copyDeviceList(devices, pendingDevices, deviceCount);
    }
    mqttDiscoveryPending = true;
    updateActiveProfile();
    debugPrintDeviceList();
  }

  void refreshStableDeviceDetails() {
    bool discoveryChanged = false;
    for (uint8_t i = 0; i < deviceCount; i++) {
      if (discoveryShapeChanged(devices[i], pendingDevices[i])) discoveryChanged = true;
      devices[i] = pendingDevices[i];
    }
    if (deviceOverflow != pendingDeviceOverflow) {
      deviceOverflow = pendingDeviceOverflow;
      discoveryChanged = true;
    }
    if (discoveryChanged) mqttDiscoveryPending = true;
    updateActiveProfile();
  }

  void processScanResult(const WireResult &result) {
    Device found[MAX_DEVICES];
    const uint8_t foundCount = min<uint8_t>(result.romCount, MAX_DEVICES);
    scanCount++;
    crcErrorCount += result.crcErrors;
    for (uint8_t i = 0; i < foundCount; i++) {
      memcpy(found[i].rom, result.roms[i], sizeof(found[i].rom));
      found[i].family = found[i].rom[0];
      romToString(found[i].rom, found[i].romStr);
      copyCachedSmartDetails(found[i]);
    }

    sortDevices(found, foundCount);
    debugPrintScanResult(found, foundCount, result.presence, result.idleLevel);

    BusHealth foundHealth = BUS_UNKNOWN;
    if (result.idleLevel == LOW) foundHealth = BUS_STUCK_LOW;
    else if (result.iterationLimit) foundHealth = BUS_SEARCH_LIMIT;
    else if (result.overflow) foundHealth = BUS_OVERFLOW;
    else if (result.crcErrors > 0) foundHealth = BUS_CRC_ERROR;
    else if (foundCount > 0) foundHealth = BUS_DEVICES;
    else if (result.presence) foundHealth = BUS_PRESENCE_INVALID;
    else foundHealth = BUS_EMPTY_CLEAN;

    if (sameDeviceList(found, foundCount, pendingDevices, pendingDeviceCount)
        && result.overflow == pendingDeviceOverflow && foundHealth == pendingBusHealth) {
      copyDeviceList(pendingDevices, found, foundCount);
      if (stableScanCount < 255) stableScanCount++;
    } else {
      pendingDeviceCount = foundCount;
      pendingDeviceOverflow = result.overflow;
      pendingBusHealth = foundHealth;
      copyDeviceList(pendingDevices, found, foundCount);
      stableScanCount = 1;
    }

    if (stableScanCount >= stableScansRequired) {
      const bool healthyListChanged = (pendingBusHealth == BUS_DEVICES || pendingBusHealth == BUS_EMPTY_CLEAN)
        && !sameDeviceList(devices, deviceCount, pendingDevices, pendingDeviceCount);
      if (busHealth != pendingBusHealth || healthyListChanged) {
        commitPendingDevices();
      } else if (busHealth == BUS_DEVICES || busHealth == BUS_EMPTY_CLEAN) {
        refreshStableDeviceDetails();
      }
    }
  }

#ifdef ARDUINO_ARCH_ESP32
  bool requestWorkerScan() {
    WireJob job;
    job.operation = WIRE_SCAN;
    job.generation = wireGeneration;
    job.pin = activeOneWirePin;
    job.internalPullup = activeInternalPullup;
    return queueWireJob(job);
  }

  void serviceWireResult() {
    if (!wireResultQueue) return;
    WireResult result;
    if (xQueueReceive(wireResultQueue, &result, 0) != pdTRUE) return;
    wireJobOutstanding = false;
    maxWireTransactionUs = max(maxWireTransactionUs, result.durationUs);
    if (result.generation != wireGeneration) return;
    if (result.operation == WIRE_SCAN) processScanResult(result);
    else handleSmartResult(result, millis());
  }
#else
  void scanBus() {
    if (!bus || activeOneWirePin < 0) return;
    WireResult result;
    result.operation = WIRE_SCAN;
    result.presence = bus->reset();
    result.idleLevel = digitalRead(activeOneWirePin);
    const uint32_t startedUs = micros();
    bus->reset_search();
    uint8_t rom[8] = {0};
    uint8_t iterations = 0;
    while (bus->search(rom)) {
      if (++iterations > MAX_SEARCH_ITERATIONS) {
        result.iterationLimit = true;
        break;
      }
      if (OneWire::crc8(rom, 7) != rom[7]) {
        if (result.crcErrors < UINT8_MAX) result.crcErrors++;
        continue;
      }
      if (result.romCount >= MAX_DEVICES) {
        result.overflow = true;
        break;
      }
      memcpy(result.roms[result.romCount++], rom, sizeof(rom));
    }
    result.success = !result.iterationLimit;
    result.durationUs = uint32_t(micros() - startedUs);
    maxWireTransactionUs = max(maxWireTransactionUs, result.durationUs);
    processScanResult(result);
  }
#endif
  // AI: end

  void learnCurrentShade() {
    if (!learnRequested) return;
    learnRequested = false;

    if (deviceCount != 1) {
      strlcpy(applyStatus, deviceCount == 0 ? "no device to learn" : "learn requires one device", sizeof(applyStatus));
      setCommandResult(deviceCount == 0 ? "no_device" : "multiple_devices", applyStatus);
      return;
    }

    if (saveProfile(devices[0].romStr, learnName, learnLedCount, learnTouch, true, true,
                    learnInputMode, true, learnButtonIndex, true, learnLedmap, true)) {
      setCommandResult("ok", "profile learned");
    }
  }

  void clearProfiles() {
    if (!clearProfilesRequested) return;
    clearProfilesRequested = false;
    for (uint8_t i = 0; i < MAX_PROFILES; i++) profiles[i] = ShadeProfile();
    profileCount = 0;
    activeProfile = -1;
    activeProfileMatches = 0;
    configDirty = true;
    mqttDiscoveryPending = true;
    setCommandResult("ok", "profiles cleared");
  }

  // AI: below section was generated by an AI
  // Strictly validates profile mutations before any persistent state is changed.
  bool validateProfileSet(JsonObject item) {
    const char *rom = item[FPSTR(_rom)] | "";
    uint8_t parsedRom[8];
    char normalized[17];
    if (!parseRomString(rom, parsedRom, normalized)) {
      strlcpy(applyStatus, "invalid profile ROM or CRC", sizeof(applyStatus));
      setCommandResult("invalid_rom", applyStatus);
      return false;
    }

    const bool ledCountProvided = !item[FPSTR(_ledCount)].isNull();
    if (ledCountProvided) {
      if (!item[FPSTR(_ledCount)].is<int>()) {
        setCommandResult("invalid_led_count", "LED count must be an integer");
        return false;
      }
      const int ledCount = item[FPSTR(_ledCount)].as<int>();
      if (ledCount < 1 || ledCount > MAX_LEDS_PER_BUS) {
        setCommandResult("invalid_led_count", "LED count is outside the build limit");
        return false;
      }
    } else if (findProfile(normalized) < 0) {
      setCommandResult("missing_led_count", "new profiles require LED count");
      return false;
    }

    if (!item[FPSTR(_touch)].isNull() && !item[FPSTR(_touch)].is<bool>()) {
      setCommandResult("invalid_touch", "touch must be boolean");
      return false;
    }
    if (!item[FPSTR(_inputMode)].isNull()) {
      InputMode parsedMode;
      if (!parseInputModeStrict(item[FPSTR(_inputMode)], parsedMode)) {
        setCommandResult("invalid_input_mode", "unknown input mode");
        return false;
      }
    }
    if (!item[FPSTR(_buttonIndex)].isNull()) {
      if (!item[FPSTR(_buttonIndex)].is<int>()) {
        setCommandResult("invalid_button", "button slot must be an integer");
        return false;
      }
      const int value = item[FPSTR(_buttonIndex)].as<int>();
      if (value < 0 || value >= WLED_MAX_BUTTONS) {
        setCommandResult("invalid_button", "button slot is outside the build limit");
        return false;
      }
    }
    if (!item[FPSTR(_ledmap)].isNull()) {
      if (!item[FPSTR(_ledmap)].is<int>()) {
        setCommandResult("invalid_ledmap", "ledmap must be an integer");
        return false;
      }
      const int value = item[FPSTR(_ledmap)].as<int>();
      if (value < -2 || value >= WLED_MAX_LEDMAPS) {
        setCommandResult("invalid_ledmap", "ledmap is outside the build limit");
        return false;
      }
    }
    if (!item["name"].isNull()) {
      if (!item["name"].is<const char*>()) {
        setCommandResult("invalid_name", "name must be a UTF-8 string");
        return false;
      }
      const char *name = item["name"].as<const char*>();
      const size_t nameLength = strlen(name);
      if (nameLength > 32
          || !owab::isValidUtf8Text(reinterpret_cast<const uint8_t *>(name), nameLength)) {
        setCommandResult("invalid_name", "name must be valid UTF-8 and at most 32 bytes");
        return false;
      }
    }
    if (findProfile(normalized) < 0 && profileCount >= MAX_PROFILES) {
      setCommandResult("profile_storage_full", "profile storage is full");
      return false;
    }
    return true;
  }

  bool handleProfileSet(JsonObject item) {
    if (!validateProfileSet(item)) return false;

    const char *rom = item[FPSTR(_rom)] | "";
    const char *name = item["name"] | "";
    const bool ledCountProvided = !item[FPSTR(_ledCount)].isNull();
    const bool touchProvided = !item[FPSTR(_touch)].isNull();
    const bool inputModeProvided = !item[FPSTR(_inputMode)].isNull();
    const bool buttonIndexProvided = !item[FPSTR(_buttonIndex)].isNull();
    const bool ledmapProvided = !item[FPSTR(_ledmap)].isNull();
    const uint16_t ledCount = ledCountProvided ? item[FPSTR(_ledCount)].as<uint16_t>() : 0;
    const int8_t ledmap = ledmapProvided ? item[FPSTR(_ledmap)].as<int8_t>() : -1;
    const bool touch = item[FPSTR(_touch)] | true;
    InputMode inputMode = legacyTouchInputMode(touch);
    if (inputModeProvided) parseInputModeStrict(item[FPSTR(_inputMode)], inputMode);
    const int8_t profileButtonIndex = buttonIndexProvided ? item[FPSTR(_buttonIndex)].as<int8_t>() : buttonIndex;

    const bool saved = saveProfile(rom, name, ledCount, touch, ledCountProvided, touchProvided,
                                   inputMode, inputModeProvided || touchProvided, profileButtonIndex, buttonIndexProvided,
                                   ledmap, ledmapProvided);
    if (saved) setCommandResult("ok", "profile saved");
    return saved;
  }
  // AI: end

  bool handleProfileDelete(JsonVariant value) {
    if (value.is<const char*>()) {
      return deleteProfile(value.as<const char*>());
    }

    if (!value.is<JsonObject>()) {
      setCommandResult("invalid_profile_delete", "profileDelete must be a ROM string or object");
      return false;
    }
    JsonObject item = value.as<JsonObject>();
    if (!item[FPSTR(_rom)].is<const char*>()) {
      setCommandResult("invalid_profile_delete", "profileDelete object requires a ROM string");
      return false;
    }
    return deleteProfile(item[FPSTR(_rom)].as<const char*>());
  }

  void registerWebRoutes() {
    server.on(F("/onewire"), HTTP_GET, [](AsyncWebServerRequest *request) {
      request->send_P(200, F("text/html"), PAGE_ONEWIRE);
    });
  }

  // AI: below section was generated by an AI
  // Captures the one supported digital bus before replacing its length.
  bool captureCurrentBus(BusSnapshot &snapshot, uint16_t ledCount) {
    if (BusManager::getNumBusses() != 1) {
      strlcpy(applyStatus, "requires exactly one LED bus", sizeof(applyStatus));
      return false;
    }

    Bus *current = BusManager::getBus(0);
    if (!current || !current->isOk() || current->isVirtual() || current->isPlaceholder() || !current->isDigital()) {
      strlcpy(applyStatus, "current LED bus is not supported", sizeof(applyStatus));
      return false;
    }
    if (ledCount == 0 || ledCount > MAX_LEDS_PER_BUS
        || uint32_t(current->getStart()) + ledCount > MAX_LEDS) {
      strlcpy(applyStatus, "LED target exceeds bus bounds", sizeof(applyStatus));
      return false;
    }

    if (ledCount > current->getLength()) {
      const uint32_t growthBytes = uint32_t(ledCount - current->getLength()) * 16U;
      if (getFreeHeapSize() < growthBytes + APPLY_HEAP_MARGIN_BYTES) {
        strlcpy(applyStatus, "insufficient heap for LED target", sizeof(applyStatus));
        return false;
      }
    }

    snapshot = BusSnapshot();
    snapshot.valid = true;
    current->getPins(snapshot.pins);
    snapshot.type = current->getType();
    if (current->isOffRefreshRequired()) snapshot.type |= 0x80;
    snapshot.start = current->getStart();
    snapshot.length = current->getLength();
    snapshot.colorOrder = current->getColorOrder();
    snapshot.reversed = current->isReversed();
    snapshot.skip = current->skippedLeds();
    snapshot.autoWhite = current->getAutoWhiteMode();
    snapshot.frequency = current->getFrequency();
    snapshot.milliAmpsPerLed = current->getLEDCurrent();
    snapshot.milliAmpsMax = current->getMaxCurrent();
    snapshot.driverType = current->getDriverType();
    snapshot.text = current->getCustomText();
    snapshot.ledmap = currentLedmap;
    return true;
  }

  // Builds WLED's deferred bus configuration from a previously validated snapshot.
  bool scheduleBusLength(const BusSnapshot &snapshot, uint16_t ledCount) {
    uint8_t pins[OUTPUT_MAX_PINS];
    memcpy(pins, snapshot.pins, sizeof(pins));
    busConfigs.clear();
    busConfigs.emplace_back(
      snapshot.type,
      pins,
      snapshot.start,
      ledCount,
      snapshot.colorOrder,
      snapshot.reversed,
      snapshot.skip,
      snapshot.autoWhite,
      snapshot.frequency,
      snapshot.milliAmpsPerLed,
      snapshot.milliAmpsMax,
      snapshot.driverType,
      snapshot.text
    );
    return !busConfigs.empty();
  }
  // AI: end

  void applyActiveProfile() {
    if (!applyRequested) return;
    if (applyPhase != APPLY_IDLE) {
      strlcpy(applyStatus, "new target pending", sizeof(applyStatus));
      return;
    }

    const unsigned long now = millis();
    if (applyFirstAttemptMs == 0) applyFirstAttemptMs = now;
    if (lastApplyAttemptMs != 0 && now - lastApplyAttemptMs < APPLY_RETRY_INTERVAL_MS) return;
    lastApplyAttemptMs = now;

    uint16_t ledCount = 0;
    int8_t ledmap = -1;
    bool applyingFallback = false;
    if (activeProfile >= 0) {
      ledCount = profiles[activeProfile].ledCount;
      ledmap = profiles[activeProfile].ledmap;
    } else if (busStateKnown && busHealth == BUS_EMPTY_CLEAN && deviceCount == 0 && fallbackLedCount > 0) {
      ledCount = fallbackLedCount;
      ledmap = fallbackLedmap;
      applyingFallback = true;
    } else {
      applyRequested = false;
      applyFirstAttemptMs = 0;
      lastApplyAttemptMs = 0;
      strlcpy(applyStatus, "no active learned profile", sizeof(applyStatus));
      return;
    }

    if (ledCount == 0 || ledCount > MAX_LEDS_PER_BUS) {
      applyRequested = false;
      applyFirstAttemptMs = 0;
      lastApplyAttemptMs = 0;
      strlcpy(applyStatus, applyingFallback ? "invalid fallback LED count" : "invalid LED count", sizeof(applyStatus));
      return;
    }

    if (targetStateMatches(ledCount, ledmap)) {
      applyRequested = false;
      applyFirstAttemptMs = 0;
      lastApplyAttemptMs = 0;
      strlcpy(applyStatus, applyingFallback ? "fallback already applied" : "already applied", sizeof(applyStatus));
      return;
    }

    if (strip.isUpdating() || doInitBusses || !busConfigs.empty()) {
      if (now - applyFirstAttemptMs >= APPLY_TIMEOUT_MS) {
        applyRequested = false;
        applyFirstAttemptMs = 0;
        lastApplyAttemptMs = 0;
        strlcpy(applyStatus, "LED bus busy", sizeof(applyStatus));
        return;
      }
      applyRequested = true;
      strlcpy(applyStatus, "waiting for LED bus", sizeof(applyStatus));
      return;
    }

    applyRequested = false;
    BusSnapshot currentBus;
    if (!captureCurrentBus(currentBus, ledCount)) {
      applyFirstAttemptMs = 0;
      lastApplyAttemptMs = 0;
      return;
    }

    const int8_t effectiveLedmap = ledmap == -1 ? static_cast<int8_t>(currentBus.ledmap) : ledmap;
    rollbackBus = currentBus;
    applyTargetLedCount = ledCount;
    applyTargetLedmap = effectiveLedmap;
    applyTargetFallback = applyingFallback;
    applyPhase = APPLY_WAIT_TARGET;
    applyPhaseStartedMs = now;

    if (currentBus.length != ledCount) {
      if (!scheduleBusLength(currentBus, ledCount)) {
        rollbackBus.valid = false;
        applyPhase = APPLY_IDLE;
        applyFirstAttemptMs = 0;
        lastApplyAttemptMs = 0;
        strlcpy(applyStatus, "LED bus config allocation failed", sizeof(applyStatus));
        return;
      }
      doInitBusses = true;
    }
    requestLedmap(effectiveLedmap);
    applyCount++;
    applyFirstAttemptMs = 0;
    lastApplyAttemptMs = 0;
    strlcpy(applyStatus, applyingFallback ? "fallback scheduled" : "scheduled", sizeof(applyStatus));
  }

  bool profileInputManaged() const {
    return enabled && blockButtonWhenNoTouch && activeProfile >= 0 && activeProfile < static_cast<int8_t>(profileCount)
      && profiles[activeProfile].inputMode != INPUT_INHERIT;
  }

  void configureButtonHardware(uint8_t b, uint8_t type) {
    if (b >= buttons.size()) return;
    const int8_t pin = buttons[b].pin;
    if (pin < 0) return;

#if defined(ARDUINO_ARCH_ESP32)
#ifdef SOC_TOUCH_VERSION_2
    if (digitalPinToTouchChannel(pin) >= 0) touchDetachInterrupt(pin);
#endif
    if (type == BTN_TYPE_ANALOG || type == BTN_TYPE_ANALOG_INVERTED) {
      if (digitalPinToAnalogChannel(pin) >= 0) analogReadResolution(12);
      return;
    }

    if (type == BTN_TYPE_TOUCH || type == BTN_TYPE_TOUCH_SWITCH) {
      if (digitalPinToTouchChannel(pin) < 0) return;
#ifdef SOC_TOUCH_VERSION_2
      touchAttachInterrupt(pin, touchButtonISR, touchThreshold << 4);
#endif
      return;
    }
#endif

    if (type == BTN_TYPE_NONE || type == BTN_TYPE_RESERVED) return;

    if (disablePullUp) {
      pinMode(pin, INPUT);
    } else {
#ifdef ESP32
      pinMode(pin, type == BTN_TYPE_PUSH_ACT_HIGH ? INPUT_PULLDOWN : INPUT_PULLUP);
#else
      pinMode(pin, type == BTN_TYPE_PUSH_ACT_HIGH ? INPUT : INPUT_PULLUP);
#endif
    }
  }

  void restoreManagedInputHardware() {
    if (configuredButtonIndex >= 0 && configuredButtonIndex < static_cast<int8_t>(buttons.size())) {
      configureButtonHardware(configuredButtonIndex, buttons[configuredButtonIndex].type);
      buttons[configuredButtonIndex].pressedBefore = false;
      buttons[configuredButtonIndex].longPressed = false;
      buttons[configuredButtonIndex].waitTime = 0;
    }
    configuredProfileIndex = -2;
    configuredButtonIndex = -1;
    configuredInputMode = INPUT_INHERIT;
  }

  void configureActiveProfileInput() {
    if (!enabled || !blockButtonWhenNoTouch) {
      restoreManagedInputHardware();
      strlcpy(inputStatus, "input management off", sizeof(inputStatus));
      return;
    }

    if (activeProfile < 0 || activeProfile >= static_cast<int8_t>(profileCount)) {
      restoreManagedInputHardware();
      strlcpy(inputStatus, "no active profile input", sizeof(inputStatus));
      return;
    }

    const ShadeProfile &profile = profiles[activeProfile];
    if (configuredProfileIndex == activeProfile && configuredButtonIndex == profile.buttonIndex && configuredInputMode == profile.inputMode) return;

    if (profile.inputMode == INPUT_INHERIT) {
      restoreManagedInputHardware();
      configuredProfileIndex = activeProfile;
      configuredButtonIndex = profile.buttonIndex;
      configuredInputMode = INPUT_INHERIT;
      strlcpy(inputStatus, "input inherited", sizeof(inputStatus));
      return;
    }

    restoreManagedInputHardware();

    configuredProfileIndex = activeProfile;
    configuredButtonIndex = profile.buttonIndex;
    configuredInputMode = profile.inputMode;

    if (profile.buttonIndex < 0 || profile.buttonIndex >= static_cast<int8_t>(buttons.size())) {
      strlcpy(inputStatus, "button slot not configured", sizeof(inputStatus));
      return;
    }

    Button &button = buttons[profile.buttonIndex];
    button.pressedBefore = false;
    button.longPressed = false;
    button.waitTime = 0;
    if (button.pin < 0) {
      strlcpy(inputStatus, "button pin disabled", sizeof(inputStatus));
      return;
    }

    if (profile.inputMode == INPUT_DISABLED) {
      strlcpy(inputStatus, "input disabled", sizeof(inputStatus));
      return;
    }

    configureButtonHardware(profile.buttonIndex, inputModeToButtonType(profile.inputMode));

    if (inputModeIsTouch(profile.inputMode)) {
#if defined(ARDUINO_ARCH_ESP32) && !defined(CONFIG_IDF_TARGET_ESP32C3)
      if (digitalPinToTouchChannel(button.pin) < 0) {
        strlcpy(inputStatus, "pin is not touch capable", sizeof(inputStatus));
        return;
      }
      strlcpy(inputStatus, inputModeName(profile.inputMode), sizeof(inputStatus));
#else
      strlcpy(inputStatus, "touch unsupported", sizeof(inputStatus));
#endif
      return;
    }

    strlcpy(inputStatus, inputModeName(profile.inputMode), sizeof(inputStatus));
  }

  bool readManagedButton(uint8_t b, InputMode mode) const {
    if (b >= buttons.size()) return false;
    const int8_t pin = buttons[b].pin;
    if (pin < 0) return false;

    switch (mode) {
      case INPUT_BUTTON_LOW:
        return digitalRead(pin) == LOW;
      case INPUT_BUTTON_HIGH:
        return digitalRead(pin) == HIGH;
      case INPUT_TOUCH:
      case INPUT_TOUCH_SWITCH:
#if defined(ARDUINO_ARCH_ESP32) && !defined(CONFIG_IDF_TARGET_ESP32C3)
        if (digitalPinToTouchChannel(pin) < 0) return false;
#ifdef SOC_TOUCH_VERSION_2
        return touchInterruptGetLastStatus(pin);
#else
        return touchRead(pin) <= touchThreshold;
#endif
#else
        return false;
#endif
      default:
        return false;
    }
  }

  bool handleManagedSwitch(uint8_t b, InputMode mode) {
    const bool pressed = readManagedButton(b, mode);
    if (buttons[b].pressedBefore != pressed) {
      buttons[b].pressedTime = millis();
      buttons[b].pressedBefore = pressed;
    }

    if (buttons[b].longPressed == buttons[b].pressedBefore) return true;

    if (millis() - buttons[b].pressedTime > BUTTON_DEBOUNCE_MS) {
      if (!buttons[b].pressedBefore) {
        if (buttons[b].macroButton) applyPreset(buttons[b].macroButton, CALL_MODE_BUTTON_PRESET);
        else if (!bri) {
          toggleOnOff();
          stateUpdated(CALL_MODE_BUTTON);
        }
      } else {
        if (buttons[b].macroLongPress) applyPreset(buttons[b].macroLongPress, CALL_MODE_BUTTON_PRESET);
        else if (bri) {
          toggleOnOff();
          stateUpdated(CALL_MODE_BUTTON);
        }
      }

      buttons[b].longPressed = buttons[b].pressedBefore;
    }
    return true;
  }

  bool handleManagedMomentary(uint8_t b, InputMode mode) {
    const unsigned long now = millis();
    const bool pressed = readManagedButton(b, mode);

    if (pressed) {
      if (buttons[b].macroButton && buttons[b].macroButton == buttons[b].macroLongPress && buttons[b].macroButton == buttons[b].macroDoublePress) {
        if (!buttons[b].pressedBefore) shortPressAction(b);
        buttons[b].pressedBefore = true;
        buttons[b].pressedTime = now;
        return true;
      }

      if (!buttons[b].pressedBefore) buttons[b].pressedTime = now;
      buttons[b].pressedBefore = true;

      if (now - buttons[b].pressedTime > BUTTON_LONG_PRESS_MS) {
        if (!buttons[b].longPressed) {
          longPressAction(b);
        } else if (b && now - buttons[b].pressedTime > BUTTON_LONG_REPEAT_MS) {
          longPressAction(b);
          buttons[b].pressedTime = now - BUTTON_LONG_REPEAT_MS;
        }
        buttons[b].longPressed = true;
      }
      return true;
    }

    if (buttons[b].pressedBefore) {
      const unsigned long dur = now - buttons[b].pressedTime;

      if (buttons[b].macroButton && buttons[b].macroButton == buttons[b].macroLongPress && buttons[b].macroButton == buttons[b].macroDoublePress) {
        if (dur > BUTTON_DEBOUNCE_MS) buttons[b].pressedBefore = false;
        return true;
      }

      if (dur < BUTTON_DEBOUNCE_MS) {
        buttons[b].pressedBefore = false;
        return true;
      }

      const bool doublePress = buttons[b].waitTime;
      buttons[b].waitTime = 0;

      if (b == 0 && dur > BUTTON_AP_MS) {
        if (dur > BUTTON_FACTORY_RESET_MS) {
          WLED_FS.format();
          doReboot = true;
        } else {
          WLED::instance().initAP(true);
        }
      } else if (!buttons[b].longPressed) {
        if (b != 1 && !buttons[b].macroDoublePress) {
          shortPressAction(b);
        } else {
          if (doublePress) doublePressAction(b);
          else buttons[b].waitTime = now;
        }
      }

      buttons[b].pressedBefore = false;
      buttons[b].longPressed = false;
    }

    if (buttons[b].waitTime && now - buttons[b].waitTime > BUTTON_DOUBLE_PRESS_MS && !buttons[b].pressedBefore) {
      buttons[b].waitTime = 0;
      shortPressAction(b);
    }

    return true;
  }

  void serviceManagedInput() {
    if (!profileInputManaged()) return;
    const ShadeProfile &profile = profiles[activeProfile];
    if (profile.buttonIndex < 0 || profile.buttonIndex >= static_cast<int8_t>(buttons.size())) return;
    if (profile.inputMode == INPUT_DISABLED) {
      managedInputServiced = true;
      return;
    }
    if (profile.inputMode == INPUT_TOUCH_SWITCH) {
      handleManagedSwitch(profile.buttonIndex, profile.inputMode);
    } else {
      handleManagedMomentary(profile.buttonIndex, profile.inputMode);
    }
    managedInputServiced = true;
  }

  bool injectDebugDevice(JsonObject top) {
    if (!(top[F("debugInject")] | false)) return false;
    if (!debugEnabled) {
      setCommandResult("debug_disabled", "debug injection is disabled");
      return false;
    }
    if (!top[FPSTR(_rom)].isNull() && !top[FPSTR(_rom)].is<const char*>()) {
      setCommandResult("invalid_rom", "debug ROM must be a string");
      return false;
    }

    const char *rom = top[FPSTR(_rom)] | "7A00000000000058";
    Device injected;
    if (!parseRomString(rom, injected.rom, injected.romStr)) {
      strlcpy(applyStatus, "invalid debug ROM", sizeof(applyStatus));
      setCommandResult("invalid_rom", applyStatus);
      return false;
    }

    injected.family = injected.rom[0];
    if (!top[F("kind")].isNull() && !top[F("kind")].is<int>()) {
      setCommandResult("invalid_kind", "debug kind must be an integer");
      return false;
    }
    const int requestedKind = top[F("kind")] | KIND_BATTERY;
    if (requestedKind != KIND_PASSIVE && requestedKind != KIND_SMART_SHADE
        && requestedKind != KIND_BATTERY && requestedKind != KIND_SENSOR && requestedKind != KIND_UNKNOWN) {
      setCommandResult("invalid_kind", "debug kind is unsupported");
      return false;
    }
    injected.kind = static_cast<DeviceKind>(requestedKind);
    injected.descriptorValid = injected.kind != KIND_UNKNOWN && injected.kind != KIND_PASSIVE;
    injected.descriptorLastSuccess = millis();
    if (!top[FPSTR(_ledCount)].isNull() && !top[FPSTR(_ledCount)].is<int>()) {
      setCommandResult("invalid_led_count", "debug LED count must be an integer");
      return false;
    }
    const int requestedLedCount = top[FPSTR(_ledCount)] | 0;
    if (requestedLedCount < 0 || requestedLedCount > MAX_LEDS_PER_BUS) {
      setCommandResult("invalid_led_count", "debug LED count is outside the build limit");
      return false;
    }
    injected.ledCount = requestedLedCount;
    if (!top[FPSTR(_touch)].isNull() && !top[FPSTR(_touch)].is<bool>()) {
      setCommandResult("invalid_touch", "debug touch must be boolean");
      return false;
    }
    injected.touch = top[FPSTR(_touch)] | true;
    copyName(injected.name, top[F("name")] | "Debug accessory");
    strlcpy(injected.hardwareVersion, top[F("hw")] | "debug", sizeof(injected.hardwareVersion));
    strlcpy(injected.firmwareVersion, top[F("fw")] | "debug", sizeof(injected.firmwareVersion));

    const unsigned long injectedAt = millis();
    if (!top[F("batteryPercent")].isNull()) {
      injected.battery.validFields |= owab::TELEMETRY_PERCENT;
      injected.battery.percent = min<uint8_t>(top[F("batteryPercent")].as<uint8_t>(), 100);
      injected.battery.percentUpdated = injectedAt;
    }
    if (!top[F("batteryVoltage")].isNull()) {
      injected.battery.validFields |= owab::TELEMETRY_MILLIVOLTS;
      const float voltage = top[F("batteryVoltage")] | 3.9f;
      injected.battery.millivolts = static_cast<uint16_t>(voltage * 1000.0f);
      injected.battery.millivoltsUpdated = injectedAt;
    }
    if (!top[F("charging")].isNull()) {
      injected.battery.validFields |= owab::TELEMETRY_CHARGING;
      injected.battery.charging = top[F("charging")] | false;
      injected.battery.chargingUpdated = injectedAt;
    }
    if (!top[F("temperature")].isNull()) {
      injected.battery.validFields |= owab::TELEMETRY_TEMPERATURE;
      const float temp = top[F("temperature")] | 25.0f;
      injected.battery.temperatureDeciC = static_cast<int16_t>(temp * 10.0f);
      injected.battery.temperatureUpdated = injectedAt;
    }
    if (!top[F("status")].isNull()) {
      injected.battery.validFields |= owab::TELEMETRY_STATUS;
      injected.battery.status = top[F("status")] | 0;
      injected.battery.statusUpdated = injectedAt;
    }

    deviceCount = 1;
    pendingDeviceCount = 1;
    deviceOverflow = false;
    pendingDeviceOverflow = false;
    devices[0] = injected;
    pendingDevices[0] = injected;
    stableScanCount = stableScansRequired;
    if (injected.kind == KIND_SMART_SHADE) applySmartShadeAsProfile(devices[0]);
    updateActiveProfile();
    mqttDiscoveryPending = true;
    strlcpy(applyStatus, "debug device injected", sizeof(applyStatus));
    setCommandResult("ok", applyStatus);
    return true;
  }

  bool clearDebugDevices(JsonObject top) {
    if (!(top[F("debugClear")] | false)) return false;
    if (!debugEnabled) {
      setCommandResult("debug_disabled", "debug clear is disabled");
      return false;
    }
    deviceCount = 0;
    pendingDeviceCount = 0;
    deviceOverflow = false;
    pendingDeviceOverflow = false;
    stableScanCount = 0;
    activeProfile = -1;
    activeProfileMatches = 0;
    mqttDiscoveryPending = true;
    strlcpy(applyStatus, "debug devices cleared", sizeof(applyStatus));
    setCommandResult("ok", applyStatus);
    return true;
  }

#ifndef WLED_DISABLE_MQTT
  // AI: below section was generated by an AI
  // Publishes bounded status and retryable Home Assistant discovery using WLED's MQTT session.
  void buildDeviceTopic(char *topic, size_t topicSize, const char *romStr, const char *field) const {
    snprintf_P(topic, topicSize, PSTR("%s/onewire/%s/%s"), mqttDeviceTopic, romStr, field);
  }

  void buildSafeMqttId(char *safeId, size_t safeIdSize) const {
    size_t written = 0;
    for (size_t i = 0; mqttClientID[i] && written + 1 < safeIdSize; i++) {
      const char c = mqttClientID[i];
      safeId[written++] = ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
                           || (c >= '0' && c <= '9') || c == '_' || c == '-') ? c : '_';
    }
    if (written == 0 && safeIdSize > 1) {
      strlcpy(safeId, "wled", safeIdSize);
      return;
    }
    safeId[written] = 0;
  }

  uint16_t mqttExpireSeconds() const {
    const uint32_t publishSeconds = (uint32_t(mqttIntervalMs) + 999U) / 1000U;
    return static_cast<uint16_t>(min<uint32_t>(3600U, max<uint32_t>(180U, publishSeconds * 3U)));
  }

  bool publishMqttText(const char *suffix, const char *value, bool retain = false) {
    if (!WLED_MQTT_CONNECTED || !mqttPublish) return false;
    char topic[128];
    snprintf_P(topic, sizeof(topic), PSTR("%s/onewire/%s"), mqttDeviceTopic, suffix);
    return mqtt->publish(topic, 0, retain, value) != 0;
  }

  void publishMqttJsonStatus() {
    if (!WLED_MQTT_CONNECTED || !mqttPublish) return;
    const unsigned long now = millis();

    mqttStatusDocument.clear();
    auto &doc = mqttStatusDocument;
    doc[F("deviceCount")] = deviceCount;
    doc[F("deviceOverflow")] = deviceOverflow;
    doc[F("profileCount")] = profileCount;
    doc[F("activeProfileIndex")] = activeProfile;
    doc[F("activeProfileMatches")] = activeProfileMatches;
    doc[F("activeProfile")] = activeProfile >= 0 ? profiles[activeProfile].name : "";
    doc[F("applyStatus")] = applyStatus;
    doc[F("inputStatus")] = inputStatus;
    doc[F("commandCode")] = commandCode;
    doc[F("commandMessage")] = commandMessage;
    doc[F("busHealth")] = busHealthName(busHealth);
    doc[FPSTR(_fallbackLedCount)] = fallbackLedCount;
    doc[FPSTR(_fallbackLedmap)] = fallbackLedmap;

    JsonArray devs = doc.createNestedArray(F("devices"));
    for (uint8_t i = 0; i < deviceCount; i++) {
      JsonObject dev = devs.createNestedObject();
      dev[F("rom")] = devices[i].romStr;
      dev[F("family")] = devices[i].family;
      dev[F("familyName")] = familyName(devices[i].family);
      dev[F("kind")] = devices[i].kind;
      dev[F("kindName")] = kindName(devices[i].kind);
      if (devices[i].name[0]) dev[F("name")] = devices[i].name;
      if (devices[i].family == smartFamilyCode) {
        dev[F("descriptorValid")] = devices[i].descriptorValid;
        dev[F("descriptorStale")] = descriptorStale(devices[i]);
        dev[F("descriptorFailures")] = devices[i].descriptorFailures;
        dev[F("descriptorStableReads")] = devices[i].descriptorStableReads;
        dev[F("descriptorProfileMismatch")] = devices[i].descriptorProfileMismatch;
        dev[F("telemetryFramed")] = devices[i].telemetryFramed;
        dev[F("telemetryFailures")] = devices[i].telemetryFailures;
        if (devices[i].lastError[0]) dev[F("lastError")] = devices[i].lastError;
        if (batteryValid(devices[i])) {
          dev[F("telemetryAgeMs")] = telemetryAgeMs(devices[i], now);
          dev[F("telemetryStale")] = telemetryStale(devices[i], now);
          dev[F("telemetryAnyStale")] = telemetryAnyStale(devices[i], now);
        }
      }
      const int8_t profile = findProfile(devices[i].romStr);
      dev[F("profileIndex")] = profile;
      if (profile >= 0) {
        dev[F("profile")] = profiles[profile].name;
        dev[FPSTR(_ledCount)] = profiles[profile].ledCount;
        dev[FPSTR(_ledmap)] = profiles[profile].ledmap;
        dev[FPSTR(_touch)] = profiles[profile].touch;
        dev[FPSTR(_inputMode)] = inputModeName(profiles[profile].inputMode);
        dev[FPSTR(_buttonIndex)] = profiles[profile].buttonIndex;
      }
    }

    if (doc.overflowed()) {
      publishMqttText("status", "{\"error\":\"onewire status overflow\"}", false);
    } else {
      String payload;
      serializeJson(doc, payload);
      publishMqttText("status", payload.c_str(), false);
    }

    char value[24];
    snprintf_P(value, sizeof(value), PSTR("%u"), deviceCount);
    publishMqttText("device_count", value, true);
    publishMqttText("device_overflow", deviceOverflow ? "true" : "false", true);
    snprintf_P(value, sizeof(value), PSTR("%u"), profileCount);
    publishMqttText("profile_count", value, true);
    snprintf_P(value, sizeof(value), PSTR("%u"), fallbackLedCount);
    publishMqttText("fallback_led_count", value, true);
    snprintf_P(value, sizeof(value), PSTR("%d"), fallbackLedmap);
    publishMqttText("fallback_ledmap", value, true);
    publishMqttText("active_profile", activeProfile >= 0 ? profiles[activeProfile].name : "", true);
    publishMqttText("apply_status", applyStatus, true);
    publishMqttText("input_status", inputStatus, true);
  }

  void publishBatteryTelemetry(const Device &device) {
    if (!WLED_MQTT_CONNECTED || !mqttPublish || !batteryValid(device)) return;

    const unsigned long now = millis();
    char topic[128];
    char value[24];

    if (batteryFieldValid(device, owab::TELEMETRY_MILLIVOLTS)
        && !telemetryFieldStale(device, owab::TELEMETRY_MILLIVOLTS, now)) {
      buildDeviceTopic(topic, sizeof(topic), device.romStr, "battery_voltage");
      snprintf_P(value, sizeof(value), PSTR("%.3f"), device.battery.millivolts / 1000.0f);
      mqtt->publish(topic, 0, false, value);
    }

    if (batteryFieldValid(device, owab::TELEMETRY_PERCENT)
        && !telemetryFieldStale(device, owab::TELEMETRY_PERCENT, now)) {
      buildDeviceTopic(topic, sizeof(topic), device.romStr, "battery_percent");
      snprintf_P(value, sizeof(value), PSTR("%u"), device.battery.percent);
      mqtt->publish(topic, 0, false, value);
    }

    if (batteryFieldValid(device, owab::TELEMETRY_CHARGING)
        && !telemetryFieldStale(device, owab::TELEMETRY_CHARGING, now)) {
      buildDeviceTopic(topic, sizeof(topic), device.romStr, "battery_charging");
      mqtt->publish(topic, 0, false, device.battery.charging ? "true" : "false");
    }

    if (batteryFieldValid(device, owab::TELEMETRY_TEMPERATURE)
        && !telemetryFieldStale(device, owab::TELEMETRY_TEMPERATURE, now)) {
      buildDeviceTopic(topic, sizeof(topic), device.romStr, "battery_temperature");
      snprintf_P(value, sizeof(value), PSTR("%.1f"), device.battery.temperatureDeciC / 10.0f);
      mqtt->publish(topic, 0, false, value);
    }

    if (batteryFieldValid(device, owab::TELEMETRY_STATUS)
        && !telemetryFieldStale(device, owab::TELEMETRY_STATUS, now)) {
      buildDeviceTopic(topic, sizeof(topic), device.romStr, "battery_status");
      snprintf_P(value, sizeof(value), PSTR("%u"), device.battery.status);
      mqtt->publish(topic, 0, false, value);
    }

    buildDeviceTopic(topic, sizeof(topic), device.romStr, "battery_stale");
    mqtt->publish(topic, 0, false, telemetryAnyStale(device, now) ? "true" : "false");
  }

  bool publishHomeAssistantSensor(const Device &device, const char *field, const char *name, const char *deviceClass, const char *unit) {
    if (!WLED_MQTT_CONNECTED || !mqttPublish || !mqttHomeAssistant) return false;

    char stateTopic[128];
    buildDeviceTopic(stateTopic, sizeof(stateTopic), device.romStr, field);
    char availabilityTopic[128];
    snprintf_P(availabilityTopic, sizeof(availabilityTopic), PSTR("%s/status"), mqttDeviceTopic);
    char safeId[41];
    buildSafeMqttId(safeId, sizeof(safeId));

    String configTopic = String(F("homeassistant/sensor/")) + safeId + F("/onewire_") + device.romStr + F("_") + field + F("/config");

    StaticJsonDocument<768> doc;
    doc[F("name")] = String(serverDescription) + F(" ") + name + F(" ") + String(device.romStr + 12);
    doc[F("state_topic")] = stateTopic;
    doc[F("unique_id")] = String(safeId) + F("_onewire_") + device.romStr + F("_") + field;
    doc[F("availability_topic")] = availabilityTopic;
    doc[F("payload_available")] = F("online");
    doc[F("payload_not_available")] = F("offline");
    doc[F("expire_after")] = mqttExpireSeconds();
    if (deviceClass && deviceClass[0]) doc[F("device_class")] = deviceClass;
    if (unit && unit[0]) doc[F("unit_of_measurement")] = unit;
    if (unit && unit[0]) doc[F("state_class")] = F("measurement");

    JsonObject deviceObj = doc.createNestedObject(F("device"));
    deviceObj[F("name")] = serverDescription;
    deviceObj[F("identifiers")] = String(F("wled-")) + safeId;
    deviceObj[F("manufacturer")] = F(WLED_BRAND);
    deviceObj[F("model")] = F(WLED_PRODUCT_NAME);
    deviceObj[F("sw_version")] = versionString;

    if (doc.overflowed()) return false;
    String payload;
    payload.reserve(768);
    serializeJson(doc, payload);
    return mqtt->publish(configTopic.c_str(), 0, true, payload.c_str()) != 0;
  }

  bool publishHomeAssistantRootSensor(const char *field, const char *name, const char *icon, bool binary = false) {
    if (!WLED_MQTT_CONNECTED || !mqttPublish || !mqttHomeAssistant) return false;

    char stateTopic[128];
    snprintf_P(stateTopic, sizeof(stateTopic), PSTR("%s/onewire/%s"), mqttDeviceTopic, field);
    char availabilityTopic[128];
    snprintf_P(availabilityTopic, sizeof(availabilityTopic), PSTR("%s/status"), mqttDeviceTopic);
    char safeId[41];
    buildSafeMqttId(safeId, sizeof(safeId));

    String configTopic = String(binary ? F("homeassistant/binary_sensor/") : F("homeassistant/sensor/"))
      + safeId + F("/onewire_") + field + F("/config");

    StaticJsonDocument<768> doc;
    doc[F("name")] = String(serverDescription) + F(" ") + name;
    doc[F("state_topic")] = stateTopic;
    doc[F("unique_id")] = String(safeId) + F("_onewire_") + field;
    doc[F("availability_topic")] = availabilityTopic;
    doc[F("payload_available")] = F("online");
    doc[F("payload_not_available")] = F("offline");
    doc[F("expire_after")] = mqttExpireSeconds();
    if (binary) {
      doc[F("payload_on")] = F("true");
      doc[F("payload_off")] = F("false");
    }
    if (icon && icon[0]) doc[F("icon")] = icon;

    JsonObject deviceObj = doc.createNestedObject(F("device"));
    deviceObj[F("name")] = serverDescription;
    deviceObj[F("identifiers")] = String(F("wled-")) + safeId;
    deviceObj[F("manufacturer")] = F(WLED_BRAND);
    deviceObj[F("model")] = F(WLED_PRODUCT_NAME);
    deviceObj[F("sw_version")] = versionString;

    if (doc.overflowed()) return false;
    String payload;
    payload.reserve(768);
    serializeJson(doc, payload);
    return mqtt->publish(configTopic.c_str(), 0, true, payload.c_str()) != 0;
  }

  bool publishHomeAssistantBinarySensor(const Device &device, const char *field, const char *name) {
    if (!WLED_MQTT_CONNECTED || !mqttPublish || !mqttHomeAssistant) return false;

    char stateTopic[128];
    buildDeviceTopic(stateTopic, sizeof(stateTopic), device.romStr, field);
    char availabilityTopic[128];
    snprintf_P(availabilityTopic, sizeof(availabilityTopic), PSTR("%s/status"), mqttDeviceTopic);
    char safeId[41];
    buildSafeMqttId(safeId, sizeof(safeId));

    String configTopic = String(F("homeassistant/binary_sensor/")) + safeId + F("/onewire_") + device.romStr + F("_") + field + F("/config");

    StaticJsonDocument<768> doc;
    doc[F("name")] = String(serverDescription) + F(" ") + name + F(" ") + String(device.romStr + 12);
    doc[F("state_topic")] = stateTopic;
    doc[F("payload_on")] = F("true");
    doc[F("payload_off")] = F("false");
    doc[F("unique_id")] = String(safeId) + F("_onewire_") + device.romStr + F("_") + field;
    doc[F("availability_topic")] = availabilityTopic;
    doc[F("payload_available")] = F("online");
    doc[F("payload_not_available")] = F("offline");
    doc[F("expire_after")] = mqttExpireSeconds();

    JsonObject deviceObj = doc.createNestedObject(F("device"));
    deviceObj[F("name")] = serverDescription;
    deviceObj[F("identifiers")] = String(F("wled-")) + safeId;
    deviceObj[F("manufacturer")] = F(WLED_BRAND);
    deviceObj[F("model")] = F(WLED_PRODUCT_NAME);
    deviceObj[F("sw_version")] = versionString;

    if (doc.overflowed()) return false;
    String payload;
    payload.reserve(768);
    serializeJson(doc, payload);
    return mqtt->publish(configTopic.c_str(), 0, true, payload.c_str()) != 0;
  }

  bool publishHomeAssistantDiscovery() {
    if (!WLED_MQTT_CONNECTED || !mqttPublish || !mqttHomeAssistant) return false;

    bool success = true;
    success &= publishHomeAssistantRootSensor("device_count", "OneWire Device Count", "mdi:counter");
    success &= publishHomeAssistantRootSensor("device_overflow", "OneWire Device Overflow", "mdi:alert-circle-outline", true);
    success &= publishHomeAssistantRootSensor("profile_count", "OneWire Profile Count", "mdi:counter");
    success &= publishHomeAssistantRootSensor("fallback_led_count", "OneWire Fallback LED Count", "mdi:led-strip-variant");
    success &= publishHomeAssistantRootSensor("fallback_ledmap", "OneWire Fallback Ledmap", "mdi:map");
    success &= publishHomeAssistantRootSensor("active_profile", "OneWire Active Profile", "mdi:lamp");
    success &= publishHomeAssistantRootSensor("apply_status", "OneWire Apply Status", "mdi:information-outline");
    success &= publishHomeAssistantRootSensor("input_status", "OneWire Input Status", "mdi:gesture-tap-button");

    for (uint8_t i = 0; i < deviceCount; i++) {
      if (!batteryValid(devices[i])) continue;
      if (batteryFieldValid(devices[i], owab::TELEMETRY_MILLIVOLTS)) success &= publishHomeAssistantSensor(devices[i], "battery_voltage", "Battery Voltage", "voltage", "V");
      if (batteryFieldValid(devices[i], owab::TELEMETRY_PERCENT)) success &= publishHomeAssistantSensor(devices[i], "battery_percent", "Battery", "battery", "%");
      if (batteryFieldValid(devices[i], owab::TELEMETRY_CHARGING)) success &= publishHomeAssistantBinarySensor(devices[i], "battery_charging", "Battery Charging");
      if (batteryFieldValid(devices[i], owab::TELEMETRY_TEMPERATURE)) {
        success &= publishHomeAssistantSensor(devices[i], "battery_temperature", "Battery Temperature", "temperature", "\xC2\xB0" "C");
      }
      if (batteryFieldValid(devices[i], owab::TELEMETRY_STATUS)) success &= publishHomeAssistantSensor(devices[i], "battery_status", "Battery Status", "", "");
      success &= publishHomeAssistantBinarySensor(devices[i], "battery_stale", "Battery Telemetry Stale");
    }

    mqttDiscoveryPending = !success;
    return success;
  }

  void publishMqtt() {
    if (!WLED_MQTT_CONNECTED || !mqttPublish) return;
    if (mqttDiscoveryPending && mqttHomeAssistant) publishHomeAssistantDiscovery();

    publishMqttJsonStatus();
    for (uint8_t i = 0; i < deviceCount; i++) publishBatteryTelemetry(devices[i]);
  }
  // AI: end
#endif

  // AI: below section was generated by an AI
  // Applies changed OneWire hardware settings from the main loop and reports the active state.
  void teardownOneWire() {
    restoreManagedInputHardware();
#ifndef ARDUINO_ARCH_ESP32
    if (bus) {
      delete bus;
      bus = nullptr;
    }
#endif
    if (pinAllocated && activeOneWirePin >= 0) {
      pinMode(activeOneWirePin, INPUT);
      PinManager::deallocatePin(activeOneWirePin, PinOwner::UM_Unspecified);
    }
    pinAllocated = false;
    activeEnabled = false;
    activeOneWirePin = -1;
    activeInternalPullup = false;
    deviceCount = 0;
    pendingDeviceCount = 0;
    deviceOverflow = false;
    pendingDeviceOverflow = false;
    busHealth = BUS_UNKNOWN;
    pendingBusHealth = BUS_UNKNOWN;
    busStateKnown = false;
    stableScanCount = 0;
    smartCursor = 0;
    activeProfile = -1;
    activeProfileMatches = 0;
    rollbackBus.valid = false;
    applyPhase = APPLY_IDLE;
    applyRequested = autoApplyProfile;
  }

  bool initializeOneWire() {
    activeEnabled = enabled;
    activeOneWirePin = oneWirePin;
    activeInternalPullup = internalPullup;
    if (!enabled || oneWirePin < 0) {
      strlcpy(applyStatus, "OneWire disabled", sizeof(applyStatus));
      return true;
    }

    if (!PinManager::allocatePin(oneWirePin, false, PinOwner::UM_Unspecified)) {
      activeEnabled = false;
      strlcpy(applyStatus, "OneWire pin unavailable", sizeof(applyStatus));
#if ONEWIRE_ACCESSORY_SERIAL_DEBUG
      Serial.printf_P(PSTR("[OneWireAccessory] failed to allocate GPIO%d\n"), oneWirePin);
      Serial.printf_P(PSTR("[OneWireAccessory] GPIO%d owner=%s\n"), oneWirePin, PinManager::getPinOwnerName(oneWirePin));
#endif
      return false;
    }

    pinAllocated = true;
#ifdef ARDUINO_ARCH_ESP32
    if (!wireJobQueue || !wireResultQueue || !wireWorkerTaskHandle) {
      PinManager::deallocatePin(oneWirePin, PinOwner::UM_Unspecified);
      pinAllocated = false;
      activeEnabled = false;
      strlcpy(applyStatus, "OneWire worker unavailable", sizeof(applyStatus));
      return false;
    }
#else
    bus = new OneWire(oneWirePin);
    if (!bus) {
      PinManager::deallocatePin(oneWirePin, PinOwner::UM_Unspecified);
      pinAllocated = false;
      activeEnabled = false;
      strlcpy(applyStatus, "OneWire allocation failed", sizeof(applyStatus));
      return false;
    }
#endif
    pinMode(oneWirePin, internalPullup ? INPUT_PULLUP : INPUT);
    scanRequested = true;
    lastScanMs = 0;
#if ONEWIRE_ACCESSORY_SERIAL_DEBUG
    Serial.printf_P(PSTR("[OneWireAccessory] OneWire enabled on GPIO%d internalPullup=%u\n"), oneWirePin, internalPullup);
#endif
    return true;
  }

  void reinitializeOneWireIfRequested() {
    if (!hardwareReinitRequested) return;
#ifdef ARDUINO_ARCH_ESP32
    if (wireJobOutstanding) return;
#endif
    hardwareReinitRequested = false;
    wireGeneration++;
    teardownOneWire();
    rebootRequired = !initializeOneWire();
    mqttDiscoveryPending = true;
  }
  // AI: end

public:
  void setup() override {
    initDone = true;
    registerWebRoutes();
#ifdef ARDUINO_ARCH_ESP32
    stateMutex = xSemaphoreCreateMutex();
    if (!stateMutex) {
      enabled = false;
      strlcpy(applyStatus, "state mutex allocation failed", sizeof(applyStatus));
      return;
    }
    if (!initializeWireWorker()) {
      enabled = false;
      strlcpy(applyStatus, "OneWire worker allocation failed", sizeof(applyStatus));
      return;
    }
#endif
#if ONEWIRE_ACCESSORY_FORCE_PIN >= 0
    oneWirePin = ONEWIRE_ACCESSORY_FORCE_PIN;
#endif
    rebootRequired = !initializeOneWire();
  }

  void loop() override {
    if (!initDone) return;
    StateGuard stateGuard(stateMutex, 0);
    if (!stateGuard) {
      stateLockFailures++;
      return;
    }
#ifdef ARDUINO_ARCH_ESP32
    serviceWireResult();
#endif
    reinitializeOneWireIfRequested();
    managedInputServiced = false;

    clearProfiles();
    if (smartProfilePersistPending && millis() - lastSmartProfilePersistMs >= SMART_PROFILE_WRITE_INTERVAL_MS) {
      smartProfilePersistPending = false;
      lastSmartProfilePersistMs = millis();
      configDirty = true;
    }
    if (configDirty) {
      configDirty = false;
      configNeedsWrite = true;
    }

    if (!activeEnabled) return;

    const unsigned long now = millis();
    if (scanRequested || now - lastScanMs >= scanIntervalMs) {
#ifdef ARDUINO_ARCH_ESP32
      if (requestWorkerScan()) {
        scanRequested = false;
        lastScanMs = now;
      }
#else
      scanRequested = false;
      lastScanMs = now;
      scanBus();
#endif
    }
    serviceSmartDevices();

    learnCurrentShade();
    refreshApplyStatus();
    applyActiveProfile();
    configureActiveProfileInput();
    serviceManagedInput();

#ifndef WLED_DISABLE_MQTT
    if (mqttPublish && millis() - lastMqttPublishMs >= mqttIntervalMs) {
      lastMqttPublishMs = millis();
      publishMqtt();
    }
#endif

  }

  bool handleButton(uint8_t b) override {
    StateGuard stateGuard(stateMutex, STATE_LOCK_WAIT);
    if (!stateGuard) {
      stateLockFailures++;
      return false;
    }
    if (!profileInputManaged()) return false;
    const ShadeProfile &profile = profiles[activeProfile];
    if (profile.buttonIndex < 0 || b != static_cast<uint8_t>(profile.buttonIndex)) return false;
    if (managedInputServiced) return true;
    if (profile.inputMode == INPUT_DISABLED) return true;
    if (profile.inputMode == INPUT_TOUCH_SWITCH) return handleManagedSwitch(b, profile.inputMode);
    return handleManagedMomentary(b, profile.inputMode);
  }

  void addToJsonInfo(JsonObject& root) override {
    StateGuard stateGuard(stateMutex, STATE_LOCK_WAIT, !initDone);
    if (!stateGuard) {
      stateLockFailures++;
      return;
    }
    JsonObject user = root["u"];
    if (user.isNull()) user = root.createNestedObject("u");

    JsonArray status = user.createNestedArray(F("OneWire Bus"));
    if (!enabled) {
      status.add(F("disabled"));
    } else if (oneWirePin < 0 || !pinAllocated) {
      status.add(F("pin not allocated"));
    } else {
      status.add(deviceCount);
      status.add(F(" device(s)"));
    }

    if (activeProfile >= 0) {
      JsonArray shade = user.createNestedArray(F("OneWire Shade"));
      shade.add(profiles[activeProfile].name);
      if (profiles[activeProfile].ledCount > 0) {
        shade.add(String(profiles[activeProfile].ledCount) + F(" LEDs"));
      }
    }

    JsonObject ow = root[F("onewire")];
    if (ow.isNull()) ow = root.createNestedObject(F("onewire"));
    ow[F("enabled")] = enabled;
    ow[F("pin")] = oneWirePin;
    ow[F("activePin")] = activeOneWirePin;
    ow[FPSTR(_internalPullup)] = internalPullup;
    ow[F("activeInternalPullup")] = activeInternalPullup;
    ow[FPSTR(_allowLegacyTelemetry)] = allowLegacyTelemetry;
    ow[FPSTR(_autoEnrollSmartShades)] = autoEnrollSmartShades;
    ow[FPSTR(_autoUpdateSmartProfiles)] = autoUpdateSmartProfiles;
    ow[F("rebootRequired")] = rebootRequired;
    ow[F("busStateKnown")] = busStateKnown;
    ow[F("busHealth")] = busHealthName(busHealth);
    ow[F("scanCount")] = scanCount;
    ow[F("crcErrors")] = crcErrorCount;
    ow[F("applyCount")] = applyCount;
    ow[F("wireWorkerBusy")] = wireJobOutstanding;
    ow[F("wireQueueFailures")] = wireQueueFailures;
    ow[F("maxWireTransactionUs")] = maxWireTransactionUs;
    ow[F("stateLockFailures")] = stateLockFailures;
    ow[F("applyStatus")] = applyStatus;
    ow[F("inputStatus")] = inputStatus;
    ow[F("commandCode")] = commandCode;
    ow[F("commandMessage")] = commandMessage;
    ow[FPSTR(_fallbackLedCount)] = fallbackLedCount;
    ow[FPSTR(_fallbackLedmap)] = fallbackLedmap;
    ow[F("deviceOverflow")] = deviceOverflow;
    ow[F("profileConfigOverflow")] = profileConfigOverflow;
    ow[F("profileConfigDuplicates")] = profileConfigDuplicates;
    ow[F("configInvalidValues")] = configInvalidValues;
    ow[F("configSkippedProfiles")] = configSkippedProfiles;
    ow[F("writeLocked")] = writeLocked();
    ow[F("stableScans")] = stableScanCount;
    ow[F("activeProfileIndex")] = activeProfile;
    ow[F("activeProfileMatches")] = activeProfileMatches;

    JsonArray devs = ow.createNestedArray(F("devices"));
    for (uint8_t i = 0; i < deviceCount; i++) {
      JsonObject dev = devs.createNestedObject();
      addDeviceToJson(dev, devices[i]);
    }

    JsonArray profileArr = ow.createNestedArray(FPSTR(_profiles));
    for (uint8_t i = 0; i < profileCount; i++) {
      JsonObject profile = profileArr.createNestedObject();
      addProfileToJson(profile, i);
    }
  }

  void addToJsonState(JsonObject& root) override {
    StateGuard stateGuard(stateMutex, STATE_LOCK_WAIT, !initDone);
    if (!stateGuard) {
      stateLockFailures++;
      return;
    }
    JsonObject top = root[FPSTR(_name)];
    if (top.isNull()) top = root.createNestedObject(FPSTR(_name));
    top[FPSTR(_enabled)] = enabled;
    top[FPSTR(_internalPullup)] = internalPullup;
    top[FPSTR(_allowLegacyTelemetry)] = allowLegacyTelemetry;
    top[FPSTR(_autoEnrollSmartShades)] = autoEnrollSmartShades;
    top[FPSTR(_autoUpdateSmartProfiles)] = autoUpdateSmartProfiles;
    top[F("activePin")] = activeOneWirePin;
    top[F("rebootRequired")] = rebootRequired;
    top[F("busStateKnown")] = busStateKnown;
    top[F("busHealth")] = busHealthName(busHealth);
    top[F("deviceCount")] = deviceCount;
    top[F("deviceOverflow")] = deviceOverflow;
    top[F("profileConfigOverflow")] = profileConfigOverflow;
    top[F("profileConfigDuplicates")] = profileConfigDuplicates;
    top[F("configInvalidValues")] = configInvalidValues;
    top[F("configSkippedProfiles")] = configSkippedProfiles;
    top[F("wireWorkerBusy")] = wireJobOutstanding;
    top[F("wireQueueFailures")] = wireQueueFailures;
    top[F("maxWireTransactionUs")] = maxWireTransactionUs;
    top[F("stateLockFailures")] = stateLockFailures;
    top[F("profileCount")] = profileCount;
    top[F("activeProfileIndex")] = activeProfile;
    top[F("activeProfileMatches")] = activeProfileMatches;
    top[F("activeProfile")] = activeProfile >= 0 ? profiles[activeProfile].name : "";
    top[F("applyStatus")] = applyStatus;
    top[F("inputStatus")] = inputStatus;
    top[F("commandCode")] = commandCode;
    top[F("commandMessage")] = commandMessage;
    top[FPSTR(_fallbackLedCount)] = fallbackLedCount;
    top[FPSTR(_fallbackLedmap)] = fallbackLedmap;
    top[F("writeLocked")] = writeLocked();

    JsonObject limits = top.createNestedObject(F("limits"));
    limits[F("maxLedsPerBus")] = MAX_LEDS_PER_BUS;
    limits[F("maxButtons")] = WLED_MAX_BUTTONS;
    limits[F("maxLedmaps")] = WLED_MAX_LEDMAPS;
    limits[F("maxProfiles")] = MAX_PROFILES;

    JsonArray roms = top.createNestedArray(F("devices"));
    for (uint8_t i = 0; i < deviceCount; i++) roms.add(devices[i].romStr);

    JsonArray deviceRoms = top.createNestedArray(F("deviceRoms"));
    for (uint8_t i = 0; i < deviceCount; i++) deviceRoms.add(devices[i].romStr);

    JsonArray devs = top.createNestedArray(F("deviceDetails"));
    for (uint8_t i = 0; i < deviceCount; i++) {
      JsonObject dev = devs.createNestedObject();
      addDeviceToJson(dev, devices[i]);
    }

    JsonArray profileArr = top.createNestedArray(FPSTR(_profiles));
    for (uint8_t i = 0; i < profileCount; i++) {
      JsonObject profile = profileArr.createNestedObject();
      addProfileToJson(profile, i);
    }
  }

  void readFromJsonState(JsonObject& root) override {
    StateGuard stateGuard(stateMutex, STATE_LOCK_WAIT, !initDone);
    if (!stateGuard) {
      stateLockFailures++;
      setCommandResult("state_unavailable", "OneWire state mutex unavailable");
      return;
    }
    JsonObject top = root[FPSTR(_name)];
    if (top.isNull()) return;

    if (!requireWriteAllowed(root, top)) return;

    const char *booleanCommands[] = {"scan", "apply", "debugInject", "debugClear", "clearProfiles", "learn"};
    uint8_t commandCount = 0;
    for (const char *command : booleanCommands) {
      JsonVariant value = top[command];
      if (value.isNull()) continue;
      if (!value.is<bool>()) {
        setCommandResult("invalid_command_type", "boolean command has a non-boolean value");
        return;
      }
      if (value.as<bool>()) commandCount++;
    }
    if (!top[FPSTR(_profileSet)].isNull()) commandCount++;
    if (!top[FPSTR(_profileDelete)].isNull()) commandCount++;
    if (commandCount == 0) {
      setCommandResult("no_command", "no OneWire mutation requested");
      return;
    }
    if (commandCount > 1) {
      setCommandResult("multiple_commands", "submit exactly one OneWire mutation per request");
      return;
    }

    if (top[F("scan")] | false) {
      const unsigned long now = millis();
      if (lastManualScanRequestMs == 0 || now - lastManualScanRequestMs >= MANUAL_SCAN_RATE_LIMIT_MS) {
        lastManualScanRequestMs = now;
        scanRequested = true;
        setCommandResult("ok", "scan requested");
      } else {
        strlcpy(applyStatus, "scan rate limited", sizeof(applyStatus));
        setCommandResult("rate_limited", applyStatus);
      }
    }

    if (top[F("apply")] | false) {
      applyRequested = true;
      setCommandResult("ok", "apply requested");
    }

    injectDebugDevice(top);
    clearDebugDevices(top);

    JsonVariant profileSetValue = top[FPSTR(_profileSet)];
    if (!profileSetValue.isNull() && !profileSetValue.is<JsonArray>() && !profileSetValue.is<JsonObject>()) {
      setCommandResult("invalid_profile_set", "profileSet must be an object or array");
      return;
    }
    if (profileSetValue.is<JsonArray>()) {
      JsonArray profileSetArray = profileSetValue.as<JsonArray>();
      if (profileSetArray.size() > MAX_PROFILES) {
        setCommandResult("too_many_profiles", "profileSet array exceeds profile capacity");
      } else {
        bool valid = true;
        char requestRoms[MAX_PROFILES][17] = {};
        uint8_t requestRomCount = 0;
        uint8_t newProfileCount = 0;
        for (JsonObject item : profileSetArray) {
          if (!validateProfileSet(item)) {
            valid = false;
            break;
          }
          uint8_t parsedRom[8];
          char normalized[17];
          parseRomString(item[FPSTR(_rom)] | "", parsedRom, normalized);
          for (uint8_t i = 0; i < requestRomCount; i++) {
            if (!strcmp(requestRoms[i], normalized)) {
              setCommandResult("duplicate_rom", "profileSet array contains duplicate ROMs");
              valid = false;
              break;
            }
          }
          if (!valid) break;
          strlcpy(requestRoms[requestRomCount++], normalized, 17);
          if (findProfile(normalized) < 0) newProfileCount++;
        }
        if (valid && profileCount + newProfileCount > MAX_PROFILES) {
          setCommandResult("profile_storage_full", "profileSet array exceeds remaining profile capacity");
          valid = false;
        }
        if (valid) for (JsonObject item : profileSetArray) handleProfileSet(item);
      }
    } else {
      JsonObject profileSet = profileSetValue.as<JsonObject>();
      if (!profileSet.isNull()) handleProfileSet(profileSet);
    }

    JsonVariant profileDelete = top[FPSTR(_profileDelete)];
    if (!profileDelete.isNull()) handleProfileDelete(profileDelete);

    if (top[F("clearProfiles")] | false) {
      clearProfilesRequested = true;
      setCommandResult("ok", "clear profiles requested");
    }

    if (top[F("learn")] | false) {
      if (!top[FPSTR(_ledCount)].is<int>()) {
        setCommandResult("missing_led_count", "learn requires an integer LED count");
        return;
      }
      const int requestedLedCount = top[FPSTR(_ledCount)].as<int>();
      if (requestedLedCount < 1 || requestedLedCount > MAX_LEDS_PER_BUS) {
        setCommandResult("invalid_led_count", "learn LED count is outside the build limit");
        return;
      }

      if (!top[FPSTR(_ledmap)].isNull() && !top[FPSTR(_ledmap)].is<int>()) {
        setCommandResult("invalid_ledmap", "learn ledmap must be an integer");
        return;
      }
      const int requestedLedmap = top[FPSTR(_ledmap)] | -1;
      if (requestedLedmap < -2 || requestedLedmap >= WLED_MAX_LEDMAPS) {
        setCommandResult("invalid_ledmap", "learn ledmap is outside the build limit");
        return;
      }
      if (!top[FPSTR(_buttonIndex)].isNull() && !top[FPSTR(_buttonIndex)].is<int>()) {
        setCommandResult("invalid_button", "learn button slot must be an integer");
        return;
      }
      const int requestedButton = top[FPSTR(_buttonIndex)] | buttonIndex;
      if (requestedButton < 0 || requestedButton >= WLED_MAX_BUTTONS) {
        setCommandResult("invalid_button", "learn button slot is outside the build limit");
        return;
      }

      learnLedCount = requestedLedCount;
      learnLedmap = requestedLedmap;
      if (!top[FPSTR(_touch)].isNull() && !top[FPSTR(_touch)].is<bool>()) {
        setCommandResult("invalid_touch", "learn touch must be boolean");
        return;
      }
      learnTouch = top[FPSTR(_touch)] | true;
      learnInputMode = legacyTouchInputMode(learnTouch);
      if (!top[FPSTR(_inputMode)].isNull() && !parseInputModeStrict(top[FPSTR(_inputMode)], learnInputMode)) {
        setCommandResult("invalid_input_mode", "unknown learn input mode");
        return;
      }
      learnButtonIndex = requestedButton;
      if (!top[F("name")].isNull() && !top[F("name")].is<const char*>()) {
        setCommandResult("invalid_name", "learn name must be a string");
        return;
      }
      const char *name = top[F("name")] | "Shade";
      const size_t nameLength = strlen(name);
      if (nameLength > 32
          || !owab::isValidUtf8Text(reinterpret_cast<const uint8_t *>(name), nameLength)) {
        setCommandResult("invalid_name", "learn name must be valid UTF-8 and at most 32 bytes");
        return;
      }
      copyName(learnName, name);
      if (!top[FPSTR(_rom)].isNull() && !top[FPSTR(_rom)].is<const char*>()) {
        setCommandResult("invalid_rom", "learn ROM must be a string");
        return;
      }
      const char *rom = top[FPSTR(_rom)] | "";
      if (rom[0]) {
        uint8_t parsedRom[8];
        char normalized[17];
        if (!parseRomString(rom, parsedRom, normalized)) {
          setCommandResult("invalid_rom", "learn ROM has invalid format or CRC");
          return;
        }
        if (saveProfile(rom, learnName, learnLedCount, learnTouch, true, true,
                        learnInputMode, true, learnButtonIndex, true, learnLedmap, true)) {
          setCommandResult("ok", "profile learned");
        }
      } else {
        learnRequested = true;
        setCommandResult("ok", "learn requested");
      }
    }
  }

#ifndef WLED_DISABLE_MQTT
  void onMqttConnect(bool sessionPresent) override {
    StateGuard stateGuard(stateMutex, STATE_LOCK_WAIT, !initDone);
    if (!stateGuard) {
      stateLockFailures++;
      return;
    }
    mqttDiscoveryPending = true;
    lastMqttPublishMs = 0;
  }
#endif

  void addToConfig(JsonObject& root) override {
    StateGuard stateGuard(stateMutex, STATE_LOCK_WAIT, !initDone);
    if (!stateGuard) {
      stateLockFailures++;
      return;
    }
    JsonObject top = root.createNestedObject(FPSTR(_name));
    top[FPSTR(_enabled)] = enabled;
    top[FPSTR(_pin)] = oneWirePin;
    top[FPSTR(_scanInterval)] = scanIntervalMs;
    top[FPSTR(_stableScans)] = stableScansRequired;
    top[FPSTR(_smartFamily)] = smartFamilyCode;
    top[FPSTR(_mqttPublish)] = mqttPublish;
    top[FPSTR(_mqttHomeAssistant)] = mqttHomeAssistant;
    top[FPSTR(_mqttInterval)] = mqttIntervalMs;
    top[FPSTR(_debugEnabled)] = debugEnabled;
    top[FPSTR(_internalPullup)] = internalPullup;
    top[FPSTR(_allowLegacyTelemetry)] = allowLegacyTelemetry;
    top[FPSTR(_autoEnrollSmartShades)] = autoEnrollSmartShades;
    top[FPSTR(_autoUpdateSmartProfiles)] = autoUpdateSmartProfiles;
    top[FPSTR(_buttonIndex)] = buttonIndex;
    top[FPSTR(_blockNoTouch)] = blockButtonWhenNoTouch;
    top[FPSTR(_autoApply)] = autoApplyProfile;
    top[FPSTR(_fallbackLedCount)] = fallbackLedCount;
    top[FPSTR(_fallbackLedmap)] = fallbackLedmap;

    JsonArray arr = top.createNestedArray(FPSTR(_profiles));
    for (uint8_t i = 0; i < profileCount; i++) {
      JsonObject profile = arr.createNestedObject();
      profile[FPSTR(_rom)] = profiles[i].romStr;
      profile[F("name")] = profiles[i].name;
      profile[FPSTR(_ledCount)] = profiles[i].ledCount;
      profile[FPSTR(_ledmap)] = profiles[i].ledmap;
      profile[FPSTR(_touch)] = profiles[i].touch;
      profile[FPSTR(_inputMode)] = inputModeName(profiles[i].inputMode);
      profile[FPSTR(_buttonIndex)] = profiles[i].buttonIndex;
    }
  }

  bool readFromConfig(JsonObject& root) override {
    StateGuard stateGuard(stateMutex, STATE_LOCK_WAIT, !initDone);
    if (!stateGuard) {
      stateLockFailures++;
      return false;
    }
    JsonObject top = root[FPSTR(_name)];
    bool complete = !top.isNull();
    configInvalidValues = 0;
    configSkippedProfiles = 0;
    const bool previousMqttPublish = mqttPublish;
    const bool previousMqttHomeAssistant = mqttHomeAssistant;

    const auto readBool = [&](JsonVariant value, bool &target, bool fallback) {
      if (value.isNull()) {
        target = fallback;
        return false;
      }
      if (!value.is<bool>()) {
        target = fallback;
        configInvalidValues++;
        return false;
      }
      target = value.as<bool>();
      return true;
    };
    const auto readInteger = [&](JsonVariant value, int32_t &target, int32_t fallback, int32_t minimum, int32_t maximum) {
      if (value.isNull()) {
        target = fallback;
        return false;
      }
      if (!value.is<int32_t>()) {
        target = fallback;
        configInvalidValues++;
        return false;
      }
      const int32_t parsed = value.as<int32_t>();
      if (parsed < minimum || parsed > maximum) {
        target = fallback;
        configInvalidValues++;
        return false;
      }
      target = parsed;
      return true;
    };

    int32_t parsed = 0;
    complete &= readBool(top[FPSTR(_enabled)], enabled, true);
    complete &= readInteger(top[FPSTR(_pin)], parsed, -1, -1, 127); oneWirePin = static_cast<int8_t>(parsed);
    complete &= readInteger(top[FPSTR(_scanInterval)], parsed, DEFAULT_SCAN_INTERVAL_MS, 500, 60000); scanIntervalMs = static_cast<uint16_t>(parsed);
    complete &= readInteger(top[FPSTR(_stableScans)], parsed, DEFAULT_STABLE_SCANS, 1, 10); stableScansRequired = static_cast<uint8_t>(parsed);
    complete &= readInteger(top[FPSTR(_smartFamily)], parsed, ONEWIRE_ACCESSORY_FAMILY, 0, UINT8_MAX); smartFamilyCode = static_cast<uint8_t>(parsed);
    complete &= readBool(top[FPSTR(_mqttPublish)], mqttPublish, true);
    complete &= readBool(top[FPSTR(_mqttHomeAssistant)], mqttHomeAssistant, true);
    complete &= readInteger(top[FPSTR(_mqttInterval)], parsed, DEFAULT_MQTT_INTERVAL_MS, 1000, 60000); mqttIntervalMs = static_cast<uint16_t>(parsed);
    complete &= readBool(top[FPSTR(_debugEnabled)], debugEnabled, false);
    complete &= readBool(top[FPSTR(_internalPullup)], internalPullup, bool(ONEWIRE_ACCESSORY_INTERNAL_PULLUP));
    complete &= readBool(top[FPSTR(_allowLegacyTelemetry)], allowLegacyTelemetry, false);
    complete &= readBool(top[FPSTR(_autoEnrollSmartShades)], autoEnrollSmartShades, false);
    complete &= readBool(top[FPSTR(_autoUpdateSmartProfiles)], autoUpdateSmartProfiles, false);
    complete &= readInteger(top[FPSTR(_buttonIndex)], parsed, 0, 0, WLED_MAX_BUTTONS - 1); buttonIndex = static_cast<int8_t>(parsed);
    complete &= readBool(top[FPSTR(_blockNoTouch)], blockButtonWhenNoTouch, true);
    complete &= readBool(top[FPSTR(_autoApply)], autoApplyProfile, false);
    complete &= readInteger(top[FPSTR(_fallbackLedCount)], parsed, 0, 0, MAX_LEDS_PER_BUS); fallbackLedCount = static_cast<uint16_t>(parsed);
    complete &= readInteger(top[FPSTR(_fallbackLedmap)], parsed, -2, -2, WLED_MAX_LEDMAPS - 1); fallbackLedmap = static_cast<int8_t>(parsed);

#if ONEWIRE_ACCESSORY_FORCE_PIN >= 0
    oneWirePin = ONEWIRE_ACCESSORY_FORCE_PIN;
#endif

    if ((!previousMqttPublish && mqttPublish)
        || (!previousMqttHomeAssistant && mqttHomeAssistant)) mqttDiscoveryPending = true;

    if (initDone && (enabled != activeEnabled || oneWirePin != activeOneWirePin || internalPullup != activeInternalPullup)) {
      rebootRequired = true;
      hardwareReinitRequested = true;
    }

    JsonVariant profilesValue = top[FPSTR(_profiles)];
    complete &= !profilesValue.isNull();
    if (!profilesValue.isNull() && !profilesValue.is<JsonArray>()) {
      configInvalidValues++;
      complete = false;
    } else if (profilesValue.is<JsonArray>()) {
      JsonArray arr = profilesValue.as<JsonArray>();
      for (uint8_t i = 0; i < MAX_PROFILES; i++) profiles[i] = ShadeProfile();
      profileCount = 0;
      profileConfigOverflow = false;
      profileConfigDuplicates = 0;
      for (JsonVariant itemValue : arr) {
        if (!itemValue.is<JsonObject>()) {
          configSkippedProfiles++;
          complete = false;
          continue;
        }
        JsonObject item = itemValue.as<JsonObject>();
        if (profileCount >= MAX_PROFILES) {
          profileConfigOverflow = true;
          configSkippedProfiles++;
          complete = false;
          continue;
        }
        if (!item[FPSTR(_rom)].is<const char*>()) {
          configSkippedProfiles++;
          complete = false;
          continue;
        }
        const char *rom = item[FPSTR(_rom)].as<const char*>();
        uint8_t parsedRom[8];
        char normalized[17];
        if (!parseRomString(rom, parsedRom, normalized)) {
          configSkippedProfiles++;
          complete = false;
          continue;
        }
        if (findProfile(normalized) >= 0) {
          if (profileConfigDuplicates < UINT8_MAX) profileConfigDuplicates++;
          configSkippedProfiles++;
          complete = false;
          continue;
        }

        ShadeProfile candidate;
        strlcpy(candidate.romStr, normalized, sizeof(candidate.romStr));
        if (!item[F("name")].isNull() && !item[F("name")].is<const char*>()) {
          configSkippedProfiles++;
          complete = false;
          continue;
        }
        const char *profileName = item[F("name")] | "Shade";
        const size_t profileNameLength = strlen(profileName);
        if (profileNameLength > 32
            || !owab::isValidUtf8Text(reinterpret_cast<const uint8_t *>(profileName), profileNameLength)) {
          configSkippedProfiles++;
          complete = false;
          continue;
        }
        copyName(candidate.name, profileName);
        if (!item[FPSTR(_ledCount)].is<int32_t>()) {
          configSkippedProfiles++;
          complete = false;
          continue;
        }
        const int32_t profileLedCount = item[FPSTR(_ledCount)].as<int32_t>();
        if (profileLedCount < 1 || profileLedCount > MAX_LEDS_PER_BUS) {
          configSkippedProfiles++;
          complete = false;
          continue;
        }
        candidate.ledCount = static_cast<uint16_t>(profileLedCount);
        if (!item[FPSTR(_ledmap)].isNull() && !item[FPSTR(_ledmap)].is<int32_t>()) {
          configSkippedProfiles++;
          complete = false;
          continue;
        }
        const int32_t profileLedmap = item[FPSTR(_ledmap)] | -1;
        if (profileLedmap < -2 || profileLedmap >= WLED_MAX_LEDMAPS) {
          configSkippedProfiles++;
          complete = false;
          continue;
        }
        candidate.ledmap = static_cast<int8_t>(profileLedmap);
        if (!item[FPSTR(_touch)].isNull() && !item[FPSTR(_touch)].is<bool>()) {
          configSkippedProfiles++;
          complete = false;
          continue;
        }
        candidate.touch = item[FPSTR(_touch)] | true;
        candidate.inputMode = legacyTouchInputMode(candidate.touch);
        if (!item[FPSTR(_inputMode)].isNull() && !parseInputModeStrict(item[FPSTR(_inputMode)], candidate.inputMode)) {
          configSkippedProfiles++;
          complete = false;
          continue;
        }
        if (!item[FPSTR(_buttonIndex)].isNull() && !item[FPSTR(_buttonIndex)].is<int32_t>()) {
          configSkippedProfiles++;
          complete = false;
          continue;
        }
        const int32_t profileButtonIndex = item[FPSTR(_buttonIndex)] | buttonIndex;
        if (profileButtonIndex < 0 || profileButtonIndex >= WLED_MAX_BUTTONS) {
          configSkippedProfiles++;
          complete = false;
          continue;
        }
        candidate.buttonIndex = static_cast<int8_t>(profileButtonIndex);
        if (candidate.inputMode != INPUT_INHERIT) candidate.touch = inputModeIsTouch(candidate.inputMode);
        profiles[profileCount] = candidate;
        profileCount++;
      }
      updateActiveProfile();
    }

    return complete;
  }

  void appendConfigData() override {
    oappend(F("addInfo('")); oappend(FPSTR(_name)); oappend(F(":enabled',1,'<a href=\"/onewire\">Open OneWire manager</a>');"));
    oappend(F("addInfo('")); oappend(FPSTR(_name)); oappend(F(":pin',1,'OneWire data GPIO. Use -1 to disable. Changes are applied by the usermod at runtime.');"));
    oappend(F("addInfo('")); oappend(FPSTR(_name)); oappend(F(":scanIntervalMs',1,'Milliseconds between OneWire scans.');"));
    oappend(F("addInfo('")); oappend(FPSTR(_name)); oappend(F(":stableScans',1,'Number of equal scans required before hotplug state changes.');"));
    oappend(F("addInfo('")); oappend(FPSTR(_name)); oappend(F(":smartFamilyCode',1,'OneWire family code used by custom smart accessory modules. Default: 122 / 0x7A.');"));
    oappend(F("addInfo('")); oappend(FPSTR(_name)); oappend(F(":mqttIntervalMs',1,'Milliseconds between MQTT status/telemetry publishes.');"));
    oappend(F("addInfo('")); oappend(FPSTR(_name)); oappend(F(":debugEnabled',1,'Allows JSON debugInject/debugClear test devices. Keep disabled for normal use.');"));
    oappend(F("addInfo('")); oappend(FPSTR(_name)); oappend(F(":internalPullup',1,'Enable the ESP internal pullup on the OneWire GPIO. Changes are applied at runtime. Prefer an external pullup when hardware allows it.');"));
    oappend(F("addInfo('")); oappend(FPSTR(_name)); oappend(F(":allowLegacyTelemetry',1,'Accept unframed battery-only TLVs. Disabled by default because legacy telemetry has no frame CRC.');"));
    oappend(F("addInfo('")); oappend(FPSTR(_name)); oappend(F(":autoEnrollSmartShades',1,'Allow stable valid smart shades to create profiles automatically. Existing profiles remain enrolled when disabled.');"));
    oappend(F("addInfo('")); oappend(FPSTR(_name)); oappend(F(":autoUpdateSmartProfiles',1,'Allow one confirmed smart descriptor change per device and boot to update an existing profile. Disabled by default.');"));
    oappend(F("addInfo('")); oappend(FPSTR(_name)); oappend(F(":autoApplyProfile',1,'Automatically apply a learned shade LED count when it is detected.');"));
    oappend(F("addInfo('")); oappend(FPSTR(_name)); oappend(F(":fallbackLedCount',1,'LED count to apply when no OneWire accessory is detected and auto apply is enabled. 0 disables fallback.');"));
    oappend(F("addInfo('")); oappend(FPSTR(_name)); oappend(F(":fallbackLedmap',1,'Ledmap to apply with fallback. -1 leaves current map unchanged, -2 disables custom mapping.');"));
    oappend(F("addInfo('")); oappend(FPSTR(_name)); oappend(F(":buttonIndex',1,'Default WLED button slot for newly learned profiles. Per-profile input modes are edited in the OneWire manager.');"));
    oappend(F("addInfo('")); oappend(FPSTR(_name)); oappend(F(":blockButtonWhenNoTouch',1,'Enable profile-controlled button/touch input handling.');"));
  }
};

const char OneWireAccessoryBusUsermod::PAGE_ONEWIRE[] PROGMEM = R"=====(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>OneWire Accessory Bus</title>
<style>
:root{color-scheme:dark;--bg:#101114;--panel:#1b1d22;--line:#30343c;--text:#f1f3f7;--muted:#a5adba;--accent:#3fc07a;--warn:#f5b84b;--bad:#ff6b6b}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--text);font:14px system-ui,-apple-system,BlinkMacSystemFont,"Segoe UI",sans-serif}
header{display:flex;align-items:center;justify-content:space-between;gap:12px;padding:14px 16px;border-bottom:1px solid var(--line);background:#15171b;position:sticky;top:0}
h1{font-size:18px;margin:0;font-weight:650}
main{max-width:1080px;margin:0 auto;padding:16px;display:grid;gap:14px}
section{border-top:1px solid var(--line);padding-top:14px}
h2{font-size:15px;margin:0 0 10px;font-weight:650}
.toolbar{display:flex;gap:8px;align-items:center;flex-wrap:wrap}
button,input,select{font:inherit;color:var(--text);background:#23262d;border:1px solid var(--line);border-radius:6px;padding:8px 10px}
button{cursor:pointer}
button.primary{background:#1f6f46;border-color:#2e9c65}
button.danger{background:#5d2529;border-color:#8e343b}
button:disabled{opacity:.5;cursor:default}
.buttonLink{display:inline-block;color:var(--text);background:#23262d;border:1px solid var(--line);border-radius:6px;padding:8px 10px;text-decoration:none}
input,select{width:100%}
.grid{display:grid;grid-template-columns:repeat(4,minmax(0,1fr));gap:10px}
.field{display:grid;gap:5px}
label{color:var(--muted);font-size:12px}
table{width:100%;border-collapse:collapse;table-layout:fixed}
caption{text-align:left;color:var(--muted);font-size:12px;padding:0 0 6px}
th,td{border-bottom:1px solid var(--line);padding:8px;text-align:left;vertical-align:middle;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
th{color:var(--muted);font-size:12px;font-weight:600}
.mono{font-family:ui-monospace,SFMono-Regular,Menlo,Consolas,monospace;white-space:normal;overflow-wrap:anywhere}
.muted{color:var(--muted)}
.ok{color:var(--accent)}
.warn{color:var(--warn)}
.bad{color:var(--bad)}
.status{min-height:20px;color:var(--muted)}
@media(max-width:760px){header{align-items:flex-start;flex-direction:column}.grid{grid-template-columns:1fr}table{font-size:12px}th,td{padding:7px 5px}}
</style>
</head>
<body>
<header>
  <h1>OneWire Accessory Bus</h1>
  <div class="toolbar">
    <button id="refresh">Refresh</button>
    <button id="scan" class="primary">Scan</button>
    <a class="buttonLink" href="/">WLED</a>
  </div>
</header>
<main>
  <section>
    <h2>Status</h2>
    <div id="status" class="status" role="status" aria-live="polite">Loading</div>
  </section>

  <section>
    <h2>Access</h2>
    <div class="grid">
      <div class="field"><label for="pin">Settings PIN</label><input id="pin" type="password" inputmode="numeric" maxlength="4" autocomplete="off"></div>
    </div>
  </section>

  <section>
    <h2>Devices</h2>
    <table>
      <caption>Detected accessories</caption>
      <thead><tr><th scope="col">ROM</th><th scope="col">Family</th><th scope="col">Type</th><th scope="col">Profile</th><th scope="col">Battery</th></tr></thead>
      <tbody id="devices"><tr><td colspan="5" class="muted">Loading</td></tr></tbody>
    </table>
  </section>

  <section>
    <h2>Assign Shade Profile</h2>
    <div class="grid">
      <div class="field"><label for="rom">ROM</label><select id="rom"></select></div>
      <div class="field"><label for="name">Name</label><input id="name" maxlength="32" value="Shade"></div>
      <div class="field"><label for="ledCount">LED count</label><input id="ledCount" type="number" min="1" max="2048" step="1" value="1"></div>
      <div class="field"><label for="buttonIndex">Button slot</label><input id="buttonIndex" type="number" min="0" max="0" step="1" value="0"></div>
      <div class="field"><label for="ledmap">Ledmap</label><input id="ledmap" type="number" min="-2" max="15" step="1" value="-1"></div>
      <div class="field"><label for="inputMode">Input mode</label><select id="inputMode">
        <option value="inherit">inherit WLED</option>
        <option value="disabled">disabled</option>
        <option value="buttonLow">button active-low</option>
        <option value="buttonHigh">button active-high</option>
        <option value="touch">touch</option>
        <option value="touchSwitch">touch switch</option>
      </select></div>
    </div>
    <div class="toolbar" style="margin-top:10px">
      <button id="save" class="primary">Save Profile</button>
      <button id="apply">Apply Active</button>
    </div>
  </section>

  <section>
    <h2>Profiles</h2>
    <table>
      <caption>Saved shade profiles</caption>
      <thead><tr><th scope="col">ROM</th><th scope="col">Name</th><th scope="col">LEDs</th><th scope="col">Input</th><th scope="col">Action</th></tr></thead>
      <tbody id="profiles"><tr><td colspan="5" class="muted">Loading</td></tr></tbody>
    </table>
  </section>
</main>
<script>
let current = {};
let apiPin = '';
const $ = id => document.getElementById(id);
const esc = v => String(v ?? '').replace(/[&<>"']/g, c => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));

async function api(payload) {
  const body = Object.assign({v:true}, apiPin ? {pin:apiPin} : {}, payload ? {OneWireAccessory:payload} : {});
  const res = await fetch('/json/state', {
    method: payload ? 'POST' : 'GET',
    headers: payload ? {'Content-Type':'application/json'} : {},
    body: payload ? JSON.stringify(body) : undefined,
    cache: 'no-store'
  });
  if (!res.ok) throw new Error('HTTP ' + res.status);
  return await res.json();
}

function owFrom(json) {
  return json.OneWireAccessory || {};
}

function batteryText(d) {
  if (!d.battery) return '';
  const b = d.battery;
  const bits = [];
  if (b.percent !== undefined) bits.push(b.percent + '%');
  if (b.mv !== undefined) bits.push((b.mv / 1000).toFixed(2) + ' V');
  if (b.charging) bits.push('charging');
  return bits.join(', ');
}

function pickProfileForRom(rom) {
  return (current.profiles || []).find(p => p.rom === rom);
}

function setFormDefaults() {
  $('name').value = 'Shade';
  $('ledCount').value = 1;
  $('buttonIndex').value = 0;
  $('ledmap').value = -1;
  $('inputMode').value = 'inherit';
}

function fillFormFromRom(rom) {
  const p = pickProfileForRom(rom);
  if (!p) {
    setFormDefaults();
    return;
  }
  $('name').value = p.name || 'Shade';
  $('ledCount').value = p.ledCount || 1;
  $('buttonIndex').value = p.buttonIndex ?? 0;
  $('ledmap').value = p.ledmap ?? -1;
  $('inputMode').value = p.inputMode || (p.touch ? 'touch' : 'disabled');
}

function render(ow) {
  current = ow;
  const previousRom = $('rom').value;
  const devices = ow.deviceDetails || (ow.devices || []).map(rom => ({rom}));
  const profiles = ow.profiles || [];
  const active = ow.activeProfile || '';
  const statusClass = ow.busHealth === 'devices' ? 'ok' : 'warn';
  const overflow = ow.deviceOverflow ? ' - overflow' : '';
  const locked = ow.writeLocked ? ' - locked' : '';
  $('status').innerHTML = `<span class="${statusClass}">${ow.deviceCount || 0} device(s)</span> - ${esc(ow.busHealth || 'unknown')}${overflow}${locked} - ${esc(active || 'no active profile')} - ${esc(ow.applyStatus || 'idle')} - ${esc(ow.inputStatus || 'idle')} - pullup ${ow.internalPullup ? 'on' : 'off'}`;

  const limits = ow.limits || {};
  $('ledCount').max = limits.maxLedsPerBus || 2048;
  $('buttonIndex').max = Math.max(0, (limits.maxButtons || 1) - 1);
  $('ledmap').max = Math.max(0, (limits.maxLedmaps || 1) - 1);

  $('devices').innerHTML = devices.length ? devices.map(d => {
    const profile = d.profile || '';
    return `<tr>
      <td class="mono" title="${esc(d.rom)}">${esc(d.rom)}</td>
      <td>${esc(d.familyName || d.family || '')}</td>
      <td>${esc(d.kindName || d.kind || '')}</td>
      <td>${esc(profile)}</td>
      <td>${esc(batteryText(d))}</td>
    </tr>`;
  }).join('') : '<tr><td colspan="5" class="muted">No devices</td></tr>';

  $('profiles').innerHTML = profiles.length ? profiles.map(p => `<tr>
    <td class="mono" title="${esc(p.rom)}">${esc(p.rom)}</td>
    <td>${esc(p.name)}</td>
    <td>${esc(p.ledCount)}</td>
    <td>${esc((p.inputMode || 'inherit') + ' / B' + (p.buttonIndex ?? 0) + ' / M' + (p.ledmap ?? -1))}</td>
    <td><button class="danger" data-del="${esc(p.rom)}">Delete</button></td>
  </tr>`).join('') : '<tr><td colspan="5" class="muted">No profiles</td></tr>';

  const roms = Array.from(new Set([...(devices.map(d => d.rom).filter(Boolean)), ...(profiles.map(p => p.rom).filter(Boolean))]));
  $('rom').innerHTML = roms.length ? roms.map(rom => `<option value="${esc(rom)}">${esc(rom)}</option>`).join('') : '<option value="">No ROM</option>';
  if (roms.length) {
    $('rom').value = roms.includes(previousRom) ? previousRom : roms[0];
    fillFormFromRom($('rom').value);
    $('save').disabled = false;
  } else {
    setFormDefaults();
    $('save').disabled = true;
  }

  document.querySelectorAll('[data-del]').forEach(btn => btn.onclick = () => {
    if (confirm('Delete this shade profile?')) runAction({profileDelete:btn.dataset.del});
  });
}

async function runAction(payload) {
  $('status').textContent = 'Working';
  try {
    const response = owFrom(await api(payload));
    if (response.commandCode && response.commandCode !== 'ok') throw new Error(response.commandMessage || response.commandCode);
    await load();
  } catch (e) {
    $('status').innerHTML = `<span class="bad">${esc(e.message)}</span>`;
  }
}

async function load() {
  $('status').textContent = 'Loading';
  try {
    render(owFrom(await api()));
  } catch (e) {
    $('status').innerHTML = `<span class="bad">${esc(e.message)}</span>`;
  }
}

$('refresh').onclick = load;
$('pin').value = apiPin;
$('pin').oninput = () => {
  apiPin = $('pin').value.trim();
};
$('scan').onclick = () => runAction({scan:true});
$('apply').onclick = () => runAction({apply:true});
$('save').onclick = () => {
  const rom = $('rom').value;
  if (!rom) return;
  runAction({profileSet:{rom, name:$('name').value, ledCount:Number($('ledCount').value || 0), inputMode:$('inputMode').value, buttonIndex:Number($('buttonIndex').value || 0), ledmap:Number($('ledmap').value || -1)}});
};
$('rom').onchange = () => fillFormFromRom($('rom').value);
load();
</script>
</body>
</html>
)=====";

const char OneWireAccessoryBusUsermod::_name[]          PROGMEM = "OneWireAccessory";
const char OneWireAccessoryBusUsermod::_enabled[]       PROGMEM = "enabled";
const char OneWireAccessoryBusUsermod::_pin[]           PROGMEM = "pin";
const char OneWireAccessoryBusUsermod::_scanInterval[]  PROGMEM = "scanIntervalMs";
const char OneWireAccessoryBusUsermod::_stableScans[]   PROGMEM = "stableScans";
const char OneWireAccessoryBusUsermod::_smartFamily[]   PROGMEM = "smartFamilyCode";
const char OneWireAccessoryBusUsermod::_mqttPublish[]   PROGMEM = "mqttPublish";
const char OneWireAccessoryBusUsermod::_mqttHomeAssistant[] PROGMEM = "mqttHomeAssistant";
const char OneWireAccessoryBusUsermod::_mqttInterval[]  PROGMEM = "mqttIntervalMs";
const char OneWireAccessoryBusUsermod::_debugEnabled[]  PROGMEM = "debugEnabled";
const char OneWireAccessoryBusUsermod::_internalPullup[] PROGMEM = "internalPullup";
const char OneWireAccessoryBusUsermod::_allowLegacyTelemetry[] PROGMEM = "allowLegacyTelemetry";
const char OneWireAccessoryBusUsermod::_autoEnrollSmartShades[] PROGMEM = "autoEnrollSmartShades";
const char OneWireAccessoryBusUsermod::_autoUpdateSmartProfiles[] PROGMEM = "autoUpdateSmartProfiles";
const char OneWireAccessoryBusUsermod::_profiles[]      PROGMEM = "profiles";
const char OneWireAccessoryBusUsermod::_rom[]           PROGMEM = "rom";
const char OneWireAccessoryBusUsermod::_ledCount[]      PROGMEM = "ledCount";
const char OneWireAccessoryBusUsermod::_ledmap[]        PROGMEM = "ledmap";
const char OneWireAccessoryBusUsermod::_touch[]         PROGMEM = "touch";
const char OneWireAccessoryBusUsermod::_inputMode[]     PROGMEM = "inputMode";
const char OneWireAccessoryBusUsermod::_buttonIndex[]   PROGMEM = "buttonIndex";
const char OneWireAccessoryBusUsermod::_blockNoTouch[]  PROGMEM = "blockButtonWhenNoTouch";
const char OneWireAccessoryBusUsermod::_autoApply[]     PROGMEM = "autoApplyProfile";
const char OneWireAccessoryBusUsermod::_fallbackLedCount[] PROGMEM = "fallbackLedCount";
const char OneWireAccessoryBusUsermod::_fallbackLedmap[] PROGMEM = "fallbackLedmap";
const char OneWireAccessoryBusUsermod::_profileSet[]    PROGMEM = "profileSet";
const char OneWireAccessoryBusUsermod::_profileDelete[] PROGMEM = "profileDelete";

static OneWireAccessoryBusUsermod oneWireAccessoryBusUsermod;
REGISTER_USERMOD(oneWireAccessoryBusUsermod);
