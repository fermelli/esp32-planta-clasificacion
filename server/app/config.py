from pydantic_settings import BaseSettings, SettingsConfigDict


class Settings(BaseSettings):
    model_config = SettingsConfigDict(env_file=".env", extra="ignore")

    database_url: str = "postgresql://planta:planta@localhost:5432/planta"
    mqtt_host: str = "localhost"
    mqtt_port: int = 1883

    jwt_secret: str = "cambia-esto"
    jwt_expire_minutes: int = 480

    seed_usuario_1_nombre: str = "operador1"
    seed_usuario_1_pin: str = "1234"
    seed_usuario_1_password: str = "operador1pass"
    seed_usuario_2_nombre: str = "operador2"
    seed_usuario_2_pin: str = "5678"
    seed_usuario_2_password: str = "operador2pass"


settings = Settings()
