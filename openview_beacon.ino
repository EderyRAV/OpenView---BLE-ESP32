#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <esp_gap_ble_api.h>

// =====================================================
// FIRMWARE ESTÁNDAR — BALIZAS OpenView (NAV_BLE_01 a NAV_BLE_06)
// =====================================================
//
// Este es el MISMO archivo para las 6 balizas. Lo único que cambia
// entre una unidad física y otra es el número de la línea de abajo.
// Todo lo demás (UUID, potencia, intervalo) debe quedar IGUAL en las
// seis, o el RSSI que compares entre ellas no va a ser comparable.
//
// Antes de subir el código a cada ESP32:
//   1. Cambia BEACON_ID al número de esa baliza física (1 a 6).
//   2. Sube el código (Verify -> Upload).
//   3. Abre el Monitor Serie (115200 baud) y confirma que dice
//      "Beacon BLE activo: NAV_BLE_0X" con el número correcto.
//   4. Pega una etiqueta física con ese mismo número en la carcasa,
//      para no confundirla luego con otra.
//
// Guía paso a paso para dejar Arduino IDE listo en Windows 11:
// ver el documento "OpenView — Configurar ESP32 en Windows 11".
//

// =====================================================
// 1) NÚMERO DE ESTA BALIZA — el único valor que cambia
// =====================================================

#define BEACON_ID 1   // <-- cambia SOLO este número: 1, 2, 3, 4, 5 o 6

#if (BEACON_ID < 1) || (BEACON_ID > 6)
  #error "BEACON_ID debe ser un numero entre 1 y 6"
#endif


// =====================================================
// 2) UUID DEL SERVICIO — igual en las 6, y coincide con la app
// =====================================================
//
// Este valor tiene que ser EXACTAMENTE el mismo que espera la app
// Android (TARGET_SERVICE_UUID en BleScanner.kt). La app detecta una
// baliza por su nombre (NAV_BLE_0X) O por este UUID, así que ambos
// caminos deben apuntar al mismo sistema.
//
#define SERVICE_UUID "12345678-1234-1234-1234-123456789001"


// =====================================================
// 3) POTENCIA DE TRANSMISIÓN — igual en las 6
// =====================================================
//
// Opciones disponibles en ESP32 (de más débil a más fuerte):
//
//   ESP_PWR_LVL_N12  ->  -12 dBm
//   ESP_PWR_LVL_N9   ->   -9 dBm
//   ESP_PWR_LVL_N6   ->   -6 dBm
//   ESP_PWR_LVL_N3   ->   -3 dBm
//   ESP_PWR_LVL_N0   ->    0 dBm
//   ESP_PWR_LVL_P3   ->   +3 dBm
//   ESP_PWR_LVL_P6   ->   +6 dBm
//   ESP_PWR_LVL_P9   ->   +9 dBm  (máxima)
//
// Se deja en +9 dBm (la máxima) como punto de partida estándar para
// las pruebas con las 6: en las pruebas anteriores el RSSI real fue
// bastante más débil de lo esperado incluso cerca de la baliza, así
// que arrancar con el máximo da más margen. Si luego quieres bajarla
// para ahorrar batería, cámbiala aquí — pero cámbiala en las SEIS a
// la vez, nunca en una sola.
//
#define TX_POWER ESP_PWR_LVL_P9


// =====================================================
// 4) INTERVALO DE ADVERTISING — igual en las 6
// =====================================================
//
// 1 unidad BLE = 0.625 ms. 160 unidades = 100 ms entre anuncios
// (~10 anuncios por segundo). No hay razón para tocar esto salvo que
// quieras experimentar con el consumo de batería vs. la frecuencia
// con la que el teléfono actualiza el RSSI.
//
#define ADV_INTERVAL 160


// =====================================================
// 5) LED DE ESTADO — por si tu placa no lo define sola
// =====================================================
//
// Varias placas ESP32 "genéricas" (clones con chip CH340, como la
// tuya) no traen definido LED_BUILTIN en su paquete de Arduino, y
// sin esto no compila. Si ya está definido, esta línea no hace nada;
// si no, usa el pin 2, el más común para el LED de a bordo en placas
// ESP32 DevKit. Si al final no ves parpadear ningún LED físico, no
// pasa nada grave: la baliza igual funciona, solo te quedas sin la
// señal visual de "estoy viva".
//
#ifndef LED_BUILTIN
#define LED_BUILTIN 2
#endif


// =====================================================
// A partir de aquí no hace falta tocar nada.
// =====================================================

char beaconName[16];  // se arma solo, ej. "NAV_BLE_01"

void setup() {
  Serial.begin(115200);
  delay(2000);

  sprintf(beaconName, "NAV_BLE_%02d", BEACON_ID);

  Serial.println("Iniciando beacon...");

  BLEDevice::init(beaconName);

  esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, TX_POWER);

  BLEAdvertising *advertising = BLEDevice::getAdvertising();

  BLEAdvertisementData advData;
  advData.setName(beaconName);
  advData.setCompleteServices(BLEUUID(SERVICE_UUID));
  advData.setFlags(0x06); // General Discoverable Mode + BR/EDR Not Supported
  advertising->setAdvertisementData(advData);
  advertising->setScanResponse(false);

  advertising->setMinInterval(ADV_INTERVAL);
  advertising->setMaxInterval(ADV_INTERVAL);

  BLEDevice::startAdvertising();

  // LED de a bordo parpadeando: sirve para confirmar a simple vista,
  // durante una prueba en el pasillo, que la baliza sigue encendida
  // y corriendo sin tener que abrir el teléfono. La mayoría de placas
  // ESP32 DevKit traen un LED en el pin 2; si la tuya no parpadea,
  // revisa en la serigrafía de la placa qué pin trae el LED integrado.
  pinMode(LED_BUILTIN, OUTPUT);

  Serial.println("=====================================");
  Serial.print("Beacon BLE activo: ");
  Serial.println(beaconName);
  Serial.print("UUID de servicio: ");
  Serial.println(SERVICE_UUID);
  Serial.print("Potencia (TX_POWER): ");
  Serial.println("ESP_PWR_LVL_P9 (+9 dBm)");
  Serial.print("Intervalo de anuncio: ");
  Serial.print(ADV_INTERVAL * 0.625);
  Serial.println(" ms");
  Serial.println("=====================================");
}

void loop() {
  // El controlador Bluetooth sigue anunciando solo, en segundo plano.
  // Este parpadeo es solo una señal visual de "estoy viva".
  digitalWrite(LED_BUILTIN, HIGH);
  delay(500);
  digitalWrite(LED_BUILTIN, LOW);
  delay(500);
}
