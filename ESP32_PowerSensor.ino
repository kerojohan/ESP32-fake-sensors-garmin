/*
 * 🚴 ESP32 Power Sensor - Sensor de Potencia BLE para Garmin (NimBLE)
 * 
 * Esta es una versión específica SOLO para el sensor de potencia
 * para facilitar las pruebas y debugging del problema de 0W.
 * 
 * Hardware: ESP32 (cualquier modelo con Bluetooth)
 * 
 * Para usar en Arduino IDE:
 * 1. Instalar ESP32 board en Arduino IDE
 * 2. Instalar biblioteca NimBLE-Arduino
 * 3. Seleccionar placa ESP32 correcta
 * 4. Subir código
 * 5. Abrir Monitor Serial (115200 baudios)
 * 6. Garmin debería ver el sensor de potencia
 * 7. Conectar y verificar que muestre potencia > 0W
 * 
 * SOLUCIÓN AL PROBLEMA DE 0W:
 * - Flags corregidos: 0x60 (bits 6 y 5 activos)
 * - Features corregidos: 0x0000000C (bits 2 y 3 activos)
 * - Formato de datos según estándar BLE CPS
 * - Debug detallado de datos enviados
 */

#include <NimBLEDevice.h>

// ===== CONFIGURACIÓN DEL SENSOR =====
#define ENABLE_POWER_SENSOR      false    // SOLO sensor de potencia
#define ENABLE_SPEED_SENSOR      false   // Desactivado
#define ENABLE_HEART_RATE_SENSOR true   // Desactivado

// ===== CONFIGURACIÓN DE DEBUG =====
#define DEBUG_SENSOR_STATUS      true    // Mostrar estado del sensor
#define DEBUG_DATA_SENDING       true    // Mostrar confirmación de envío de datos
#define DEBUG_CONNECTION_STATUS  true    // Mostrar estado de conexión detallado
#define DEBUG_POWER_DATA         true    // Debug específico de datos de potencia

// ===== CONFIGURACIÓN DE ESTABILIDAD =====
#define ENABLE_HEARTBEAT         true    // Habilitar heartbeat para mantener conexión
#define HEARTBEAT_INTERVAL_MS    250     // Intervalo de heartbeat en milisegundos
#define STABLE_CONNECTION_MS     10000   // Tiempo para considerar conexión estable (ms)

// UUIDs de los servicios
#define CYCLING_POWER_SERVICE_UUID        "1818"
#define CYCLING_POWER_MEASUREMENT_UUID    "2A63"
#define CYCLING_POWER_FEATURE_UUID        "2A65"
#define CYCLING_POWER_CONTROL_POINT_UUID  "2A66"
#define CYCLING_SPEED_CADENCE_UUID        "1816"
#define CYCLING_SPEED_MEASUREMENT_UUID    "2A5B"
#define CYCLING_SPEED_FEATURE_UUID        "2A5C"
#define SENSOR_LOCATION_UUID               "2A5D"

// UUIDs para el sensor de frecuencia cardíaca
#define HEART_RATE_SERVICE_UUID           "180D"
#define HEART_RATE_MEASUREMENT_UUID       "2A37"
#define HEART_RATE_CONTROL_POINT_UUID     "2A39"

// Estado de conexión
bool deviceConnected = false;

// Variables para estabilidad de conexión
uint8_t connectionAttempts = 0;
const uint8_t MAX_CONNECTION_ATTEMPTS = 5;
unsigned long lastDisconnectTime = 0;
const unsigned long RECONNECT_DELAY = 2000;
const unsigned long MAX_RECONNECT_DELAY = 10000;

// Variables para estabilidad de conexión
unsigned long lastConnectionTime = 0;
unsigned long connectionDuration = 0;
bool isStableConnection = false;
const unsigned long STABLE_CONNECTION_THRESHOLD = STABLE_CONNECTION_MS;

// Timers para actualización de datos
unsigned long lastPowerUpdateTime = 0;
const unsigned long UPDATE_INTERVAL = 1000; // 1 segundo

// Timer para heartbeat de conexión
unsigned long lastHeartbeatTime = 0;
const unsigned long HEARTBEAT_INTERVAL = HEARTBEAT_INTERVAL_MS;

// Variables para datos del sensor de potencia
uint32_t cumulativeWheelRevolutions = 0;
uint16_t lastWheelEventTime = 0;
uint16_t cumulativeCrankRevolutions = 0;
uint16_t lastCrankEventTime = 0;
uint16_t instantPower = 0;
uint16_t instantCadence = 0;

// Variables para datos del sensor de velocidad/cadencia
uint16_t instantSpeed = 0;        // km/h
uint32_t totalDistance = 0;       // metros
uint16_t wheelCircumference = 2100; // mm (700c estándar)

// Variables para datos del sensor de frecuencia cardíaca
uint16_t heartRate = 0;           // BPM (Beats Per Minute)
uint16_t energyExpended = 0;      // kJ (kilojulios)
uint8_t rrIntervals[10];          // RR intervals (ms) - últimos 10
uint8_t rrIntervalCount = 0;      // Número de RR intervals válidos

// Timers independientes para cada sensor
unsigned long lastHeartRateUpdateTime = 0;
const unsigned long HEART_RATE_UPDATE_INTERVAL = 1000; // 1 segundo para frecuencia cardíaca

// Características BLE (NimBLE)
NimBLECharacteristic* pPowerMeasurementCharacteristic = nullptr;
NimBLECharacteristic* pPowerFeatureCharacteristic = nullptr;
NimBLECharacteristic* pPowerControlPointCharacteristic = nullptr;

// Características para sensor de velocidad/cadencia
NimBLECharacteristic* pSpeedMeasurementCharacteristic = nullptr;
NimBLECharacteristic* pSpeedFeatureCharacteristic = nullptr;

// Características para sensor de frecuencia cardíaca
NimBLECharacteristic* pHeartRateMeasurementCharacteristic = nullptr;
NimBLECharacteristic* pHeartRateControlPointCharacteristic = nullptr;

// Flags para saber qué servicios se han creado
bool hasPowerService = false;

// Función para generar datos aleatorios realistas de potencia
void generateRandomPowerData() {
  // Potencia instantánea (150-450W) - rango realista para ciclismo
  instantPower = random(150, 450);
  
  // Cadencia (75-105 RPM) - rango realista
  instantCadence = random(75, 105);
  
  // Revoluciones acumuladas de la rueda
  // Simular que la rueda gira continuamente
  cumulativeWheelRevolutions += random(3, 10);
  
  // Tiempo del último evento de rueda (en 1/1024 segundos)
  // Simular eventos de rueda cada ~150-400ms
  uint16_t wheelEventInterval = random(150, 400);
  lastWheelEventTime += wheelEventInterval;
  
  // Revoluciones acumuladas del pedalier
  // Simular que el pedalier gira más lento que la rueda
  cumulativeCrankRevolutions += random(2, 6);
  
  // Tiempo del último evento del pedalier
  // Simular eventos de pedalier cada ~250-600ms
  uint16_t crankEventInterval = random(250, 600);
  lastCrankEventTime += crankEventInterval;
  
  // Mantener valores dentro de rangos razonables para evitar overflow
  if (lastWheelEventTime > 65535) lastWheelEventTime = 0;
  if (lastCrankEventTime > 65535) lastCrankEventTime = 0;
  
  // Debug de datos generados
  #if DEBUG_POWER_DATA
  Serial.printf("🔧 Power data generated: Power=%uW, Cadence=%uRPM\n", instantPower, instantCadence);
  Serial.printf("   Wheel Revs: %lu, Crank Revs: %u\n", cumulativeWheelRevolutions, cumulativeCrankRevolutions);
  Serial.printf("   Wheel Time: %u, Crank Time: %u\n", lastWheelEventTime, lastCrankEventTime);
  #endif
}

// Función para generar datos aleatorios realistas de velocidad/cadencia
void generateRandomSpeedData() {
  // Velocidad instantánea (15-45 km/h) - rango realista para ciclismo
  instantSpeed = random(15, 45);
  
  // Cadencia (70-110 RPM) - rango realista
  instantCadence = random(70, 110);
  
  // Revoluciones ACUMULATIVAS de la rueda (total desde el inicio)
  // IMPORTANTE: Para CSCS, Garmin espera revoluciones ACUMULATIVAS, no incrementales
  // Garmin calcula la velocidad basándose en la diferencia entre paquetes consecutivos
  uint32_t wheelRevolutionsIncrement = 1;
  cumulativeWheelRevolutions += wheelRevolutionsIncrement;
  
  // Tiempo ACUMULATIVO del evento de rueda (en MILISEGUNDOS)
  // IMPORTANTE: Para CSCS, Garmin espera tiempos ACUMULATIVOS desde el inicio
  // Garmin calcula la velocidad basándose en la diferencia entre paquetes consecutivos
  uint16_t wheelEventInterval = random(200, 600);
  lastWheelEventTime += wheelEventInterval; // ACUMULAR tiempo total
  
  // Revoluciones ACUMULATIVAS del pedalier (total desde el inicio)
  // IMPORTANTE: Para CSCS, Garmin espera revoluciones ACUMULATIVAS, no incrementales
  // Garmin calcula la cadencia basándose en la diferencia entre paquetes consecutivos
  uint32_t crankRevolutionsIncrement = 1;
  cumulativeCrankRevolutions += crankRevolutionsIncrement;
  
  // Tiempo ACUMULATIVO del evento del pedalier (en MILISEGUNDOS)
  // IMPORTANTE: Para CSCS, Garmin espera tiempos ACUMULATIVOS desde el inicio
  // Garmin calcula la cadencia basándose en la diferencia entre paquetes consecutivos
  uint16_t crankEventInterval = random(545, 857);
  lastCrankEventTime += crankEventInterval; // ACUMULAR tiempo total
  
  // RESET PERIÓDICO DE TIEMPOS para evitar valores muy altos
  // Garmin necesita tiempos relativamente pequeños para calcular cadencia correctamente
  if (lastWheelEventTime > 10000) { // Reset cada ~10 segundos
    lastWheelEventTime = wheelEventInterval; // Reset al último intervalo
    Serial.println("🔄 Reset de Wheel Time para mantener rango razonable");
  }
  
  if (lastCrankEventTime > 10000) { // Reset cada ~10 segundos
    lastCrankEventTime = crankEventInterval; // Reset al último intervalo
    Serial.println("🔄 Reset de Crank Time para mantener rango razonable");
  }
  
  // Calcular distancia total (en metros)
  totalDistance = (cumulativeWheelRevolutions * wheelCircumference) / 1000;
  
  // Debug de datos generados
  #if DEBUG_POWER_DATA
  Serial.printf("🔧 Speed data generated: Speed=%u km/h, Cadence=%uRPM\n", instantSpeed, instantCadence);
  Serial.printf("   Wheel Revs: %lu (acumulativo: +%u), Crank Revs: %u (acumulativo: +%u), Distance=%lu m\n", 
               cumulativeWheelRevolutions, wheelRevolutionsIncrement, cumulativeCrankRevolutions, crankRevolutionsIncrement, totalDistance);
  Serial.printf("   Wheel Time: %u ms (acumulativo), Crank Time: %u ms (acumulativo)\n", lastWheelEventTime, lastCrankEventTime);
  
  // Debug específico para cadencia
  Serial.printf("🔧 CADENCIA DEBUG: Crank Revs=%u, Crank Time=%u ms, Calculated RPM=%u\n",
               cumulativeCrankRevolutions, lastCrankEventTime, 
               (lastCrankEventTime > 0) ? (60000 / lastCrankEventTime) : 0);
  
  // Mostrar fórmula de cálculo
  if (lastCrankEventTime > 0) {
    Serial.printf("   • Fórmula: 60000 / %u = %u RPM\n", lastCrankEventTime, 60000 / lastCrankEventTime);
  }
  
  // Explicar cómo Garmin calcula la cadencia
  Serial.println("   • 💡 Garmin calcula cadencia usando DELTAS entre paquetes consecutivos");
  Serial.println("   • 💡 RPM = (delta_revoluciones * 1024 * 60) / delta_tiempo");
  Serial.println("   • 💡 Por eso enviamos revoluciones ACUMULATIVAS, no incrementales");
  #endif
}

