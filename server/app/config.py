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


settings = Settings()
