# 🚴 ESP32 Multi-Sensor BLE para Ciclismo

## 📋 Descripción

Este proyecto implementa un **ESP32 multi-sensor BLE** que puede funcionar como:
- **Sensor de Potencia** (Cycling Power Service - CPS)
- **Sensor de Velocidad/Cadencia** (Cycling Speed and Cadence Service - CSCS)  
- **Sensor de Frecuencia Cardíaca** (Heart Rate Service - HRS)

Diseñado para ser compatible con dispositivos Garmin, Zwift y otras aplicaciones de ciclismo.

## ✨ Características

### 🔧 **Multi-Sensor Modular**
- **Configuración flexible:** Activa solo el sensor que necesites
- **Compatibilidad total:** Con estándares BLE oficiales
- **Detección automática:** De conexión y desconexión
- **Heartbeat inteligente:** Mantiene conexiones estables

### 📱 **Compatibilidad**
- **Garmin Edge/Fenix/Forerunner** ✅
- **Zwift** ✅
- **Strava** ✅
- **Otras apps de ciclismo** ✅

### 🚀 **Funcionalidades Avanzadas**
- **Datos realistas:** Simulación de valores de ciclismo
- **Debug extensivo:** Monitorización completa del sistema
- **Manejo de errores:** Reconexión automática y estabilidad
- **Optimización BLE:** Potencia de transmisión máxima

## 🛠️ Hardware Requerido

### **Componentes Principales**
- **ESP32 Dev Module** (con Bluetooth integrado)
- **Cable USB** para programación y alimentación
- **Computadora** con Arduino IDE

### **Especificaciones Técnicas**
- **Placa:** ESP32 Dev Module
- **Frecuencia:** 240MHz
- **Memoria:** 4MB Flash + 1.5MB SPIFFS
- **Bluetooth:** BLE 4.0 integrado
- **Alimentación:** 5V via USB

## 📦 Instalación

### **1. Requisitos Previos**
- **Arduino IDE 1.8.19** o superior
- **Biblioteca ESP32** para Arduino
- **Biblioteca NimBLE-Arduino**

### **2. Instalación de Bibliotecas**
```bash
# En Arduino IDE: Herramientas > Administrar Bibliotecas
# Buscar e instalar:
- "ESP32" por Espressif Systems
- "NimBLE-Arduino" por h2zero
```

### **3. Configuración de Placa**
```
Placa: "ESP32 Dev Module"
Upload Speed: 921600
CPU Frequency: 240MHz (WiFi/BT)
Flash Frequency: 80MHz
Flash Mode: QIO
Flash Size: "4MB (32Mb)"
Partition Scheme: "Default 4MB with spiffs (1.2MB APP/1.5MB SPIFFS)"
```

## ⚙️ Configuración

### **Selección de Sensor**
Edita las siguientes líneas en `ESP32_PowerSensor.ino`:

```cpp
// ===== CONFIGURACIÓN DEL SENSOR =====
#define ENABLE_POWER_SENSOR      false    // Sensor de potencia
#define ENABLE_SPEED_SENSOR      false    // Sensor de velocidad/cadencia
#define ENABLE_HEART_RATE_SENSOR true     // Sensor de frecuencia cardíaca
```

### **Opciones de Debug**
```cpp
// ===== CONFIGURACIÓN DE DEBUG =====
#define DEBUG_SENSOR_STATUS      true     // Estado del sensor
#define DEBUG_DATA_SENDING       true     // Confirmación de envío
#define DEBUG_CONNECTION_STATUS  true     // Estado de conexión detallado
#define DEBUG_POWER_DATA         true     // Debug específico de datos
```

## 🔌 Uso

### **1. Compilación y Subida**
1. Abre `ESP32_PowerSensor.ino` en Arduino IDE
2. Selecciona la placa ESP32 Dev Module
3. Compila con `Ctrl+Shift+U`
4. Sube al ESP32 con `Ctrl+Shift+U`