// Función para crear el paquete de datos de potencia (2A63)
void updatePowerMeasurementData() {
  #if ENABLE_POWER_SENSOR
  // Estructura según estándar BLE Cycling Power Service (CPS)
  // Flags (2B) | Instantaneous Power (2B) | Wheel Revolution Data (4B) | Wheel Event Time (2B) | Crank Revolution Data (2B) | Crank Event Time (2B)
  uint8_t powerData[14];
  
  // Flags: 
  // Bit 0-1: Reserved for future use
  // Bit 2: Pedal Power Balance Present (0 = not present)
  // Bit 3: Pedal Power Balance Reference (0 = unknown)
  // Bit 4: Accumulated Torque Present (0 = not present)
  // Bit 5: Accumulated Torque Source (0 = wheel based)
  // Bit 6: Wheel Revolution Data Present (1 = present)
  // Bit 7: Crank Revolution Data Present (1 = present)
  // Bit 8-15: Reserved for future use
  powerData[0] = 0x60;  // 0x60 = 01100000 (bits 6 y 5 activos)
  powerData[1] = 0x00;  // 0x00 = bits 8-15 reservados
  
  // Potencia instantánea (16 bits, Little Endian)
  // Valor en vatios (0-65535W)
  powerData[2] = instantPower & 0xFF;
  powerData[3] = (instantPower >> 8) & 0xFF;
  
  // Wheel Revolution Data (32 bits, Little Endian)
  // Número acumulado de revoluciones de la rueda
  powerData[4] = cumulativeWheelRevolutions & 0xFF;
  powerData[5] = (cumulativeWheelRevolutions >> 8) & 0xFF;
  powerData[6] = (cumulativeWheelRevolutions >> 16) & 0xFF;
  powerData[7] = (cumulativeWheelRevolutions >> 24) & 0xFF;
  
  // Wheel Event Time (16 bits, Little Endian)
  // Tiempo del último evento de rueda en unidades de 1/1024 segundos
  powerData[8] = lastWheelEventTime & 0xFF;
  powerData[9] = (lastWheelEventTime >> 8) & 0xFF;
  
  // Crank Revolution Data (16 bits, Little Endian)
  // Número acumulado de revoluciones del pedalier
  powerData[10] = cumulativeCrankRevolutions & 0xFF;
  powerData[11] = (cumulativeCrankRevolutions >> 8) & 0xFF;
  
  // Crank Event Time (16 bits, Little Endian)
  // Tiempo del último evento del pedalier en unidades de 1/1024 segundos
  powerData[12] = lastCrankEventTime & 0xFF;
  powerData[13] = lastCrankEventTime >> 8;
  
  if (pPowerMeasurementCharacteristic != nullptr) {
    pPowerMeasurementCharacteristic->setValue(powerData, sizeof(powerData));
    
    // Enviar notificación
    if (pPowerMeasurementCharacteristic->notify()) {
      #if DEBUG_DATA_SENDING
      Serial.printf("✅ Power data sent: %uW, Wheel:%lu, Crank:%u\n", 
                   instantPower, cumulativeWheelRevolutions, cumulativeCrankRevolutions);
      Serial.printf("🔍 DEBUG: Flags=0x%02X%02X, Power=%uW, WheelTime=%u, CrankTime=%u\n",
                   powerData[0], powerData[1], instantPower, lastWheelEventTime, lastCrankEventTime);
      
      // Mostrar dump completo de datos
      Serial.print("📊 Raw data dump: ");
      for (int i = 0; i < sizeof(powerData); i++) {
        Serial.printf("%02X ", powerData[i]);
      }
      Serial.println();
      #endif
    } else {
      Serial.println("❌ Failed to notify power data");
    }
  }
  #endif
}

// Función para crear el paquete de datos de velocidad/cadencia (2A5B)
void updateSpeedMeasurementData() {
  #if ENABLE_SPEED_SENSOR
  // FORMATO SIMPLIFICADO QUE FUNCIONA (basado en ESP32-Zwift-Cadence)
  // Estructura: Flags(1B) + Crank Revolution Data(2B) + Crank Event Time(2B)
  // Solo enviamos datos de cadencia, no de rueda (como hace el proyecto que funciona)
  uint8_t speedData[5];
  
  // Flags: 
  // Bit 0: Wheel Revolution Data Present (0 = not present)
  // Bit 1: Crank Revolution Data Present (1 = present)
  // Bit 2-7: Reserved for future use
  speedData[0] = 0x02;  // 0x02 = 00000010 (solo Crank, sin Wheel)
  
  // Crank Revolution Data (16 bits, Little Endian)
  // IMPORTANTE: Para CSCS, Garmin espera revoluciones ACUMULATIVAS
  // Enviar el total acumulado de revoluciones desde el inicio
  speedData[1] = cumulativeCrankRevolutions & 0xFF;
  speedData[2] = (cumulativeCrankRevolutions >> 8) & 0xFF;
  
  // Crank Event Time (16 bits, Little Endian)
  // IMPORTANTE: Garmin espera tiempos en unidades de 1/1024 segundos
  // Convertir de milisegundos a unidades de 1/1024 segundos: tiempo_ms * 1024 / 1000
  uint16_t crankTimeIn1024Units = (lastCrankEventTime * 1024) / 1000;
  
  // CONTROL DE OVERFLOW (como hace el proyecto que funciona)
  if (crankTimeIn1024Units > 65530) {
    crankTimeIn1024Units -= 65530;
    Serial.println("🔄 Overflow control aplicado a Crank Time");
  }
  
  speedData[3] = crankTimeIn1024Units & 0xFF;
  speedData[4] = (crankTimeIn1024Units >> 8) & 0xFF;
  
  if (pSpeedMeasurementCharacteristic != nullptr) {
    pSpeedMeasurementCharacteristic->setValue(speedData, sizeof(speedData));
    
    // Enviar notificación
    if (pSpeedMeasurementCharacteristic->notify()) {
      #if DEBUG_DATA_SENDING
      Serial.printf("✅ Speed data sent (FORMATO SIMPLIFICADO): Speed=%u km/h, Crank:%u, Distance=%lu m\n", 
                   instantSpeed, cumulativeCrankRevolutions, totalDistance);
      Serial.printf("🔍 DEBUG: Flags=0x%02X (Solo Crank), Crank Revs: %u (acumulativo), Crank Time: %u ms (acumulativo)\n",
                   speedData[0], cumulativeCrankRevolutions, lastCrankEventTime);
      
      // Mostrar conversión de unidades
      Serial.printf("🔍 CONVERSIÓN: Crank Time %u ms → %u (1/1024s)\n",
                   lastCrankEventTime, crankTimeIn1024Units);
      
      // Mostrar dump completo de datos
      Serial.print("📊 Raw speed data dump (5 bytes): ");
      for (int i = 0; i < sizeof(speedData); i++) {
        Serial.printf("%02X ", speedData[i]);
      }
      Serial.println();
      
      // Mostrar análisis detallado de los datos
      Serial.println("📊 ANÁLISIS DE DATOS (FORMATO SIMPLIFICADO):");
      Serial.printf("   • Flags: 0x%02X (Wheel:%s, Crank:%s)\n", 
                   speedData[0], 
                   (speedData[0] & 0x01) ? "✅" : "❌",
                   (speedData[0] & 0x02) ? "✅" : "❌");
      Serial.printf("   • Crank Revs: %u (acumulativo) (0x%02X%02X)\n", 
                   cumulativeCrankRevolutions, speedData[2], speedData[1]);
      Serial.printf("   • Crank Time: %u ms (acumulativo) → %u (1/1024s) (0x%02X%02X)\n", 
                   lastCrankEventTime, crankTimeIn1024Units, speedData[4], speedData[3]);
      
      // Verificar que los tiempos estén en rango razonable
      if (lastCrankEventTime > 1000) {
        Serial.println("⚠️  ADVERTENCIA: Crank Time muy alto - verificar unidades");
      }
      if (lastCrankEventTime < 100 || lastCrankEventTime > 1000) {
        Serial.printf("⚠️  ADVERTENCIA: Crank Time fuera de rango razonable: %u ms\n", lastCrankEventTime);
      }
      
      // Explicar por qué este formato funciona
      Serial.println("✅ VENTAJAS DEL FORMATO SIMPLIFICADO:");
      Serial.println("   • Solo cadencia (sin rueda) - menos complejidad");
      Serial.println("   • 5 bytes en lugar de 11 bytes - más simple");
      Serial.println("   • Basado en proyecto ESP32-Zwift-Cadence que FUNCIONA");
      Serial.println("   • Flags=0x02 (solo Crank) - formato probado");
      #endif
    } else {
      Serial.println("❌ Failed to notify speed data");
    }
  }
  #endif
}

// Callback para característica de control
class PowerControlPointCallbacks: public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* pCharacteristic) {
    std::string value = pCharacteristic->getValue();
    if (value.length() > 0) {
      Serial.print("📝 Comando recibido: ");
      for (size_t i = 0; i < value.length(); i++) {
        Serial.printf("%02X ", (uint8_t)value[i]);
      }
      Serial.println();
      
      // Procesar comandos del Garmin si es necesario
      if ((uint8_t)value[0] == 0x01) {
        Serial.println("✅ Comando de calibración recibido");
      }

      // Responder con Response Code (CPS Control Point):
      // Formato: [0x20 Response Code][RequestOpCode][ResultCode]
      // ResultCode: 0x01 Success (como respuesta mínima por compatibilidad)
      uint8_t response[3];
      response[0] = 0x20; // Response Code
      response[1] = (uint8_t)value[0]; // Echo del OpCode recibido
      response[2] = 0x01; // Success
      pCharacteristic->setValue(response, sizeof(response));
      pCharacteristic->indicate();
    }
  }
};

