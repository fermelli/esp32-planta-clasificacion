import secrets
from pathlib import Path

from fastapi import Header, HTTPException, status

from app.config import settings

RAIZ = Path(__file__).resolve().parent.parent


def _resolver(valor: str) -> Path:
    ruta = Path(valor)
    return ruta if ruta.is_absolute() else RAIZ / ruta


def capturas_dir() -> Path:
    ruta = _resolver(settings.capturas_dir)
    ruta.mkdir(parents=True, exist_ok=True)
    return ruta


def modelos_dir() -> Path:
    ruta = _resolver(settings.modelos_dir)
    ruta.mkdir(parents=True, exist_ok=True)
    return ruta


async def token_camara(x_camara_token: str = Header(default="")) -> None:
    """La placa no tiene JWT: se autentica con un token compartido en el header."""
    esperado = settings.camara_token.encode()
    if not secrets.compare_digest(x_camara_token.encode(), esperado):
        raise HTTPException(status.HTTP_401_UNAUTHORIZED, "Token de cámara inválido")


def guardar_jpeg(subdir: str, nombre: str, datos: bytes) -> str:
    """Guarda el JPEG en capturas/<subdir>/<nombre> y devuelve la ruta relativa
    (la misma que sirve /capturas/<ruta>)."""
    destino = capturas_dir() / subdir
    destino.mkdir(parents=True, exist_ok=True)
    (destino / nombre).write_bytes(datos)
    return f"{subdir}/{nombre}"
