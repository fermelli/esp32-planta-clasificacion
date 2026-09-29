import asyncpg
from fastapi import APIRouter, Depends, HTTPException, status

from app.db import pool
from app.deps import get_current_user
from app.schemas import UsuarioCreate, UsuarioOut
from app.security import hash_secret

router = APIRouter(prefix="/api/usuarios", tags=["usuarios"], dependencies=[Depends(get_current_user)])


@router.get("", response_model=list[UsuarioOut])
async def listar_usuarios() -> list[UsuarioOut]:
    filas = await pool().fetch("SELECT id, nombre, creado_en FROM usuarios ORDER BY id")
    return [UsuarioOut(**dict(f)) for f in filas]


@router.post("", response_model=UsuarioOut, status_code=status.HTTP_201_CREATED)
async def registrar_usuario(body: UsuarioCreate) -> UsuarioOut:
    try:
        fila = await pool().fetchrow(
            "INSERT INTO usuarios (nombre, password_hash) VALUES ($1, $2) RETURNING id, nombre, creado_en",
            body.nombre, hash_secret(body.password),
        )
    except asyncpg.UniqueViolationError:
        raise HTTPException(status.HTTP_409_CONFLICT, "Ya existe un operador con ese nombre")
    return UsuarioOut(**dict(fila))
