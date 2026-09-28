#pragma once
// Base común de los firmwares de cámara (esp32-cam-rostro y esp32-cam-color):
// pinout de la ESP32-S3-CAM, captura JPEG, WiFi y POST de la foto al servidor.

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include "esp_camera.h"

// Pinout típico de las ESP32-S3-CAM de 20 pines con OV2640/OV3660 (estilo
// Freenove). Si tu placa difiere, redefinir estos macros en config.h ANTES de
// incluir este header, o mirar el esquema de tu placa.
#ifndef CAM_PIN_PWDN
#define CAM_PIN_PWDN -1
#endif
#ifndef CAM_PIN_RESET
#define CAM_PIN_RESET -1
#endif
#ifndef CAM_PIN_XCLK
#define CAM_PIN_XCLK 15
#endif
#ifndef CAM_PIN_SIOD
#define CAM_PIN_SIOD 4
#endif
#ifndef CAM_PIN_SIOC
#define CAM_PIN_SIOC 5
#endif
#ifndef CAM_PIN_D0
#define CAM_PIN_D0 11
#define CAM_PIN_D1 9
#define CAM_PIN_D2 8
#define CAM_PIN_D3 10
#define CAM_PIN_D4 12
#define CAM_PIN_D5 18
#define CAM_PIN_D6 17
#define CAM_PIN_D7 16
#endif
#ifndef CAM_PIN_VSYNC
#define CAM_PIN_VSYNC 6
#endif
#ifndef CAM_PIN_HREF
#define CAM_PIN_HREF 7
#endif
#ifndef CAM_PIN_PCLK
#define CAM_PIN_PCLK 13
#endif

// Según cómo esté montado el sensor la imagen sale invertida: poner 1 si hace falta.
#ifndef CAM_VFLIP
#define CAM_VFLIP 0
#endif
#ifndef CAM_HMIRROR
#define CAM_HMIRROR 0
#endif

inline bool iniciarCamara() {
  camera_config_t cfg = {};
  cfg.ledc_channel = LEDC_CHANNEL_0;
  cfg.ledc_timer = LEDC_TIMER_0;
  cfg.pin_d0 = CAM_PIN_D0;
  cfg.pin_d1 = CAM_PIN_D1;
  cfg.pin_d2 = CAM_PIN_D2;
  cfg.pin_d3 = CAM_PIN_D3;
  cfg.pin_d4 = CAM_PIN_D4;
  cfg.pin_d5 = CAM_PIN_D5;
  cfg.pin_d6 = CAM_PIN_D6;
  cfg.pin_d7 = CAM_PIN_D7;
  cfg.pin_xclk = CAM_PIN_XCLK;
  cfg.pin_pclk = CAM_PIN_PCLK;
  cfg.pin_vsync = CAM_PIN_VSYNC;
  cfg.pin_href = CAM_PIN_HREF;
  cfg.pin_sccb_sda = CAM_PIN_SIOD;
  cfg.pin_sccb_scl = CAM_PIN_SIOC;
  cfg.pin_pwdn = CAM_PIN_PWDN;
  cfg.pin_reset = CAM_PIN_RESET;
  cfg.xclk_freq_hz = 20000000;
  cfg.pixel_format = PIXFORMAT_JPEG;
  cfg.frame_size = FRAMESIZE_VGA;  // 640x480: de sobra para detectar caras y color
  cfg.jpeg_quality = 12;
  cfg.fb_count = 2;
  cfg.fb_location = CAMERA_FB_IN_PSRAM;
  cfg.grab_mode = CAMERA_GRAB_LATEST;  // siempre el frame más nuevo, no uno viejo del buffer

  esp_err_t err = esp_camera_init(&cfg);
  if (err != ESP_OK) {
    Serial.printf("esp_camera_init fallo: 0x%x (revisar pinout y PSRAM: memory_type en platformio.ini)\n", err);
    return false;
  }
  sensor_t *sensor = esp_camera_sensor_get();
  if (sensor) {
    sensor->set_vflip(sensor, CAM_VFLIP);
    sensor->set_hmirror(sensor, CAM_HMIRROR);
    Serial.printf("Camara OK, sensor PID=0x%04x (OV3660=0x3660, OV2640=0x2642)\n", sensor->id.PID);
  }
  return true;
}

inline void conectarWifi(const char *ssid, const char *clave) {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, clave);
  Serial.printf("Conectando a WiFi \"%s\"", ssid);
  unsigned long inicio = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
    if (millis() - inicio > 20000) {
      Serial.printf("\nSin WiFi tras 20s (status=%d): revisar SSID/clave y que la red sea de 2.4 GHz\n",
                    WiFi.status());
      inicio = millis();
    }
  }
  Serial.printf("\nWiFi OK, IP %s\n", WiFi.localIP().toString().c_str());
}

// Devuelve el codigo HTTP (o <=0 si fallo la conexion) y deja el cuerpo en *respuesta.
inline int postJpeg(const String &url, const char *token, const camera_fb_t *foto, String *respuesta) {
  HTTPClient http;
  http.setTimeout(8000);
  http.begin(url);
  http.addHeader("Content-Type", "image/jpeg");
  http.addHeader("X-Camara-Token", token);
  int codigo = http.POST(const_cast<uint8_t *>(foto->buf), foto->len);
  if (codigo > 0 && respuesta) *respuesta = http.getString();
  http.end();
  return codigo;
}
