#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLEAdvertising.h>
#include <esp_gap_ble_api.h>

// =====================================================
// FIRMWARE v3 — BALIZAS OpenView (NAV_BLE_01 a NAV_BLE_06)
// =====================================================
//
// MISMO archivo para las 6 balizas. Lo único que cambias al subirlo es BEACON_ID.
// La potencia de cada baliza sale sola de la tabla TX_POR_BALIZA (sección 3).
//
// Qué cambia respecto a la v2:
//   1. POTENCIA POR BALIZA. Las placas 01 y 03 (mismo fabricante, puente USB CP210x)
//      se oyen mucho más fuerte que 04/05/06 con la MISMA potencia configurada:
//      medido con el Redmi Note 14 (24-sep-2026), la 03 da -67 dBm a 1 m frente a
//      -80…-84 de las demás, y -36 en contacto frente a -54…-57 (unos 15-19 dB más).
//      No es la potencia (todas leían nivel 7 = +9 dBm): es la antena/placa.
//      Solución por software: a 01 y 03 se les baja la potencia para igualarlas.
//   2. FW_VERSION = 3 (la app lo muestra en Administración > Balizas).
//   3. El nivel de potencia que viaja en los datos de fabricante es el REAL aplicado,
//      así la app sabe que esa baliza está recortada y avisa si su calibración es de
//      otra potencia.
//
// Pasos por cada ESP32:
//   1. Cambia BEACON_ID (1 a 6). NADA más.
//   2. Sube el código (Verify -> Upload).
//   3. Monitor Serie a 115200 y comprueba la línea
//        "TX REAL: nivel N = X dBm (tabla: nivel N)"
//      01 y 03 deben decir nivel 3 = -3 dBm; 02, 04, 05 y 06 nivel 7 = +9 dBm.
//   4. En la app: vuelve a CALIBRAR 01 y 03 con el asistente (la calibración anterior
//      era de +9 dBm y ya no vale) y borra sus huellas de la prueba guiada.
//
// Cómo afinar (después de calibrar):
//   Objetivo: que 01 y 03 den a 1 m lo mismo que las demás (±3 dB), unos -80 dBm.
//   - Si la 03 sigue más fuerte que -77 dBm a 1 m, baja un nivel más (3 -> 2 = -6 dBm).
//   - Si ahora sale más débil que -86 dBm a 1 m, sube un nivel (3 -> 4 = 0 dBm).
//   Cada nivel son 3 dB. Cambia solo su fila en la tabla y vuelve a subir.
//
// Niveles del ESP32: 0=-12, 1=-9, 2=-6, 3=-3, 4=0, 5=+3, 6=+6, 7=+9 dBm
//

// =====================================================
// 1) NÚMERO DE ESTA BALIZA
// =====================================================
#define BEACON_ID 3   // <-- cambia SOLO este número: 1, 2, 3, 4, 5 o 6

#if (BEACON_ID < 1) || (BEACON_ID > 6)
  #error "BEACON_ID debe ser un numero entre 1 y 6"
#endif

// =====================================================
// 2) PARÁMETROS COMUNES (iguales en las 6)
// =====================================================
#define FW_VERSION    3
#define SERVICE_UUID  "12345678-1234-1234-1234-123456789001"   // igual que TARGET_SERVICE_UUID en la app
#define ADV_INTERVAL  80                 // 80 * 0.625 ms = 50 ms (~20 anuncios/s)

// Company ID de pruebas (0xFFFF = reservado para uso interno/desarrollo).
#define COMPANY_ID_LO 0xFF
#define COMPANY_ID_HI 0xFF

// =====================================================
// 3) POTENCIA DE CADA BALIZA (índice = BEACON_ID)
// =====================================================
static const esp_power_level_t TX_POR_BALIZA[7] = {
  ESP_PWR_LVL_P9,   // (sin uso)
  ESP_PWR_LVL_N3,   // 01: CP210x, mismo fabricante que la 03 -> -3 dBm (sin medir aún: confirmar con el asistente)
  ESP_PWR_LVL_P9,   // 02: CH340 -> +9 dBm
  ESP_PWR_LVL_N3,   // 03: CP210x, ~15 dB más fuerte medida -> -3 dBm (recorte de 12 dB)
  ESP_PWR_LVL_P9,   // 04: CH340 -> +9 dBm
  ESP_PWR_LVL_P9,   // 05: CH340 -> +9 dBm
  ESP_PWR_LVL_P9    // 06: CH340 -> +9 dBm
};
#define TX_POWER (TX_POR_BALIZA[BEACON_ID])

