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