### **2. Conexión con Garmin**
1. **Configuración > Sensores > Añadir sensor**
2. **Buscar sensores disponibles**
3. **Verás el sensor correspondiente:**
   - `ESP32 PowerSensor` (para potencia)
   - `ESP32 SpeedSensor` (para velocidad/cadencia)
   - `ESP32 HeartRate` (para frecuencia cardíaca)
4. **Vincula el sensor**
5. **Verifica que muestre datos > 0**

### **3. Monitorización**
- **Monitor Serial:** 115200 baudios
- **Logs detallados:** Estado, datos y debug
- **Verificación:** De conexión y envío de datos

## 📊 Especificaciones Técnicas

### **Sensor de Potencia (CPS)**
- **UUID Servicio:** 1818
- **UUID Característica:** 2A63 (Cycling Power Measurement)
- **Formato:** 14 bytes (Flags + Power + Wheel + Crank)
- **Rango:** 150-450W
- **Apariencia GAP:** 0x0485 (Cycling Power Sensor)

### **Sensor de Velocidad/Cadencia (CSCS)**
- **UUID Servicio:** 1816
- **UUID Característica:** 2A5B (Cycling Speed and Cadence Measurement)
- **Formato:** 5 bytes (Flags + Crank Revs + Crank Time)
- **Velocidad:** 15-45 km/h
- **Cadencia:** 70-110 RPM
- **Apariencia GAP:** 0x0486 (Cycling Speed and Cadence Sensor)

### **Sensor de Frecuencia Cardíaca (HRS)**
- **UUID Servicio:** 180D
- **UUID Característica:** 2A37 (Heart Rate Measurement)
- **Formato:** 2 bytes (Flags + Heart Rate)
- **Rango:** 60-180 BPM
- **Apariencia GAP:** 0x03C0 (Heart Rate Sensor)

## 🔍 Debug y Troubleshooting

### **Logs de Estado**
```
🚴 === ESP32 MULTI-SENSOR (NimBLE) - VERSIÓN MODULAR ===

🔧 CONFIGURACIÓN:
   • Power Sensor: ❌ DESACTIVADO
   • Speed/Cadence Sensor: ❌ DESACTIVADO
   • Heart Rate Sensor: ✅ ACTIVADO

💓 Creando servicio de frecuencia cardíaca (180D)...
✅ Servicio de frecuencia cardíaca creado correctamente
```

### **Logs de Datos**
```
💓 Heart rate data generated (SIMPLIFICADO): HR=140 BPM
💓 DEBUG: Enviando datos SIMPLIFICADOS de frecuencia cardíaca - HR=140 BPM
✅ Heart rate data sent (SIMPLIFICADO): HR=140 BPM
📊 Raw heart rate data dump (SIMPLIFICADO): 00 8C
```

### **Problemas Comunes**

#### **1. Garmin no detecta el sensor**
- **Verificar:** Que el sensor esté en modo Advertising
- **Solución:** Reiniciar el ESP32 o verificar configuración BLE

#### **2. Datos muestran 0**
- **Verificar:** Que la conexión esté establecida
- **Solución:** Revisar logs de debug para identificar el problema

#### **3. Conexión inestable**
- **Verificar:** Potencia de transmisión BLE
- **Solución:** El código ya incluye manejo automático de reconexión

## 🚀 Características Avanzadas

### **Detección de Conexión Robusta**
- **Callbacks BLE:** `onConnect` y `onDisconnect`
- **Detección alternativa:** Si los callbacks fallan
- **Verificación periódica:** Estado de conexión cada 5 segundos

### **Heartbeat Inteligente**
- **Mantenimiento:** De conexiones activas
- **Intervalo configurable:** Por defecto 250ms
- **Datos actualizados:** En cada heartbeat

