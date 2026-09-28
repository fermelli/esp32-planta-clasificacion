import secrets
from pathlib import Path

import cv2
import numpy as np
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


def calidad(jpeg: bytes) -> dict | None:
    """Métricas para saber si la cámara está bien montada e iluminada, o None si
    no es un JPEG legible. Bloqueante: llamar con to_thread."""
    imagen = cv2.imdecode(np.frombuffer(jpeg, np.uint8), cv2.IMREAD_COLOR)
    if imagen is None:
        return None
    gris = cv2.cvtColor(imagen, cv2.COLOR_BGR2GRAY)
    alto, ancho = gris.shape
    return {
        "ancho": ancho,
        "alto": alto,
        "kb": round(len(jpeg) / 1024, 1),
        "brillo": round(float(gris.mean()), 1),  # 0 negro .. 255 blanco
        "nitidez": round(float(cv2.Laplacian(gris, cv2.CV_64F).var()), 1),  # más alto = más enfocada
    }


def avisos(cal: dict) -> list[str]:
    """Advertencias orientativas (los umbrales son una guía, no una norma)."""
    lista = []
    if cal["brillo"] < 60:
        lista.append("Imagen muy oscura: más luz, o apuntar lejos de un contraluz")
    elif cal["brillo"] > 200:
        lista.append("Imagen sobreexpuesta: hay demasiada luz o un reflejo directo")
    if cal["nitidez"] < 30:
        lista.append("Imagen poco nítida: revisar el enfoque del lente o la distancia")
    return lista
