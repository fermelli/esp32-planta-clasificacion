from datetime import datetime

from pydantic import BaseModel


class LoginRequest(BaseModel):
    nombre: str
    password: str


class LoginResponse(BaseModel):
    access_token: str
    token_type: str = "bearer"
    nombre: str


class IntentoOut(BaseModel):
    id: int
    usuario_nombre: str | None
    exito: bool
    origen: str
    creado_en: datetime


class AlertaOut(BaseModel):
    id: int
    tipo: str
    mensaje: str
    creado_en: datetime


class ComandoRequest(BaseModel):
    cmd: str
    arg: int = 0


class EventoCajaOut(BaseModel):
    id: int
    color: str
    conteo: int
    lote_completo: bool
    r: int
    g: int
    b: int
    c: int
    creado_en: datetime


class CamaraVersionOut(BaseModel):
    flag: bool  # la versión está activada en el .env del servidor
    online: bool  # hay una cámara con ese firmware conectada
    activo: bool  # flag y online: solo entonces la versión actúa


class CamaraConfigOut(BaseModel):
    rostro: CamaraVersionOut
    color: CamaraVersionOut


class RostroConfigOut(BaseModel):
    login_rostro: bool
    umbral: float
    modelos_cargados: bool


class RostroUsuarioOut(BaseModel):
    usuario_id: int
    nombre: str
    muestras: int


class VerificacionRostroOut(BaseModel):
    id: int
    usuario_nombre: str
    similitud: float | None
    exito: bool
    imagen: str | None
    creado_en: datetime


class CapturaColorOut(BaseModel):
    evento_id: int
    color_sensor: str
    color_ia: str | None
    confianza: float | None
    coincide: bool | None
    imagen: str
    creado_en: datetime


class ResumenColorOut(BaseModel):
    total: int
    con_ia: int
    coinciden: int
    acuerdo_pct: float | None
    dataset: dict[str, int]
    modelo_entrenado: bool
    camara_color: bool
    min_por_clase: int
    metricas: dict | None


class ConteoColorOut(BaseModel):
    color: str
    conteo_actual: int
    total_historico: int
    lotes_completados: int


class CajasPorHoraOut(BaseModel):
    hora: datetime
    color: str
    cantidad: int