#ifndef LED_BUILTIN
#define LED_BUILTIN 2
#endif

// Arduino-ESP32 3.x usa String; la 2.x usa std::string. Este alias funciona en ambas.
#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
  typedef String BleBytes;
#else
  typedef std::string BleBytes;
#endif

char beaconName[16];   // p.ej. "NAV_BLE_03"

static int txLevelToDbm(int level) {
  return -12 + 3 * level;
}

void setup() {
  Serial.begin(115200);
  delay(1500);

  sprintf(beaconName, "NAV_BLE_%02d", BEACON_ID);
  Serial.println();
  Serial.println("Iniciando beacon v3...");

  BLEDevice::init(beaconName);

  // Potencia: se fija para los dos tipos y luego se LEE de vuelta.
  esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, TX_POWER);
  esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, TX_POWER);
  esp_power_level_t realLvl = esp_ble_tx_power_get(ESP_BLE_PWR_TYPE_ADV);

  BLEAdvertising *advertising = BLEDevice::getAdvertising();

  // Anuncio principal (22 bytes): flags(3) + nombre(12) + datos de fabricante(7)
  //   datos de fabricante = company id (2) + beaconId (1) + fwVersion (1) + txLevel REAL (1)
  BleBytes mfg;
  mfg += (char)COMPANY_ID_LO;
  mfg += (char)COMPANY_ID_HI;
  mfg += (char)BEACON_ID;
  mfg += (char)FW_VERSION;
  mfg += (char)realLvl;

  BLEAdvertisementData advData;
  advData.setFlags(0x06);            // General Discoverable + BR/EDR no soportado
  advData.setName(beaconName);
  advData.setManufacturerData(mfg);
  advertising->setAdvertisementData(advData);

  // Respuesta de escaneo (18 bytes): UUID de servicio de 128 bits.
  BLEAdvertisementData scanResp;
  scanResp.setCompleteServices(BLEUUID(SERVICE_UUID));
  advertising->setScanResponseData(scanResp);
  advertising->setScanResponse(true);

  // Escaneable pero NO conectable. (NONCONN_IND no permitiría la respuesta de escaneo.)
  advertising->setAdvertisementType(ADV_TYPE_SCAN_IND);

  advertising->setMinInterval(ADV_INTERVAL);
  advertising->setMaxInterval(ADV_INTERVAL);
  BLEDevice::startAdvertising();

  pinMode(LED_BUILTIN, OUTPUT);

  // ---- Diagnóstico REAL ----
  Serial.println("=====================================");
  Serial.printf("Beacon BLE activo: %s | firmware v%d\n", beaconName, FW_VERSION);
  Serial.printf("chip %s rev %d | %d MHz | flash %u KB\n",
                ESP.getChipModel(), (int)ESP.getChipRevision(),
                (int)ESP.getCpuFreqMHz(), (unsigned)(ESP.getFlashChipSize() / 1024));
  Serial.printf("MAC %s\n", BLEDevice::getAddress().toString().c_str());
  Serial.printf("UUID de servicio: %s\n", SERVICE_UUID);
  Serial.printf("TX REAL: nivel %d = %+d dBm (tabla: nivel %d = %+d dBm)\n",
                (int)realLvl, txLevelToDbm((int)realLvl), (int)TX_POWER, txLevelToDbm((int)TX_POWER));
  if ((int)realLvl != (int)TX_POWER) {
    Serial.println("!!! ATENCION: la potencia leida NO coincide con la tabla. Anota esta baliza.");
  }
  Serial.printf("Intervalo de anuncio: %.1f ms\n", ADV_INTERVAL * 0.625);
  Serial.println("=====================================");
}

void loop() {
  // Parpadea BEACON_ID veces y hace una pausa larga (3 parpadeos = baliza 03).
  for (int i = 0; i < BEACON_ID; i++) {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(200);
    digitalWrite(LED_BUILTIN, LOW);
    delay(250);
  }
  delay(1800);

  // Latido en el Monitor Serie: confirma que sigue viva y con qué potencia.
  static unsigned long lastBeat = 0;
  if (millis() - lastBeat > 10000) {
    lastBeat = millis();
    int lvl = (int)esp_ble_tx_power_get(ESP_BLE_PWR_TYPE_ADV);
    Serial.printf("[%lus] %s viva | TX nivel %d = %+d dBm\n", millis() / 1000, beaconName, lvl, txLevelToDbm(lvl));
  }
}
