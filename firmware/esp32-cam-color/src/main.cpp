// ESP32-S3-CAM — versión B (color). Cada caja que ve el sensor TCS3472 llega al
// servidor como evento; el servidor publica {"evento_id":N} en planta/camara/capturar
// y esta placa saca una foto y la sube a /api/color/captura?evento_id=N. El
// clasificador (scikit-learn) corre en el servidor y compara contra el sensor.

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>  // aca (y no solo en camara_s3.h) para que PlatformIO detecte la libreria
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "config.h"
#include "camara_s3.h"

constexpr const char *TOPIC_CAPTURAR = "planta/camara/capturar";
// Saludo retenido + last will: el servidor sabe que version de camara hay conectada
// y, si se cae, el broker publica online:false solo (ver docs/camara-rostro.md).
constexpr const char *TOPIC_ESTADO = "planta/camara/estado/color";
constexpr const char *ESTADO_ONLINE = "{\"version\":\"color\",\"online\":true}";
constexpr const char *ESTADO_OFFLINE = "{\"version\":\"color\",\"online\":false}";

// Cola chica: si pasan dos cajas seguidas mientras se sube una foto, no se pierde la orden.
constexpr int MAX_PENDIENTES = 6;
int pendientes[MAX_PENDIENTES];
int cantidadPendientes = 0;

WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);

void onMqttMessage(char *topic, byte *payload, unsigned int length) {
  JsonDocument doc;
  if (deserializeJson(doc, payload, length) != DeserializationError::Ok) return;
  int eventoId = doc["evento_id"] | 0;
  if (eventoId > 0 && cantidadPendientes < MAX_PENDIENTES) pendientes[cantidadPendientes++] = eventoId;
}

void conectarMqtt() {
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setCallback(onMqttMessage);
  while (!mqtt.connected()) {
    if (mqtt.connect("esp32-cam-color", nullptr, nullptr, TOPIC_ESTADO, 1, true, ESTADO_OFFLINE)) {
      mqtt.publish(TOPIC_ESTADO, ESTADO_ONLINE, true);
      mqtt.subscribe(TOPIC_CAPTURAR);
      Serial.println("MQTT OK, suscrito a planta/camara/capturar");
    } else {
      Serial.printf("MQTT fallo, state()=%d, reintento en 1s\n", mqtt.state());
      delay(1000);
    }
  }
}

void capturarYEnviar(int eventoId, unsigned long recibidoEn) {
  camera_fb_t *foto = esp_camera_fb_get();
  if (!foto) {
    Serial.println("esp_camera_fb_get devolvio NULL");
    return;
  }
  unsigned long capturadoEn = millis();
  String cuerpo;
  int codigo = postJpeg(String(API_URL) + "/api/color/captura?evento_id=" + eventoId, CAMARA_TOKEN, foto, &cuerpo);
  Serial.printf("evento %d: foto %u bytes | orden->foto %lu ms | POST HTTP %d en %lu ms | %s\n", eventoId,
                (unsigned)foto->len, capturadoEn - recibidoEn, codigo, millis() - capturadoEn, cuerpo.c_str());
  esp_camera_fb_return(foto);
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.printf("PSRAM: %u bytes\n", (unsigned)ESP.getPsramSize());
  if (!iniciarCamara()) {
    Serial.println("Sin camara: reiniciando en 5s");
    delay(5000);
    ESP.restart();
  }
  conectarWifi(WIFI_SSID, WIFI_PASSWORD);
  conectarMqtt();
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.reconnect();
    delay(1000);
    return;
  }
  if (!mqtt.connected()) conectarMqtt();
  mqtt.loop();

  if (cantidadPendientes > 0) {
    unsigned long recibidoEn = millis();
    int eventoId = pendientes[0];
    for (int i = 1; i < cantidadPendientes; i++) pendientes[i - 1] = pendientes[i];
    cantidadPendientes--;
    capturarYEnviar(eventoId, recibidoEn);
  }
}