### **Manejo de Errores**
- **Reconexión automática:** Con backoff exponencial
- **Verificación de salud:** De conexión y datos
- **Recuperación:** Automática de errores

## 📁 Estructura del Proyecto

```
ESP32_PowerSensor/
├── ESP32_PowerSensor.ino          # Archivo principal
├── README.md                       # Este archivo
└── .gitignore                     # Archivos a ignorar
```

## 🔧 Desarrollo

### **Arquitectura del Código**
- **Modular:** Cada sensor tiene sus propias funciones
- **Configurable:** Fácil activar/desactivar sensores
- **Extensible:** Fácil añadir nuevos sensores
- **Debug:** Extensivo para desarrollo y troubleshooting

### **Funciones Principales**
- **`setup()`:** Configuración inicial y creación de servicios
- **`loop()`:** Bucle principal y envío de datos
- **`sendHeartbeat()`:** Mantenimiento de conexión
- **`checkConnectionHealth()`:** Verificación de salud
- **`debugBLEConnectionState()`:** Debug de estado BLE

## 📚 Referencias

### **Estándares BLE**
- **Cycling Power Service:** [GATT Specification](https://www.bluetooth.com/specifications/specs/cycling-power-service-1-0/)
- **Cycling Speed and Cadence Service:** [GATT Specification](https://www.bluetooth.com/specifications/specs/cycling-speed-and-cadence-service-1-0/)
- **Heart Rate Service:** [GATT Specification](https://www.bluetooth.com/specifications/specs/heart-rate-service-1-0/)

### **Bibliotecas Utilizadas**
- **NimBLE-Arduino:** [GitHub](https://github.com/h2zero/NimBLE-Arduino)
- **ESP32 Arduino:** [GitHub](https://github.com/espressif/arduino-esp32)

## 🤝 Contribuciones

### **Cómo Contribuir**
1. **Fork** del repositorio
2. **Crea** una rama para tu feature (`git checkout -b feature/AmazingFeature`)
3. **Commit** tus cambios (`git commit -m 'Add some AmazingFeature'`)
4. **Push** a la rama (`git push origin feature/AmazingFeature`)
5. **Abre** un Pull Request

### **Áreas de Mejora**
- **Nuevos sensores:** Temperatura, humedad, etc.
- **Optimización:** Consumo de energía
- **Interfaz web:** Para configuración
- **Almacenamiento:** Datos en SPIFFS

## 📄 Licencia

Este proyecto está bajo la **Licencia MIT**. Ver el archivo `LICENSE` para más detalles.

## 🙏 Agradecimientos

- **Espressif Systems:** Por el ESP32
- **h2zero:** Por la biblioteca NimBLE-Arduino
- **Comunidad BLE:** Por los estándares y especificaciones
- **Usuarios:** Por el feedback y testing

## 📞 Soporte

### **Problemas Comunes**
- **Revisa:** Los logs del Monitor Serial
- **Verifica:** La configuración del sensor
- **Consulta:** La sección de troubleshooting

### **Contacto**
- **Issues:** Usa la sección de Issues de GitHub
- **Discussions:** Para preguntas y discusiones
- **Wiki:** Para documentación adicional

---

## 🎯 **Estado del Proyecto**

### **✅ Funcionando**
- **Sensor de Potencia:** Completamente funcional
- **Sensor de Velocidad/Cadencia:** Completamente funcional  
- **Sensor de Frecuencia Cardíaca:** Completamente funcional

### **🚧 En Desarrollo**
- **Optimización:** De consumo de energía
- **Nuevos sensores:** En planificación
- **Interfaz web:** Para configuración

### **📋 Roadmap**
- **v1.1:** Optimización de estabilidad
- **v1.2:** Nuevos tipos de sensores
- **v2.0:** Interfaz web y configuración avanzada

---

**¡Disfruta usando tu ESP32 Multi-Sensor BLE para ciclismo! 🚴‍♂️💪**
