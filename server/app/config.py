from pydantic_settings import BaseSettings, SettingsConfigDict


class Settings(BaseSettings):
    model_config = SettingsConfigDict(env_file=".env", extra="ignore")

    database_url: str = "postgresql://planta:planta@localhost:5432/planta"
    mqtt_host: str = "localhost"
    mqtt_port: int = 1883

    jwt_secret: str = "cambia-esto"
    jwt_expire_minutes: int = 480

    # Un solo secreto por usuario: se teclea igual en el ESP32 y en la web.
    seed_usuario_1_nombre: str = "operador1"
    seed_usuario_1_secreto: str = "1234"
    seed_usuario_2_nombre: str = "operador2"
    seed_usuario_2_secreto: str = "5678"

    # Cámara ESP32-S3-CAM. Las dos versiones vienen apagadas: con ambos flags
    # en false el servidor se comporta igual que antes de tener cámara.
    camara_token: str = "cambia-el-token-de-la-camara"
    capturas_dir: str = "capturas"
    modelos_dir: str = "modelos"

    login_rostro: bool = False
    rostro_umbral: float = 0.363  # umbral coseno recomendado por OpenCV para SFace
    rostro_ventana_s: int = 15
    rostro_max_rechazos: int = 3

    camara_color: bool = False
    color_recorte: float = 0.6  # fracción central de la foto que mira el clasificador de color

    # Webcam USB en la laptop, en vez de la ESP32-CAM: se captura la foto
    # localmente con OpenCV, sin MQTT ni HTTP de por medio. Independiente de
    # login_rostro/camara_color, que siguen controlando si el flujo esta activo.
    rostro_webcam: bool = False
    color_webcam: bool = False
    webcam_indice: int = 0


settings = Settings()
