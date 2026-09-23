import asyncio
from datetime import datetime, timedelta, timezone

import bcrypt
import jwt

from app.config import settings


def hash_secret(secret: str) -> str:
    return bcrypt.hashpw(secret.encode(), bcrypt.gensalt()).decode()


async def verify_secret(secret: str, hashed: str) -> bool:
    # CPU-bound: en un hilo aparte para no bloquear el event loop compartido
    # con MQTT y WebSocket mientras se verifica la contraseña.
    return await asyncio.to_thread(bcrypt.checkpw, secret.encode(), hashed.encode())


def create_access_token(usuario_id: int, nombre: str) -> str:
    expira = datetime.now(timezone.utc) + timedelta(minutes=settings.jwt_expire_minutes)
    payload = {"sub": str(usuario_id), "nombre": nombre, "exp": expira}
    return jwt.encode(payload, settings.jwt_secret, algorithm="HS256")


def decode_access_token(token: str) -> dict:
    return jwt.decode(token, settings.jwt_secret, algorithms=["HS256"])