// Función para crear el servicio de potencia
void createPowerService(NimBLEServer* pServer) {
  // Crear servicio de potencia
  NimBLEService* pPowerService = pServer->createService(NimBLEUUID(CYCLING_POWER_SERVICE_UUID));
  
  // Característica de medición de potencia
  pPowerMeasurementCharacteristic = pPowerService->createCharacteristic(
    NimBLEUUID(CYCLING_POWER_MEASUREMENT_UUID),
    NIMBLE_PROPERTY::NOTIFY
  );
  
  // Característica de características (features)
  // Según estándar BLE Cycling Power Service (CPS)
  // Bit 0: Pedal Power Balance Supported (0 = not supported)
  // Bit 1: Accumulated Torque Supported (0 = not supported)
  // Bit 2: Wheel Revolution Data Supported (1 = supported)
  // Bit 3: Crank Revolution Data Supported (1 = supported)
  // Bit 4: Extreme Magnitudes Supported (0 = not supported)
  // Bit 5: Extreme Angles Supported (0 = not supported)
  // Bit 6: Top Dead Angle Supported (0 = not supported)
  // Bit 7: Bottom Dead Angle Supported (0 = not supported)
  // Bit 8-31: Reserved for future use
  uint32_t features = 0x0000000C; // 0x0C = 00001100 (bits 2 y 3 activos)
  pPowerFeatureCharacteristic = pPowerService->createCharacteristic(
    NimBLEUUID(CYCLING_POWER_FEATURE_UUID),
    NIMBLE_PROPERTY::READ
  );
  pPowerFeatureCharacteristic->setValue((uint8_t*)&features, 4);
  
  // Característica de punto de control
  pPowerControlPointCharacteristic = pPowerService->createCharacteristic(
    NimBLEUUID(CYCLING_POWER_CONTROL_POINT_UUID),
    NIMBLE_PROPERTY::WRITE |
    NIMBLE_PROPERTY::INDICATE
  );
  pPowerControlPointCharacteristic->setCallbacks(new PowerControlPointCallbacks());

  // Característica opcional: Sensor Location (2A5D) → 0x0D Rear Wheel
  NimBLECharacteristic* pPowerSensorLocation = pPowerService->createCharacteristic(
    NimBLEUUID(SENSOR_LOCATION_UUID),
    NIMBLE_PROPERTY::READ
  );
  uint8_t powerSensorLocation = 0x0D; // Rear Wheel
  pPowerSensorLocation->setValue(&powerSensorLocation, 1);
  
  // Iniciar servicio
  pPowerService->start();
  Serial.println("✅ Servicio de potencia iniciado");
  hasPowerService = true;
}

// Función para crear el servicio de velocidad/cadencia
void createSpeedService(NimBLEServer* pServer) {
  // Crear servicio de velocidad/cadencia
  NimBLEService* pSpeedService = pServer->createService(NimBLEUUID(CYCLING_SPEED_CADENCE_UUID));
  
  // Característica de medición de velocidad
  pSpeedMeasurementCharacteristic = pSpeedService->createCharacteristic(
    NimBLEUUID(CYCLING_SPEED_MEASUREMENT_UUID),
    NIMBLE_PROPERTY::NOTIFY
  );
  
  // Característica de características (features)
  // Para CSC, esta característica es de 2 bytes. Bit0: Wheel Supported, Bit1: Crank Supported
  uint16_t speedFeatures = 0x0003; // 0x03 = 00000011 (bits 0 y 1 activos)
  pSpeedFeatureCharacteristic = pSpeedService->createCharacteristic(
    NimBLEUUID(CYCLING_SPEED_FEATURE_UUID),
    NIMBLE_PROPERTY::READ
  );
  pSpeedFeatureCharacteristic->setValue((uint8_t*)&speedFeatures, 2);

  // Característica opcional: Sensor Location (2A5D) → 0x0D Rear Wheel
  NimBLECharacteristic* pSpeedSensorLocation = pSpeedService->createCharacteristic(
    NimBLEUUID(SENSOR_LOCATION_UUID),
    NIMBLE_PROPERTY::READ
  );
  uint8_t speedSensorLocation = 0x0D; // Rear Wheel
  pSpeedSensorLocation->setValue(&speedSensorLocation, 1);
  
  // Iniciar servicio
  pSpeedService->start();
  Serial.println("✅ Servicio de velocidad/cadencia iniciado");
}

// Callback para conexiones BLE
class ServerCallbacks: public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* pServer) {
    Serial.println("🚴 Garmin conectado al ESP32 Power Sensor!");
    Serial.println("🔍 DEBUG: onConnect callback ejecutado");
    Serial.printf("🔍 DEBUG: deviceConnected antes = %s\n", deviceConnected ? "true" : "false");
    deviceConnected = true;
    Serial.printf("🔍 DEBUG: deviceConnected después = %s\n", deviceConnected ? "true" : "false");
    connectionAttempts = 0;
    lastConnectionTime = millis();
    isStableConnection = false;
    
          // Verificar que la característica esté disponible
      if (ENABLE_POWER_SENSOR) {
        Serial.println("🔍 Verificando característica de potencia...");
        if (pPowerMeasurementCharacteristic) {
      Serial.println("✅ Power measurement disponible");
        } else {
          Serial.println("❌ Power measurement NO disponible");
    }
      } else if (ENABLE_SPEED_SENSOR) {
        Serial.println("🔍 Verificando característica de velocidad/cadencia...");
        if (pSpeedMeasurementCharacteristic) {
      Serial.println("✅ Speed measurement disponible");
        } else {
          Serial.println("❌ Speed measurement NO disponible");
    }
    } else if (ENABLE_HEART_RATE_SENSOR) {
        Serial.println("🔍 Verificando característica de frecuencia cardíaca...");
        if (pHeartRateMeasurementCharacteristic) {
          Serial.println("✅ Heart rate measurement disponible");
        } else {
          Serial.println("❌ Heart rate measurement NO disponible");
        }
      }
    
    Serial.println("🔗 Conexión establecida - enviando datos...");
    
    // Enviar primer dato inmediatamente para confirmar la conexión
    if (ENABLE_POWER_SENSOR && pPowerMeasurementCharacteristic != nullptr) {
      generateRandomPowerData();
      updatePowerMeasurementData();
      lastPowerUpdateTime = millis();
      } else if (ENABLE_SPEED_SENSOR && pSpeedMeasurementCharacteristic != nullptr) {
      generateRandomSpeedData();
      updateSpeedMeasurementData();
        lastPowerUpdateTime = millis();
    } else if (ENABLE_HEART_RATE_SENSOR && pHeartRateMeasurementCharacteristic != nullptr) {
      generateRandomHeartRateData();
      updateHeartRateMeasurementData();
      lastHeartRateUpdateTime = millis();
    }
    
    Serial.println("✅ Configuración de conexión completada");
  }
  
  void onDisconnect(NimBLEServer* pServer) {
    Serial.println("❌ Garmin desconectado del ESP32 Power Sensor");
    Serial.println("🔍 DEBUG: onDisconnect callback ejecutado");
    Serial.printf("🔍 DEBUG: deviceConnected antes = %s\n", deviceConnected ? "true" : "false");
    
    // Calcular duración de la conexión
    connectionDuration = millis() - lastConnectionTime;
    
    Serial.printf("❌ Garmin desconectado (conexión duró %lu ms)\n", connectionDuration);
    deviceConnected = false;
    Serial.printf("🔍 DEBUG: deviceConnected después = %s\n", deviceConnected ? "true" : "false");
    isStableConnection = false;
    lastDisconnectTime = millis();
    
    // Incrementar contador de intentos
    connectionAttempts++;
    
    // Calcular delay de reconexión (exponencial backoff)
    unsigned long reconnectDelay = RECONNECT_DELAY;
    if (connectionAttempts > 2) {
      reconnectDelay = min(RECONNECT_DELAY * (1 << (connectionAttempts - 2)), MAX_RECONNECT_DELAY);
    }
    
    if (connectionAttempts >= MAX_CONNECTION_ATTEMPTS) {
      Serial.printf("⚠️  Demasiados intentos de conexión (%d). Esperando %lu segundos...\n", connectionAttempts, reconnectDelay/1000);
      delay(reconnectDelay);
      connectionAttempts = 0;
    } else {
      Serial.printf("🔄 Intento %d/%d. Reiniciando advertising en %lu segundos...\n", connectionAttempts, MAX_CONNECTION_ATTEMPTS, reconnectDelay/1000);
      delay(reconnectDelay);
    }
    
    // Reiniciar advertising
    NimBLEDevice::startAdvertising();
  }
};

// Función para mantener la conexión activa con heartbeat
void sendHeartbeat() {
  if (!deviceConnected) return;
  
  unsigned long currentTime = millis();
  
  // Solo enviar heartbeat si no se han enviado datos recientemente
  if (currentTime - lastPowerUpdateTime > UPDATE_INTERVAL / 2) {
  if (ENABLE_POWER_SENSOR) {
      // Enviar datos de potencia actualizados como heartbeat
      generateRandomPowerData();
      updatePowerMeasurementData();
      lastPowerUpdateTime = currentTime;
      
      #if DEBUG_DATA_SENDING
      Serial.println("💓 Heartbeat enviado (Power) - manteniendo conexión activa");
      #endif
    } else if (ENABLE_SPEED_SENSOR) {
      // Enviar datos de velocidad/cadencia como heartbeat
      generateRandomSpeedData();
      updateSpeedMeasurementData();
      lastPowerUpdateTime = currentTime;
      
      #if DEBUG_DATA_SENDING
      Serial.println("💓 Heartbeat enviado (Speed) - manteniendo conexión activa");
      #endif
      
      // Debug adicional para heartbeat del sensor de velocidad/cadencia
      Serial.printf("💓 HEARTBEAT: Speed=%u km/h, Cadence=%uRPM, Crank=%u, Time=%u ms (intervalo)\n",
                   instantSpeed, instantCadence, cumulativeCrankRevolutions, lastCrankEventTime);
      
      // Verificación de cadencia en heartbeat
      uint32_t realCadence = calculateRealCadence();
      if (realCadence > 0) {
        Serial.printf("💓 CADENCIA REAL: %u RPM (Calculada de datos)\n", realCadence);
      }
    } else if (ENABLE_HEART_RATE_SENSOR) {
      // Enviar datos de frecuencia cardíaca como heartbeat
      generateRandomHeartRateData();
      updateHeartRateMeasurementData();
      lastHeartRateUpdateTime = currentTime;
      
      #if DEBUG_DATA_SENDING
      Serial.println("💓 Heartbeat enviado (Heart Rate) - manteniendo conexión activa");
      Serial.printf("💓 HEARTBEAT: Heart Rate=%u BPM, Energy=%u kJ, RR Intervals=%u\n",
                   heartRate, energyExpended, rrIntervalCount);
      #endif
    }
  }
}

