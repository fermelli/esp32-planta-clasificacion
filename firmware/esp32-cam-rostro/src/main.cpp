// ESP32-S3-CAM — versión A (rostro). Espera órdenes del servidor por MQTT en
// planta/camara/rostro y manda fotos por HTTP:
//   {"modo":"verificar","usuario_id":N}  -> fotos a /api/rostro/verificar hasta que el servidor decida
//   {"modo":"enrolar","usuario_id":N}    -> 5 fotos a /api/rostro/muestra (registrar la cara del usuario)

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>  // aca (y no solo en camara_s3.h) para que PlatformIO detecte la libreria
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "config.h"
#include "camara_s3.h"

constexpr const char *TOPIC_ROSTRO = "planta/camara/rostro";
constexpr const char *TOPIC_PROBAR = "planta/camara/probar";  // foto de prueba desde el dashboard
// Saludo retenido + last will: el servidor sabe que version de camara hay conectada
// y, si se cae, el broker publica online:false solo (ver docs/camara-rostro.md).
constexpr const char *TOPIC_ESTADO = "planta/camara/estado/rostro";
constexpr const char *ESTADO_ONLINE = "{\"version\":\"rostro\",\"online\":true}";
constexpr const char *ESTADO_OFFLINE = "{\"version\":\"rostro\",\"online\":false}";

// El servidor vence el login a los 15 s (ROSTRO_VENTANA_S): dejar margen.
constexpr unsigned long VENTANA_VERIFICAR_MS = 13000;
constexpr unsigned long ENTRE_FOTOS_VERIFICAR_MS = 800;
constexpr int FOTOS_ENROLAR = 5;
constexpr unsigned long ENTRE_FOTOS_ENROLAR_MS = 1200;

WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);

enum Modo { NINGUNO, VERIFICAR, ENROLAR };
Modo modoPendiente = NINGUNO;
int usuarioPendiente = 0;
bool probarPendiente = false;

void onMqttMessage(char *topic, byte *payload, unsigned int length) {
  JsonDocument doc;
  if (deserializeJson(doc, payload, length) != DeserializationError::Ok) return;
  if (strcmp(topic, TOPIC_PROBAR) == 0) {
    if (strcmp(doc["version"] | "", "rostro") == 0) probarPendiente = true;
    return;
  }
  const char *modo = doc["modo"] | "";
  usuarioPendiente = doc["usuario_id"] | 0;
  if (strcmp(modo, "verificar") == 0) modoPendiente = VERIFICAR;
  else if (strcmp(modo, "enrolar") == 0) modoPendiente = ENROLAR;
  Serial.printf("Orden recibida: %s usuario_id=%d\n", modo, usuarioPendiente);
}

void conectarMqtt() {
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setCallback(onMqttMessage);
  while (!mqtt.connected()) {
    if (mqtt.connect("esp32-cam-rostro", nullptr, nullptr, TOPIC_ESTADO, 1, true, ESTADO_OFFLINE)) {
      mqtt.publish(TOPIC_ESTADO, ESTADO_ONLINE, true);
      mqtt.subscribe(TOPIC_ROSTRO);
      mqtt.subscribe(TOPIC_PROBAR);
      Serial.println("MQTT OK, suscrito a planta/camara/rostro");
    } else {
      Serial.printf("MQTT fallo, state()=%d, reintento en 1s\n", mqtt.state());
      delay(1000);
    }
  }
}

// Captura y manda una foto. Devuelve el codigo HTTP; el cuerpo JSON queda en *cuerpo.
int enviarFoto(const String &ruta, String *cuerpo) {
  camera_fb_t *foto = esp_camera_fb_get();
  if (!foto) {
    Serial.println("esp_camera_fb_get devolvio NULL");
    return -1;
  }
  unsigned long t0 = millis();
  int codigo = postJpeg(String(API_URL) + ruta, CAMARA_TOKEN, foto, cuerpo);
  Serial.printf("  foto %u bytes -> HTTP %d en %lu ms\n", (unsigned)foto->len, codigo, millis() - t0);
  esp_camera_fb_return(foto);
  return codigo;
}

void probar() {
  String cuerpo;
  int codigo = enviarFoto("/api/camara/prueba?version=rostro", &cuerpo);
  Serial.printf("Foto de prueba: HTTP %d %s\n", codigo, cuerpo.c_str());
}

void verificar(int usuarioId) {
  String ruta = "/api/rostro/verificar?usuario_id=" + String(usuarioId);
  unsigned long inicio = millis();
  while (millis() - inicio < VENTANA_VERIFICAR_MS) {
    String cuerpo;
    int codigo = enviarFoto(ruta, &cuerpo);
    if (codigo == 200) {
      JsonDocument doc;
      if (deserializeJson(doc, cuerpo) == DeserializationError::Ok) {
        Serial.printf("  servidor: %s\n", cuerpo.c_str());
        if (doc["listo"] | false) return;
      }
    }
    unsigned long hasta = millis() + ENTRE_FOTOS_VERIFICAR_MS;
    while (millis() < hasta) {
      mqtt.loop();
      delay(10);
    }
  }
  Serial.println("Ventana de verificacion agotada");
}

void enrolar(int usuarioId) {
  String ruta = "/api/rostro/muestra?usuario_id=" + String(usuarioId);
  int guardadas = 0;
  for (int i = 0; i < FOTOS_ENROLAR; i++) {
    String cuerpo;
    if (enviarFoto(ruta, &cuerpo) == 200) {
      JsonDocument doc;
      if (deserializeJson(doc, cuerpo) == DeserializationError::Ok && (doc["ok"] | false)) guardadas++;
      Serial.printf("  servidor: %s\n", cuerpo.c_str());
    }
    delay(ENTRE_FOTOS_ENROLAR_MS);
  }
  Serial.printf("Enrolamiento: %d/%d muestras guardadas\n", guardadas, FOTOS_ENROLAR);
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

  if (probarPendiente) {
    probarPendiente = false;
    probar();
  }

  if (modoPendiente != NINGUNO) {
    Modo modo = modoPendiente;
    modoPendiente = NINGUNO;
    if (modo == VERIFICAR) verificar(usuarioPendiente);
    else enrolar(usuarioPendiente);
  }
}
