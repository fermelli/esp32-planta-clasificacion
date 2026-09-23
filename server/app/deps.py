import jwt
from fastapi import Depends, HTTPException, status
from fastapi.security import HTTPAuthorizationCredentials, HTTPBearer

from app.security import decode_access_token

_bearer = HTTPBearer()


async def get_current_user(creds: HTTPAuthorizationCredentials = Depends(_bearer)) -> dict:
    try:
        payload = decode_access_token(creds.credentials)
    except jwt.PyJWTError:
        raise HTTPException(status.HTTP_401_UNAUTHORIZED, "Token inválido o expirado")
    return {"id": int(payload["sub"]), "nombre": payload["nombre"]}