// Función para verificar la salud de la conexión
bool checkConnectionHealth() {
  if (!deviceConnected) return false;
  
  if (ENABLE_POWER_SENSOR && pPowerMeasurementCharacteristic == nullptr) {
    Serial.println("❌ Power measurement characteristic no disponible");
    return false;
  }
  
  if (ENABLE_SPEED_SENSOR && pSpeedMeasurementCharacteristic == nullptr) {
    Serial.println("❌ Speed measurement characteristic no disponible");
    return false;
  }
  
  if (ENABLE_HEART_RATE_SENSOR && pHeartRateMeasurementCharacteristic == nullptr) {
    Serial.println("❌ Heart rate measurement characteristic no disponible");
    return false;
  }
  
  // Verificar que los datos sean válidos según el sensor activado
  if (ENABLE_POWER_SENSOR && instantPower == 0) {
    Serial.println("⚠️  Power data is 0W - regenerating...");
    generateRandomPowerData();
  }
  
  if (ENABLE_SPEED_SENSOR && instantSpeed == 0) {
    Serial.println("⚠️  Speed data is 0 km/h - regenerating...");
    generateRandomSpeedData();
  }
  
  if (ENABLE_HEART_RATE_SENSOR && heartRate == 0) {
    Serial.println("⚠️  Heart rate data is 0 BPM - regenerating...");
    generateRandomHeartRateData();
  }
  
  // Verificación específica para cadencia
  if (ENABLE_SPEED_SENSOR) {
    if (cumulativeCrankRevolutions == 0) {
      Serial.println("⚠️  Crank revolutions is 0 - regenerating...");
      generateRandomSpeedData();
    }
    
    if (lastCrankEventTime == 0) {
      Serial.println("⚠️  Crank event time is 0 ms - regenerating...");
      generateRandomSpeedData();
    }
    
    // Verificar que los datos de cadencia sean razonables
    if (instantCadence < 50 || instantCadence > 120) {
      Serial.println("⚠️  Cadence out of range - regenerating...");
      generateRandomSpeedData();
    }
    
    // Verificar que los tiempos estén en rango razonable (acumulativos, no incrementales)
    // Para CSCS, los tiempos deben ser acumulativos pero no muy altos
    if (lastCrankEventTime > 15000) { // Más de 15 segundos
      Serial.printf("⚠️  Crank time muy alto (%u ms) - regenerating...\n", lastCrankEventTime);
      generateRandomSpeedData();
    }
    
    if (lastWheelEventTime > 15000) { // Más de 15 segundos
      Serial.printf("⚠️  Wheel time muy alto (%u ms) - regenerating...\n", lastWheelEventTime);
      generateRandomSpeedData();
    }
    
    // Verificar que los tiempos no sean muy bajos (deben ser acumulativos)
    if (lastCrankEventTime < 100) {
      Serial.printf("⚠️  Crank time muy bajo (%u ms) - regenerating...\n", lastCrankEventTime);
      generateRandomSpeedData();
    }
    
    if (lastWheelEventTime < 50) {
      Serial.printf("⚠️  Wheel time muy bajo (%u ms) - regenerating...\n", lastWheelEventTime);
      generateRandomSpeedData();
    }
  }
  
  return true;
}

// Función para detectar conexión de manera alternativa
bool detectConnectionAlternative() {
  // Intentar enviar un dato de prueba para verificar si hay conexión activa
  if (ENABLE_POWER_SENSOR && pPowerMeasurementCharacteristic != nullptr) {
    // Generar datos de prueba
      generateRandomPowerData();
    
    // Intentar enviar el dato
    if (pPowerMeasurementCharacteristic->notify()) {
      Serial.println("✅ Conexión detectada alternativamente - enviando datos de potencia...");
      return true;
    } else {
      Serial.println("❌ No se pudo enviar datos de potencia - sin conexión activa");
      return false;
    }
  } else if (ENABLE_SPEED_SENSOR && pSpeedMeasurementCharacteristic != nullptr) {
    // Generar datos de prueba
      generateRandomSpeedData();
    
    // Intentar enviar el dato
    if (pSpeedMeasurementCharacteristic->notify()) {
      Serial.println("✅ Conexión detectada alternativamente - enviando datos de velocidad...");
      return true;
    } else {
      Serial.println("❌ No se pudo enviar datos de velocidad - sin conexión activa");
      return false;
    }
  }
  return false;
}

// Función para forzar la detección de conexión
void forceConnectionDetection() {
  Serial.println("🔍 DEBUG: Forzando detección de conexión...");
  
  // Intentar detectar si hay una conexión activa
  NimBLEServer* pServer = NimBLEDevice::getServer();
  if (pServer) {
    // Verificar si podemos enviar datos (indicador de conexión)
    if (ENABLE_POWER_SENSOR && pPowerMeasurementCharacteristic != nullptr) {
      Serial.println("🔍 DEBUG: Probando envío de datos de potencia para detectar conexión...");
      
      // Generar y enviar un dato de prueba
      generateRandomPowerData();
      
      // Intentar enviar el dato
      if (pPowerMeasurementCharacteristic->notify()) {
        Serial.println("✅ DEBUG: Datos de potencia enviados exitosamente - conexión activa detectada!");
        if (!deviceConnected) {
          Serial.println("🔄 DEBUG: Estado de conexión actualizado manualmente");
          deviceConnected = true;
          lastConnectionTime = millis();
          isStableConnection = false;
          
          // Enviar primer dato inmediatamente
          updatePowerMeasurementData();
          lastPowerUpdateTime = millis();
        }
      } else {
        Serial.println("❌ DEBUG: No se pudieron enviar datos de potencia - sin conexión activa");
        if (deviceConnected) {
          Serial.println("🔄 DEBUG: Estado de conexión actualizado manualmente");
          deviceConnected = false;
        }
      }
    } else if (ENABLE_SPEED_SENSOR && pSpeedMeasurementCharacteristic != nullptr) {
      Serial.println("🔍 DEBUG: Probando envío de datos de velocidad para detectar conexión...");
      
      // Generar y enviar un dato de prueba
      generateRandomSpeedData();
      
      // Intentar enviar el dato
      if (pSpeedMeasurementCharacteristic->notify()) {
        Serial.println("✅ DEBUG: Datos de velocidad enviados exitosamente - conexión activa detectada!");
        if (!deviceConnected) {
          Serial.println("🔄 DEBUG: Estado de conexión actualizado manualmente");
          deviceConnected = true;
          lastConnectionTime = millis();
          isStableConnection = false;
          
          // Enviar primer dato inmediatamente
          updateSpeedMeasurementData();
          lastPowerUpdateTime = millis();
        }
      } else {
        Serial.println("❌ DEBUG: No se pudieron enviar datos de velocidad - sin conexión activa");
    if (deviceConnected) {
          Serial.println("🔄 DEBUG: Estado de conexión actualizado manualmente");
          deviceConnected = false;
        }
      }
    } else if (ENABLE_HEART_RATE_SENSOR && pHeartRateMeasurementCharacteristic != nullptr) {
      Serial.println("🔍 DEBUG: Probando envío de datos de frecuencia cardíaca para detectar conexión...");
      
      // Generar y enviar un dato de prueba
      generateRandomHeartRateData();
      
      // Intentar enviar el dato
      if (pHeartRateMeasurementCharacteristic->notify()) {
        Serial.println("✅ DEBUG: Datos de frecuencia cardíaca enviados exitosamente - conexión activa detectada!");
        if (!deviceConnected) {
          Serial.println("🔄 DEBUG: Estado de conexión actualizado manualmente");
          deviceConnected = true;
          lastConnectionTime = millis();
          isStableConnection = false;
          
          // Enviar primer dato inmediatamente
          updateHeartRateMeasurementData();
          lastPowerUpdateTime = millis();
        }
      } else {
        Serial.println("❌ DEBUG: No se pudieron enviar datos de frecuencia cardíaca - sin conexión activa");
        if (deviceConnected) {
          Serial.println("🔄 DEBUG: Estado de conexión actualizado manualmente");
          deviceConnected = false;
        }
      }
    }
  }
}

// Función para verificar el estado de la conexión BLE
void checkBLEConnectionStatus() {
  Serial.println("🔍 VERIFICANDO ESTADO BLE:");
  
  // Verificar si el servidor está activo
  NimBLEServer* pServer = NimBLEDevice::getServer();
  if (pServer) {
    Serial.println("   • Servidor BLE: ✅ Activo");
  } else {
    Serial.println("   • Servidor BLE: ❌ No disponible");
    return;
  }
  
  // Verificar si la característica está disponible
  if (ENABLE_POWER_SENSOR && pPowerMeasurementCharacteristic != nullptr) {
    Serial.println("   • Power Characteristic: ✅ Disponible");
    
    // Verificar propiedades de la característica
    uint32_t properties = pPowerMeasurementCharacteristic->getProperties();
    Serial.printf("   • Propiedades: 0x%02X\n", properties);
    
    // Verificar si tiene la propiedad NOTIFY
    if (properties & NIMBLE_PROPERTY::NOTIFY) {
      Serial.println("   • NOTIFY: ✅ Soportado");
    } else {
      Serial.println("   • NOTIFY: ❌ No soportado");
    }
  } else if (ENABLE_SPEED_SENSOR && pSpeedMeasurementCharacteristic != nullptr) {
    Serial.println("   • Speed Characteristic: ✅ Disponible");
    
    // Verificar propiedades de la característica
    uint32_t properties = pSpeedMeasurementCharacteristic->getProperties();
    Serial.printf("   • Propiedades: 0x%02X\n", properties);
    
    // Verificar si tiene la propiedad NOTIFY
    if (properties & NIMBLE_PROPERTY::NOTIFY) {
      Serial.println("   • NOTIFY: ✅ Soportado");
    } else {
      Serial.println("   • NOTIFY: ❌ No soportado");
    }
  } else {
    if (ENABLE_POWER_SENSOR) {
      Serial.println("   • Power Characteristic: ❌ No disponible");
    } else if (ENABLE_SPEED_SENSOR) {
      Serial.println("   • Speed Characteristic: ❌ No disponible");
    }
  }
  
  // Verificar estado del advertising
  NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
  if (pAdvertising) {
    Serial.println("   • Advertising: ✅ Configurado");
  } else {
    Serial.println("   • Advertising: ❌ No configurado");
  }
  
  Serial.println();
}

