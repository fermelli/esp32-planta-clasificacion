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


class ConteoColorOut(BaseModel):
    color: str
    conteo_actual: int
    total_historico: int
    lotes_completados: int


class CajasPorHoraOut(BaseModel):
    hora: datetime
    color: str
    cantidad: int
