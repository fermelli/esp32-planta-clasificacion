from fastapi import APIRouter, HTTPException, status

from app.db import pool
from app.schemas import LoginRequest, LoginResponse
from app.security import create_access_token, verify_secret

router = APIRouter(prefix="/api/auth", tags=["auth"])


@router.post("/login", response_model=LoginResponse)
async def login(body: LoginRequest) -> LoginResponse:
    fila = await pool().fetchrow(
        "SELECT id, nombre, password_hash FROM usuarios WHERE nombre = $1", body.nombre
    )

    exito = fila is not None and verify_secret(body.password, fila["password_hash"])

    await pool().execute(
        "INSERT INTO intentos_login (usuario_id, exito, origen) VALUES ($1, $2, 'web')",
        fila["id"] if fila else None, exito,
    )

    if not exito:
        raise HTTPException(status.HTTP_401_UNAUTHORIZED, "Usuario o contraseña incorrectos")

    token = create_access_token(fila["id"], fila["nombre"])
    return LoginResponse(access_token=token, nombre=fila["nombre"])