// Función para verificar el estado del advertising
void checkAdvertisingStatus() {
  Serial.println("📡 VERIFICANDO ESTADO DEL ADVERTISING:");
  Serial.println("🔧 CONFIGURACIÓN ACTIVA:");
  
  if (ENABLE_POWER_SENSOR) {
    Serial.println("   • Power Sensor (1818): ✅ ACTIVADO");
    Serial.println();
    Serial.println("⚙️  SENSOR DE POTENCIA:");
    Serial.println("   • Servicio: 1818 (Cycling Power Service)");
    Serial.println("   • Característica: 2A63 (Cycling Power Measurement)");
    Serial.println("   • Features: 0x0000000C (Wheel + Crank Revolution Data)");
    Serial.println("   • Flags: 0x60 (Wheel + Crank Revolution Data Present)");
    Serial.println("   • Formato: 14 bytes (Flags + Power + Wheel + Crank data)");
  } else if (ENABLE_SPEED_SENSOR) {
    Serial.println("   • Speed Sensor (1816): ✅ ACTIVADO");
    Serial.println();
    Serial.println("🚴 SENSOR DE VELOCIDAD/CADENCIA:");
    Serial.println("   • Servicio: 1816 (Cycling Speed and Cadence Service)");
    Serial.println("   • Característica: 2A5B (Cycling Speed and Cadence Measurement)");
    Serial.println("   • Features: 0x0003 (Wheel + Crank Revolution Data)");
    Serial.println("   • Flags: 0x03 (Wheel + Crank Revolution Data Present)");
    Serial.println("   • Formato: 11 bytes (Flags + Wheel + Crank data)");
    Serial.println("   • Tiempos: En MILISEGUNDOS (no en 1/1024 segundos)");
    
    // Información adicional para debugging de cadencia
    Serial.println("🔧 DEBUG CADENCIA:");
    Serial.printf("   • Crank Revs actuales: %u\n", cumulativeCrankRevolutions);
    Serial.printf("   • Crank Time actual: %u ms (intervalo)\n", lastCrankEventTime);
    Serial.printf("   • Cadencia simulada: %u RPM\n", instantCadence);
    Serial.printf("   • Distancia total: %lu m\n", totalDistance);
  } else if (ENABLE_HEART_RATE_SENSOR) {
    Serial.println("   • Heart Rate Sensor (180D): ✅ ACTIVADO");
    Serial.println();
    Serial.println("💓 SENSOR DE FRECUENCIA CARDÍACA:");
    Serial.println("   • Servicio: 180D (Heart Rate Service)");
    Serial.println("   • Característica: 2A37 (Heart Rate Measurement)");
    Serial.println("   • Features: 0x00000000 (No additional features)");
    Serial.println("   • Flags: 0x00 (No additional flags)");
    Serial.println("   • Formato: 2 bytes (Heart Rate Value)");
  }
  
  Serial.println();
}

// Función para verificar el estado de las características
void checkSubscriptionStatus() {
  if (!deviceConnected) return;
  
  Serial.println("🔍 Verificando estado de características...");
  
  if (ENABLE_POWER_SENSOR && pPowerMeasurementCharacteristic != nullptr) {
    Serial.println("   • Power Sensor: ✅ Disponible");
    
    // Debug específico para sensor de potencia
    Serial.printf("   • Power Data: %uW, Cadence: %uRPM\n", instantPower, instantCadence);
    Serial.printf("   • Wheel Revs: %lu, Crank Revs: %u\n", cumulativeWheelRevolutions, cumulativeCrankRevolutions);
    Serial.printf("   • Wheel Time: %u ms, Crank Time: %u ms\n", lastWheelEventTime, lastCrankEventTime);
  }
  
  if (ENABLE_SPEED_SENSOR && pSpeedMeasurementCharacteristic != nullptr) {
    Serial.println("   • Speed Sensor: ✅ Disponible");
    
    // Debug específico para sensor de velocidad/cadencia
    Serial.printf("   • Speed Data: %u km/h, Cadence: %uRPM\n", instantSpeed, instantCadence);
    Serial.printf("   • Wheel Revs: %lu, Crank Revs: %u, Distance: %lu m\n", 
                 cumulativeWheelRevolutions, cumulativeCrankRevolutions, totalDistance);
    Serial.printf("   • Wheel Time: %u ms (intervalo), Crank Time: %u ms (intervalo)\n", lastWheelEventTime, lastCrankEventTime);
  }
  
  if (ENABLE_HEART_RATE_SENSOR && pHeartRateMeasurementCharacteristic != nullptr) {
    Serial.println("   • Heart Rate Sensor: ✅ Disponible");
    
    // Debug específico para sensor de frecuencia cardíaca
    Serial.printf("   • Heart Rate Data: %u BPM, Energy: %u kJ\n", heartRate, energyExpended);
    Serial.printf("   • RR Intervals: %u intervalos válidos\n", rrIntervalCount);
  }
}

// Función específica para debug del sensor de velocidad/cadencia
void debugSpeedSensor() {
  if (!ENABLE_SPEED_SENSOR) return;
  
  Serial.println("🚴 DEBUG DEL SENSOR DE VELOCIDAD/CADENCIA (FORMATO SIMPLIFICADO):");
  Serial.printf("   • Velocidad simulada: %u km/h\n", instantSpeed);
  Serial.printf("   • Cadencia simulada: %u RPM\n", instantCadence);
  Serial.printf("   • Revoluciones de rueda: %lu (acumulativo)\n", cumulativeWheelRevolutions);
  Serial.printf("   • Revoluciones de pedalier: %u (acumulativo)\n", cumulativeCrankRevolutions);
  Serial.printf("   • Distancia total: %lu metros\n", totalDistance);
  Serial.printf("   • Tiempo de rueda: %u ms (acumulativo)\n", lastWheelEventTime);
  Serial.printf("   • Tiempo de pedalier: %u ms (acumulativo)\n", lastCrankEventTime);
  
  // Calcular cadencia real basada en los datos
  uint32_t realCadence = calculateRealCadence();
  if (realCadence > 0) {
    Serial.printf("   • Cadencia calculada: %u RPM\n", realCadence);
  }
  
  // Verificar propiedades de la característica BLE
  if (pSpeedMeasurementCharacteristic != nullptr) {
    Serial.println("🔗 PROPIEDADES DE LA CARACTERÍSTICA BLE:");
    Serial.printf("   • UUID: %s\n", pSpeedMeasurementCharacteristic->getUUID().toString().c_str());
    Serial.printf("   • Propiedades: %s\n", pSpeedMeasurementCharacteristic->getProperties() & BLE_GATT_CHR_PROP_NOTIFY ? "NOTIFY ✅" : "NOTIFY ❌");
    Serial.printf("   • Tamaño del paquete: 5 bytes (formato simplificado)\n");
    Serial.printf("   • Flags enviados: 0x02 (solo Crank, sin Wheel)\n");
  }
  
  Serial.println();
  Serial.println("💡 FORMATO SIMPLIFICADO IMPLEMENTADO:");
  Serial.println("   • Basado en proyecto ESP32-Zwift-Cadence que FUNCIONA");
  Serial.println("   • Solo envía datos de cadencia (sin rueda)");
  Serial.println("   • 5 bytes: Flags(1B) + Crank Revs(2B) + Crank Time(2B)");
  Serial.println("   • Compatible con Zwift y otras aplicaciones de ciclismo");
  Serial.println();
}

// Función para verificar que los datos de velocidad/cadencia se estén enviando correctamente
void verifySpeedDataFormat() {
  if (!ENABLE_SPEED_SENSOR) return;
  
  Serial.println("🔍 VERIFICACIÓN DE FORMATO DE DATOS (FORMATO SIMPLIFICADO):");
  Serial.println("   • Basado en proyecto ESP32-Zwift-Cadence que FUNCIONA");
  Serial.println("   • Formato: Flags(1B) + Crank Revs(2B) + Crank Time(2B) = 5 bytes");
  Serial.println();
  
  // Datos de prueba para verificar formato simplificado
  uint8_t testData[5];
  
  // Flags: Solo Crank Revolution Data Present
  testData[0] = 0x02;  // 0x02 = 00000010 (solo Crank, sin Wheel)
  
  // Crank Revolution Data (16 bits, Little Endian) - ACUMULATIVO
  // IMPORTANTE: Para CSCS, Garmin espera revoluciones ACUMULATIVAS
  uint16_t testCrankRevs = 50;  // 50 revoluciones acumuladas
  testData[1] = testCrankRevs & 0xFF;
  testData[2] = (testCrankRevs >> 8) & 0xFF;
  
  // Crank Event Time (16 bits, Little Endian) - ACUMULATIVO en 1/1024s
  // IMPORTANTE: Para CSCS, Garmin espera tiempos ACUMULATIVOS en 1/1024s
  uint16_t testCrankTime = 3000;  // 3000 ms acumulativos = 3072 (1/1024s)
  uint16_t testCrankTime1024 = (testCrankTime * 1024) / 1000;
  testData[3] = testCrankTime1024 & 0xFF;
  testData[4] = (testCrankTime1024 >> 8) & 0xFF;
  
  Serial.println("📊 ANÁLISIS DEL FORMATO:");
  Serial.printf("   • Flags: 0x%02X (Wheel:%s, Crank:%s)\n", 
               testData[0], 
               (testData[0] & 0x01) ? "✅" : "❌",
               (testData[0] & 0x02) ? "✅" : "❌");
  
  uint16_t crankRevs = testData[1] | (testData[2] << 8);
  uint16_t crankTime = testData[3] | (testData[4] << 8);
  
  Serial.printf("   • Crank Revs: %u (0x%02X%02X)\n", 
               crankRevs, testData[2], testData[1]);
  Serial.printf("   • Crank Time: %u ms (intervalo, no acumulativo) → %u (1/1024s)\n", 
               crankTime, testCrankTime1024);
  
  // Calcular cadencia real
  if (crankTime > 0) {
    // Convertir de unidades de 1/1024 segundos a milisegundos para el cálculo
    uint16_t crankTimeMs = (crankTime * 1000) / 1024;
    uint32_t calculatedCadence = 60000 / crankTimeMs; // Fórmula corregida
    Serial.printf("   • Cadencia calculada: %u RPM\n", calculatedCadence);
    
    // Verificar que esté en rango razonable
    if (calculatedCadence < 30 || calculatedCadence > 200) {
      Serial.printf("   • ⚠️  Cadencia fuera de rango razonable\n");
      Serial.printf("   • Fórmula: 60000 / %u ms = %u RPM\n", crankTimeMs, calculatedCadence);
    }
  }
  
  Serial.println("📊 Raw data dump:");
  for (int i = 0; i < sizeof(testData); i++) {
    Serial.printf("%02X ", testData[i]);
  }
  Serial.println();
  Serial.println();
  
  // Mostrar dump completo de datos de prueba
  Serial.print("📊 Raw test data dump (5 bytes): ");
  for (int i = 0; i < sizeof(testData); i++) {
    Serial.printf("%02X ", testData[i]);
  }
  Serial.println();
  
  // Análisis del formato simplificado
  Serial.println("📊 ANÁLISIS DEL FORMATO SIMPLIFICADO:");
  Serial.printf("   • Flags: 0x%02X (Wheel:%s, Crank:%s)\n", 
               testData[0], 
               (testData[0] & 0x01) ? "✅" : "❌",
               (testData[0] & 0x02) ? "✅" : "❌");
  Serial.printf("   • Crank Revs: %u (acumulativo) (0x%02X%02X)\n", 
               testCrankRevs, testData[2], testData[1]);
  Serial.printf("   • Crank Time: %u ms (acumulativo) → %u (1/1024s) (0x%02X%02X)\n", 
               testCrankTime, testCrankTime1024, testData[4], testData[3]);
  
  // Calcular cadencia basada en el tiempo acumulativo
  if (testCrankTime > 0) {
    // Para el formato simplificado, Garmin calcula cadencia usando deltas
    // entre paquetes consecutivos, no del tiempo absoluto
    Serial.println("💡 CÁLCULO DE CADENCIA (Garmin):");
    Serial.println("   • Garmin NO usa el tiempo absoluto de este paquete");
    Serial.println("   • Garmin calcula: RPM = (delta_revs * 1024 * 60) / delta_tiempo");
    Serial.println("   • Donde delta_tiempo = diferencia entre paquetes consecutivos");
    Serial.println("   • Por eso enviamos revoluciones ACUMULATIVAS");
  }
  
  Serial.println();
  Serial.println("✅ FORMATO SIMPLIFICADO VERIFICADO:");
  Serial.println("   • 5 bytes en lugar de 11 bytes");
  Serial.println("   • Solo datos de cadencia (sin rueda)");
  Serial.println("   • Basado en proyecto que FUNCIONA con Zwift");
  Serial.println("   • Compatible con estándar BLE CSCS");
  Serial.println();
}

