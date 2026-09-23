from fastapi import APIRouter, Depends

from app.db import pool
from app.deps import get_current_user
from app.schemas import AlertaOut, IntentoOut

router = APIRouter(prefix="/api", tags=["intentos"], dependencies=[Depends(get_current_user)])


@router.get("/intentos", response_model=list[IntentoOut])
async def listar_intentos(limite: int = 50) -> list[IntentoOut]:
    filas = await pool().fetch(
        """
        SELECT i.id, u.nombre AS usuario_nombre, i.exito, i.origen, i.creado_en
        FROM intentos_login i
        LEFT JOIN usuarios u ON u.id = i.usuario_id
        ORDER BY i.creado_en DESC
        LIMIT $1
        """,
        limite,
    )
    return [IntentoOut(**dict(f)) for f in filas]


@router.get("/alertas", response_model=list[AlertaOut])
async def listar_alertas(limite: int = 50) -> list[AlertaOut]:
    filas = await pool().fetch(
        "SELECT id, tipo, mensaje, creado_en FROM alertas ORDER BY creado_en DESC LIMIT $1",
        limite,
    )
    return [AlertaOut(**dict(f)) for f in filas]