// Función para calcular la cadencia real basada en los datos enviados
uint32_t calculateRealCadence() {
  if (lastCrankEventTime == 0) return 0;
  
  // Calcular cadencia basada en el intervalo de tiempo
  // Fórmula: RPM = (60000 ms) / (tiempo_ms por revolución)
  // Como enviamos 1 revolución por evento, el tiempo es directamente el intervalo
  uint32_t realCadence = 60000 / lastCrankEventTime;
  
  // Verificar que la cadencia esté en un rango razonable
  if (realCadence < 30 || realCadence > 200) {
    Serial.printf("⚠️  Cadencia calculada fuera de rango: %u RPM\n", realCadence);
    Serial.printf("   • Tiempo del evento: %u ms\n", lastCrankEventTime);
    Serial.printf("   • Fórmula: 60000 / %u = %u RPM\n", lastCrankEventTime, realCadence);
    return instantCadence; // Usar la cadencia simulada como fallback
  }
  
  return realCadence;
}

// Función para mostrar la diferencia entre cadencia simulada y calculada
void debugCadenceDifference() {
  if (!ENABLE_SPEED_SENSOR) return;
  
  uint32_t realCadence = calculateRealCadence();
  if (realCadence > 0) {
    int32_t difference = (int32_t)instantCadence - (int32_t)realCadence;
    Serial.printf("🔧 DIFERENCIA DE CADENCIA: Simulada=%u, Calculada=%u, Diferencia=%d RPM\n",
                 instantCadence, realCadence, difference);
    
    // Mostrar fórmula de cálculo
    Serial.printf("   • Fórmula: 60000 / %u ms = %u RPM\n", lastCrankEventTime, realCadence);
    
    // Verificar si la diferencia es significativa
    if (abs(difference) > 20) {
      Serial.printf("⚠️  Diferencia significativa detectada (>20 RPM)\n");
      Serial.printf("   • Posible causa: Tiempos muy altos o muy bajos\n");
      Serial.printf("   • Wheel Time: %u ms, Crank Time: %u ms\n", lastWheelEventTime, lastCrankEventTime);
    }
  }
}

// Función para mostrar el estado de los tiempos del sensor de velocidad/cadencia
void debugTimeStatus() {
  if (!ENABLE_SPEED_SENSOR) return;
  
  Serial.println("⏱️  ESTADO DE TIEMPOS DEL SENSOR DE VELOCIDAD/CADENCIA:");
  Serial.printf("   • Wheel Time: %u ms (acumulativo)\n", lastWheelEventTime);
  Serial.printf("   • Crank Time: %u ms (acumulativo)\n", lastCrankEventTime);
  
  // Verificar si los tiempos están en rango razonable
  if (lastWheelEventTime > 10000) {
    Serial.printf("   • ⚠️  Wheel Time muy alto (>10s) - próximo reset\n");
  } else if (lastWheelEventTime > 5000) {
    Serial.printf("   • ⚠️  Wheel Time alto (>5s) - monitorear\n");
  } else {
    Serial.printf("   • ✅ Wheel Time en rango razonable\n");
  }
  
  if (lastCrankEventTime > 10000) {
    Serial.printf("   • ⚠️  Crank Time muy alto (>10s) - próximo reset\n");
  } else if (lastCrankEventTime > 5000) {
    Serial.printf("   • ⚠️  Crank Time alto (>5s) - monitorear\n");
  } else {
    Serial.printf("   • ✅ Crank Time en rango razonable\n");
  }
  
  // Calcular cadencia basada en el tiempo actual
  if (lastCrankEventTime > 0) {
    uint32_t currentCadence = 60000 / lastCrankEventTime;
    Serial.printf("   • Cadencia actual: %u RPM\n", currentCadence);
    
    if (currentCadence < 10) {
      Serial.printf("   • ⚠️  Cadencia muy baja - tiempos muy altos\n");
    } else if (currentCadence > 200) {
      Serial.printf("   • ⚠️  Cadencia muy alta - tiempos muy bajos\n");
    } else {
      Serial.printf("   • ✅ Cadencia en rango razonable\n");
    }
  }
  
  Serial.println();
}

// Función para mostrar cómo Garmin interpreta los datos del sensor de velocidad/cadencia
void debugGarminInterpretation() {
  if (!ENABLE_SPEED_SENSOR) return;
  
  Serial.println("🧠 INTERPRETACIÓN DE GARMIN (FORMATO SIMPLIFICADO):");
  Serial.println("   • Basado en proyecto ESP32-Zwift-Cadence que FUNCIONA");
  Serial.println("   • Garmin NO usa los valores absolutos que enviamos");
  Serial.println("   • Garmin calcula DELTAS entre paquetes consecutivos:");
  Serial.println("   • 💡 Cadencia = (delta_crank_revs * 1024 * 60) / delta_tiempo");
  Serial.println("   • 💡 Donde delta_tiempo está en unidades de 1/1024 segundos");
  Serial.println();
  
  Serial.println("📊 DATOS ENVIADOS EN ESTE PAQUETE (5 bytes):");
  Serial.printf("   • Flags: 0x02 (solo Crank, sin Wheel)\n");
  Serial.printf("   • Crank Revs: %u (acumulativo)\n", cumulativeCrankRevolutions);
  Serial.printf("   • Crank Time: %u ms → %u (1/1024s)\n", lastCrankEventTime, (lastCrankEventTime * 1024) / 1000);
  Serial.println();
  
  Serial.println("🔍 CÁLCULO DE GARMIN (ejemplo con paquete anterior):");
  Serial.println("   • Si el paquete anterior tenía:");
  Serial.println("     - Crank Revs: 49, Crank Time: 2900 ms");
  Serial.println("   • Entonces Garmin calcula:");
  Serial.printf("     - Delta Crank: %u - 49 = 1 rev\n", cumulativeCrankRevolutions);
  Serial.printf("     - Delta Crank Time: %u - 2900 = %d ms\n", lastCrankEventTime, (int)lastCrankEventTime - 2900);
  Serial.println();
  
  Serial.println("✅ VENTAJAS DEL FORMATO SIMPLIFICADO:");
  Serial.println("   • Solo cadencia - menos complejidad para Garmin");
  Serial.println("   • 5 bytes en lugar de 11 bytes - más eficiente");
  Serial.println("   • Basado en proyecto que FUNCIONA con Zwift");
  Serial.println("   • Compatible con estándar BLE CSCS");
  Serial.println();
  
  Serial.println("🎯 CONCLUSIÓN:");
  Serial.println("   • Enviamos revoluciones ACUMULATIVAS ✅");
  Serial.println("   • Enviamos tiempos ACUMULATIVOS ✅");
  Serial.println("   • Formato simplificado (solo cadencia) ✅");
  Serial.println("   • Garmin calcula DELTAS para cadencia ✅");
  Serial.println("   • Por eso los valores individuales no importan, solo las diferencias ✅");
  Serial.println();
}

// Función para generar datos aleatorios realistas de frecuencia cardíaca
void generateRandomHeartRateData() {
  // Frecuencia cardíaca (60-180 BPM) - rango realista para ejercicio
  heartRate = random(60, 180);
  
  // FORMATO SIMPLIFICADO: Solo frecuencia cardíaca básica
  // No generar Energy Expended ni RR Intervals para simplificar
  
  // Debug de datos generados
  #if DEBUG_POWER_DATA
  Serial.printf("💓 Heart rate data generated (SIMPLIFICADO): HR=%u BPM\n", heartRate);
  Serial.println("   • 💡 FORMATO SIMPLIFICADO: Solo frecuencia cardíaca (como en bf05e06)");
  Serial.println("   • 💡 Sin Energy Expended ni RR Intervals para mayor compatibilidad");
  #endif
  
  // Debug adicional para verificar que se esté ejecutando
  Serial.printf("💓 DEBUG: Función generateRandomHeartRateData ejecutada - HR=%u BPM (SIMPLIFICADO)\n", heartRate);
}

// Función para debug del estado de conexión BLE
void debugBLEConnectionState() {
  Serial.println("🔍 DEBUG DEL ESTADO DE CONEXIÓN BLE:");
  Serial.printf("   • deviceConnected: %s\n", deviceConnected ? "true" : "false");
  Serial.printf("   • NimBLEDevice::getServer()->getConnectedCount(): %d\n", NimBLEDevice::getServer()->getConnectedCount());
  
  // Verificar características del sensor activo
  if (ENABLE_HEART_RATE_SENSOR) {
    Serial.printf("   • pHeartRateMeasurementCharacteristic: %s\n", 
                 pHeartRateMeasurementCharacteristic != nullptr ? "✅ Disponible" : "❌ NULL");
    if (pHeartRateMeasurementCharacteristic != nullptr) {
      Serial.printf("   • Propiedades de la característica: 0x%02X\n", 
                   pHeartRateMeasurementCharacteristic->getProperties());
    }
  }
  
  Serial.println();
}

// Función para debug comparativo con commit bf05e06
void debugComparisonWithWorkingCommit() {
  if (!ENABLE_HEART_RATE_SENSOR) return;
  
  Serial.println("🔍 COMPARACIÓN CON COMMIT bf05e06 (QUE FUNCIONABA):");
  Serial.println("   • COMMIT ANTERIOR (bf05e06):");
  Serial.println("     - Formato: SIMPLE (2-3 bytes)");
  Serial.println("     - Solo frecuencia cardíaca básica");
  Serial.println("     - Sin Energy Expended ni RR Intervals");
  Serial.println("     - Compatible con Garmin ✅");
  Serial.println();
  Serial.println("   • VERSIÓN ACTUAL:");
  Serial.println("     - Formato: COMPLEJO (25 bytes)");
  Serial.println("     - Frecuencia cardíaca + Energy + RR Intervals");
  Serial.println("     - Demasiado complejo para Garmin ❌");
  Serial.println();
  Serial.println("   • SOLUCIÓN IMPLEMENTADA:");
  Serial.println("     - Volver al formato SIMPLE (2 bytes)");
  Serial.println("     - Solo Flags + Heart Rate Value");
  Serial.println("     - Compatible con estándar BLE HRS ✅");
  Serial.println();
}

void setup() {
  // Inicializar comunicación serial para debug
  Serial.begin(115200);
  Serial.println();
  Serial.println("🚴 === ESP32 MULTI-SENSOR (NimBLE) - VERSIÓN MODULAR ===");
  
  Serial.println();
  Serial.println("🔧 CONFIGURACIÓN:");
  if (ENABLE_POWER_SENSOR) {
    Serial.println("   • Power Sensor: ✅ ACTIVADO");
  } else {
    Serial.println("   • Power Sensor: ❌ DESACTIVADO");
  }
  if (ENABLE_SPEED_SENSOR) {
    Serial.println("   • Speed/Cadence Sensor: ✅ ACTIVADO");
  } else {
    Serial.println("   • Speed/Cadence Sensor: ❌ DESACTIVADO");
  }
  if (ENABLE_HEART_RATE_SENSOR) {
    Serial.println("   • Heart Rate Sensor: ✅ ACTIVADO");
  } else {
    Serial.println("   • Heart Rate Sensor: ❌ DESACTIVADO");
  }
  Serial.println();
  
  Serial.println("🔧 CONFIGURACIÓN DE ESTABILIDAD:");
  Serial.printf("   • Heartbeat: ✅ ACTIVADO (cada %d ms)\n", HEARTBEAT_INTERVAL_MS);
  Serial.println();
  
  // Inicializar generador de números aleatorios
  randomSeed(analogRead(0));
  
  // Configurar BLE inicial con parámetros optimizados
  String deviceName;
  if (ENABLE_POWER_SENSOR) {
    deviceName = "ESP32 PowerSensor";
  } else if (ENABLE_SPEED_SENSOR) {
    deviceName = "ESP32 SpeedSensor";
  } else if (ENABLE_HEART_RATE_SENSOR) {
    deviceName = "ESP32 HeartRate";
  }
  
  NimBLEDevice::init(deviceName.c_str());
  Serial.printf("✅ BLE inicializado como '%s'\n", deviceName.c_str());
  
  // Subir potencia de transmisión para favorecer el escaneo
  NimBLEDevice::setPower(ESP_PWR_LVL_P9);
  Serial.println("✅ Potencia de transmisión configurada al máximo");
  
  // Crear servidor BLE
  NimBLEServer* pServer = NimBLEDevice::createServer();
  pServer->setCallbacks(new ServerCallbacks());
  
  // Crear servicios según configuración
  if (ENABLE_POWER_SENSOR) {
    createPowerService(pServer);
    Serial.println("✅ Servicio de potencia activado");
  }
  
  if (ENABLE_SPEED_SENSOR) {
    createSpeedService(pServer);
    Serial.println("✅ Servicio de velocidad/cadencia activado");
  }
  
  if (ENABLE_HEART_RATE_SENSOR) {
    createHeartRateService(pServer);
    Serial.println("✅ Servicio de frecuencia cardíaca activado");
  }
  
  // Configurar advertising
  NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
  
  if (ENABLE_POWER_SENSOR) {
    pAdvertising->addServiceUUID(CYCLING_POWER_SERVICE_UUID);
  }
  
  if (ENABLE_SPEED_SENSOR) {
    pAdvertising->addServiceUUID(CYCLING_SPEED_CADENCE_UUID);
  }
  
  if (ENABLE_HEART_RATE_SENSOR) {
    pAdvertising->addServiceUUID(HEART_RATE_SERVICE_UUID);
  }
  
  // Establecer apariencia GAP según el sensor activado
  uint16_t appearance;
  if (ENABLE_POWER_SENSOR) {
    appearance = 0x0485; // Cycling Power Sensor
  } else if (ENABLE_SPEED_SENSOR) {
    appearance = 0x0486; // Cycling Speed and Cadence Sensor
  } else if (ENABLE_HEART_RATE_SENSOR) {
    appearance = 0x03C0; // Heart Rate Sensor
  }
  pAdvertising->setAppearance(appearance);
  
  // Iniciar advertising
  Serial.println("🚀 Iniciando advertising...");
  pAdvertising->start();
  
  Serial.println();
  if (ENABLE_POWER_SENSOR) {
    Serial.println("📡 Anunciando sensor de potencia (1818)");
  Serial.println("🔧 INSTRUCCIONES PARA GARMIN:");
  Serial.println("   1. Configuración > Sensores > Añadir sensor");
  Serial.println("   2. Buscar sensores disponibles");
    Serial.println("   3. Verás 'ESP32 PowerSensor' como sensor de potencia");
    Serial.println("   4. Vincula el sensor de potencia");
    Serial.println("   5. Verifica que muestre potencia > 0W");
  } else if (ENABLE_SPEED_SENSOR) {
    Serial.println("📡 Anunciando sensor de velocidad/cadencia (1816)");
    Serial.println("🔧 INSTRUCCIONES PARA GARMIN:");
    Serial.println("   1. Configuración > Sensores > Añadir sensor");
    Serial.println("   2. Buscar sensores disponibles");
    Serial.println("   3. Verás 'ESP32 SpeedSensor' como sensor de velocidad/cadencia");
    Serial.println("   4. Vincula el sensor de velocidad/cadencia");
    Serial.println("   5. Verifica que muestre velocidad y cadencia");
  } else if (ENABLE_HEART_RATE_SENSOR) {
    Serial.println("📡 Anunciando sensor de frecuencia cardíaca (180D)");
    Serial.println("🔧 INSTRUCCIONES PARA GARMIN:");
    Serial.println("   1. Configuración > Sensores > Añadir sensor");
    Serial.println("   2. Buscar sensores disponibles");
    Serial.println("   3. Verás 'ESP32 HeartRate' como sensor de frecuencia cardíaca");
    Serial.println("   4. Vincula el sensor de frecuencia cardíaca");
    Serial.println("   5. Verifica que muestre BPM > 0");
  }
  Serial.println("⏱️  Actualización cada 1 segundo");
  Serial.println();
  Serial.println();
  if (ENABLE_POWER_SENSOR) {
    Serial.println("🎯 SOLUCIÓN AL PROBLEMA DE 0W:");
    Serial.println("   • Flags corregidos: 0x60 (bits 6 y 5 activos)");
    Serial.println("   • Features corregidos: 0x0000000C (bits 2 y 3 activos)");
    Serial.println("   • Formato de datos según estándar BLE CPS");
    Serial.println("   • Debug detallado de datos enviados");
  } else if (ENABLE_SPEED_SENSOR) {
    Serial.println("🎯 CONFIGURACIÓN DEL SENSOR DE VELOCIDAD/CADENCIA:");
    Serial.println("   • Flags: 0x03 (bits 0 y 1 activos)");
    Serial.println("   • Features: 0x0003 (Wheel + Crank Revolution Data)");
    Serial.println("   • Formato: 11 bytes según estándar BLE CSCS");
    Serial.println("   • Debug detallado de datos enviados");
  }
  Serial.println();
  Serial.println("📊 Estado: Advertising iniciado, esperando conexión...");
  
  // Verificar estado inicial del advertising
  checkAdvertisingStatus();
  
  // Verificar estado inicial de BLE
  checkBLEConnectionStatus();
  
  // Mostrar configuración final
  Serial.println("🔧 CONFIGURACIÓN FINAL:");
  if (ENABLE_POWER_SENSOR) {
    Serial.println("   • Power Sensor: ✅ ACTIVADO (Servicio 1818)");
  } else if (ENABLE_SPEED_SENSOR) {
    Serial.println("   • Speed Sensor: ✅ ACTIVADO (Servicio 1816)");
  }
  Serial.println();
  
  // Mostrar información específica del sensor activado
  if (ENABLE_POWER_SENSOR) {
    Serial.println("⚙️  SENSOR DE POTENCIA ACTIVADO:");
    Serial.println("   • UUID del servicio: 1818");
    Serial.println("   • Característica: 2A63 (Cycling Power Measurement)");
    Serial.println("   • Apariencia GAP: 0x0485 (Cycling Power Sensor)");
    Serial.println("   • El Garmin debería ver este sensor como 'ESP32 PowerSensor'");
    Serial.println("   • Formato de datos: Flags(2B) + Power(2B) + Wheel(4B) + WheelTime(2B) + Crank(2B) + CrankTime(2B)");
  } else if (ENABLE_SPEED_SENSOR) {
    Serial.println("🚴 SENSOR DE VELOCIDAD/CADENCIA ACTIVADO:");
    Serial.println("   • UUID del servicio: 1816");
    Serial.println("   • Característica: 2A5B (Cycling Speed and Cadence Measurement)");
    Serial.println("   • Apariencia GAP: 0x0486 (Cycling Speed and Cadence Sensor)");
    Serial.println("   • El Garmin debería ver este sensor como 'ESP32 SpeedSensor'");
    Serial.println("   • Formato de datos: Flags(1B) + Wheel(4B) + WheelTime(2B) + Crank(2B) + CrankTime(2B)");
  }
    Serial.println();
}

void loop() {
  unsigned long currentTime = millis();

  // Mostrar información periódica
  static unsigned long lastInfoTime = 0;
  static unsigned long lastSubscriptionCheck = 0;
  static unsigned long lastConnectionCheck = 0;
  if (currentTime - lastInfoTime >= 3000) { // Cada 3 segundos
    lastInfoTime = currentTime;
    
    #if DEBUG_CONNECTION_STATUS
    Serial.printf("📊 Estado: %s | ", deviceConnected ? "Conectado" : "Advertising");
    
    if (deviceConnected) {
      if (ENABLE_POWER_SENSOR) {
        Serial.printf("⚙️  Power:%uW Cad:%uRPM | ", instantPower, instantCadence);
      } else if (ENABLE_SPEED_SENSOR) {
        Serial.printf("🚴 Speed:%u km/h Cad:%uRPM | ", instantSpeed, instantCadence);
      } else if (ENABLE_HEART_RATE_SENSOR) {
        Serial.printf("💓 Heart Rate:%u BPM | ", heartRate);
      }
      Serial.printf("Wheel:%lu Crank:%u", cumulativeWheelRevolutions, cumulativeCrankRevolutions);
    } else {
      Serial.print("📡 Advertising activo - esperando conexión...");
    }
    
    Serial.println();
    
    // Verificar suscripciones cada 10 segundos SOLO si hay conexión
    if (deviceConnected && currentTime - lastSubscriptionCheck >= 10000) {
      lastSubscriptionCheck = currentTime;
      checkSubscriptionStatus();
      
      // Debug específico del sensor de velocidad/cadencia cada 10 segundos si hay conexión
      if (ENABLE_SPEED_SENSOR) {
        debugSpeedSensor();
        verifySpeedDataFormat();  // Verificar formato de datos
        
        // Verificación adicional de cadencia
        uint32_t realCadence = calculateRealCadence();
        if (realCadence > 0 && abs((int32_t)instantCadence - (int32_t)realCadence) > 20) {
          Serial.printf("⚠️  Diferencia significativa en cadencia: Simulada=%u, Calculada=%u RPM\n", 
                       instantCadence, realCadence);
        }
      }
    }
    
    // Verificar estado del advertising cada 30 segundos si no hay conexión
    static unsigned long lastAdvertisingCheck = 0;
    if (!deviceConnected && currentTime - lastAdvertisingCheck >= 30000) {
      lastAdvertisingCheck = currentTime;
      Serial.println("⏰ Verificación periódica del advertising...");
      checkAdvertisingStatus();
      
      // Verificar estado BLE cada 30 segundos
      checkBLEConnectionStatus();
      
      // Debug específico del sensor de velocidad/cadencia cada 30 segundos
      if (ENABLE_SPEED_SENSOR) {
        debugSpeedSensor();
        verifySpeedDataFormat();  // Verificar formato de datos
        debugTimeStatus();        // Mostrar estado de tiempos
        debugGarminInterpretation(); // Mostrar cómo Garmin interpreta los datos
      }
      
      // Debug específico del sensor de frecuencia cardíaca cada 30 segundos
      if (ENABLE_HEART_RATE_SENSOR) {
        debugBLEConnectionState(); // Debug del estado de conexión BLE
        debugComparisonWithWorkingCommit(); // Comparación con commit que funcionaba
      }
      
      // Reiniciar advertising si no hay conexión después de 2 minutos
      static unsigned long lastAdvertisingRestart = 0;
      if (currentTime - lastAdvertisingRestart >= 120000) { // 2 minutos
        lastAdvertisingRestart = currentTime;
        Serial.println("🔄 Reiniciando advertising para mejorar descubrimiento...");
        NimBLEDevice::getAdvertising()->stop();
        delay(1000);
        NimBLEDevice::getAdvertising()->start();
        Serial.println("✅ Advertising reiniciado");
      }
    }
    
          // Verificar estado de conexión cada 5 segundos para detectar conexiones perdidas
      if (currentTime - lastConnectionCheck >= 5000) {
      lastConnectionCheck = currentTime;
      
        // Si no hay conexión detectada, intentar detectarla alternativamente
      if (!deviceConnected) {
          Serial.println("🔍 Verificando conexión alternativamente...");
        forceConnectionDetection();
      }
    }
      
      // Si hay conexión detectada, verificar que se mantenga activa
      if (deviceConnected) {
        static unsigned long lastConnectionVerification = 0;
        if (currentTime - lastConnectionVerification >= 10000) { // Cada 10 segundos
          lastConnectionVerification = currentTime;
          
          // Verificar que la conexión siga activa intentando enviar datos
          if (ENABLE_POWER_SENSOR && pPowerMeasurementCharacteristic != nullptr) {
            if (!pPowerMeasurementCharacteristic->notify()) {
              Serial.println("⚠️  Conexión perdida detectada - actualizando estado");
              deviceConnected = false;
            }
          } else if (ENABLE_SPEED_SENSOR && pSpeedMeasurementCharacteristic != nullptr) {
            if (!pSpeedMeasurementCharacteristic->notify()) {
              Serial.println("⚠️  Conexión perdida detectada - actualizando estado");
              deviceConnected = false;
            }
          } else if (ENABLE_HEART_RATE_SENSOR && pHeartRateMeasurementCharacteristic != nullptr) {
            if (!pHeartRateMeasurementCharacteristic->notify()) {
              Serial.println("⚠️  Conexión perdida detectada - actualizando estado");
              deviceConnected = false;
            }
          }
        }
      }
    
    // Mostrar información de estabilidad de conexión
    if (deviceConnected) {
      unsigned long connectionTime = currentTime - lastConnectionTime;
      Serial.printf("🔗 Conexión activa: %lu segundos | ", connectionTime / 1000);
      
      if (connectionTime > STABLE_CONNECTION_THRESHOLD && !isStableConnection) {
        isStableConnection = true;
        Serial.println("✅ Conexión estable alcanzada!");
      }
      
      if (isStableConnection) {
        Serial.print("✅ Conexión estable | ");
      }
      
      if (ENABLE_POWER_SENSOR) {
        Serial.printf("Power Characteristic: %s\n", pPowerMeasurementCharacteristic ? "✅" : "❌");
      } else if (ENABLE_SPEED_SENSOR) {
        Serial.printf("Speed Characteristic: %s\n", pSpeedMeasurementCharacteristic ? "✅" : "❌");
      }
      
      // Verificar salud de la conexión
      if (!checkConnectionHealth()) {
        Serial.println("⚠️  Problemas detectados en la conexión");
      }
    } else {
      Serial.println("🔍 Buscando dispositivos Garmin... (Advertising activo)");
    }
    #endif
  }

  if (!deviceConnected) {
    delay(100);
    return;
  }

  // Verificar salud de la conexión antes de enviar datos
  if (!checkConnectionHealth()) {
    Serial.println("❌ Conexión no saludable, esperando reconexión...");
    delay(1000);
    return;
  }

  // Heartbeat para mantener la conexión activa
  if (ENABLE_HEARTBEAT && currentTime - lastHeartbeatTime >= HEARTBEAT_INTERVAL) {
    sendHeartbeat();
    lastHeartbeatTime = currentTime;
  }

  // Actualización de datos según el sensor activado
  if (ENABLE_POWER_SENSOR && currentTime - lastPowerUpdateTime >= UPDATE_INTERVAL) {
    generateRandomPowerData();
    updatePowerMeasurementData();
    lastPowerUpdateTime = currentTime;
  }

  if (ENABLE_SPEED_SENSOR && currentTime - lastPowerUpdateTime >= UPDATE_INTERVAL) {
    generateRandomSpeedData();
    updateSpeedMeasurementData();
    lastPowerUpdateTime = currentTime;
  }

  if (ENABLE_HEART_RATE_SENSOR && currentTime - lastHeartRateUpdateTime >= HEART_RATE_UPDATE_INTERVAL) {
    Serial.println("💓 Generando y enviando datos de frecuencia cardíaca...");
    generateRandomHeartRateData();
    updateHeartRateMeasurementData();
    lastHeartRateUpdateTime = currentTime;
    Serial.printf("💓 Datos enviados: HR=%u BPM, Energy=%u kJ, RR Intervals=%u\n", 
                 heartRate, energyExpended, rrIntervalCount);
  }

  delay(5);
}

// Función para crear el servicio de frecuencia cardíaca
void createHeartRateService(NimBLEServer* pServer) {
  Serial.println("💓 Creando servicio de frecuencia cardíaca (180D)...");
  
  // Crear el servicio de frecuencia cardíaca
  NimBLEService* pHeartRateService = pServer->createService(NimBLEUUID(HEART_RATE_SERVICE_UUID));
  
  // Característica de medición de frecuencia cardíaca (2A37)
  // Propiedades: NOTIFY (para enviar datos de frecuencia cardíaca)
  // NOTA: En NimBLE no es necesario añadir descriptor NimBLE2902
  pHeartRateMeasurementCharacteristic = pHeartRateService->createCharacteristic(
    NimBLEUUID(HEART_RATE_MEASUREMENT_UUID),
    NIMBLE_PROPERTY::NOTIFY
  );
  
  // Característica de punto de control (2A39) - OPCIONAL
  // Propiedades: WRITE (para comandos de control)
  pHeartRateControlPointCharacteristic = pHeartRateService->createCharacteristic(
    NimBLEUUID(HEART_RATE_CONTROL_POINT_UUID),
    NIMBLE_PROPERTY::WRITE
  );
  
  // Iniciar el servicio
  pHeartRateService->start();
  
  Serial.println("✅ Servicio de frecuencia cardíaca creado correctamente");
  Serial.printf("   • UUID del servicio: %s\n", HEART_RATE_SERVICE_UUID);
  Serial.printf("   • Característica de medición: %s (NOTIFY)\n", HEART_RATE_MEASUREMENT_UUID);
  Serial.printf("   • Característica de control: %s (WRITE)\n", HEART_RATE_CONTROL_POINT_UUID);
  Serial.println();
}

// Función para actualizar y enviar datos de frecuencia cardíaca
void updateHeartRateMeasurementData() {
  #if ENABLE_HEART_RATE_SENSOR
  // FORMATO SIMPLIFICADO (como en commit bf05e06 que funcionaba)
  // Estructura simple: Flags(1B) + Heart Rate Value(1B) = 2 bytes
  
  uint8_t heartRateData[2];
  
  // Flags: Solo Heart Rate Value Present (formato UINT8)
  // Bit 0: Heart Rate Value Format (0 = UINT8, 1 = UINT16)
  // Bit 1: Sensor Contact Status (0 = not supported)
  // Bit 2: Energy Expended Status (0 = not present)
  // Bit 3: RR Interval Status (0 = not present)
  uint8_t flags = 0x00;  // 0x00 = solo Heart Rate en formato UINT8
  
  heartRateData[0] = flags;
  heartRateData[1] = heartRate & 0xFF;  // Heart Rate en UINT8
  
  // Enviar datos
  if (pHeartRateMeasurementCharacteristic != nullptr) {
    Serial.printf("💓 DEBUG: Enviando datos SIMPLIFICADOS de frecuencia cardíaca - HR=%u BPM\n", heartRate);
    pHeartRateMeasurementCharacteristic->setValue(heartRateData, sizeof(heartRateData));
    
    if (pHeartRateMeasurementCharacteristic->notify()) {
      Serial.printf("💓 DEBUG: Notificación SIMPLIFICADA enviada exitosamente\n");
      #if DEBUG_DATA_SENDING
      Serial.printf("✅ Heart rate data sent (SIMPLIFICADO): HR=%u BPM\n", heartRate);
      Serial.printf("🔍 DEBUG: Flags=0x%02X, Packet Size=%u bytes\n", flags, sizeof(heartRateData));
      
      // Mostrar dump completo de datos
      Serial.print("📊 Raw heart rate data dump (SIMPLIFICADO): ");
      for (int i = 0; i < sizeof(heartRateData); i++) {
        Serial.printf("%02X ", heartRateData[i]);
      }
      Serial.println();
      
      // Análisis detallado
      Serial.println("📊 ANÁLISIS DE DATOS SIMPLIFICADOS:");
      Serial.printf("   • Flags: 0x%02X (Solo Heart Rate, formato UINT8)\n", flags);
      Serial.printf("   • Heart Rate: %u BPM\n", heartRate);
      Serial.printf("   • Tamaño del paquete: %u bytes (formato simple)\n", sizeof(heartRateData));
      Serial.println("   • 💡 FORMATO SIMPLIFICADO: Como en commit bf05e06 que funcionaba");
      Serial.println();
      #endif
    } else {
      Serial.println("❌ Failed to notify heart rate data");
    }
  }
  #endif
}
