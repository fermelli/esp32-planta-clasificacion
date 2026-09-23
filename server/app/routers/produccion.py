from fastapi import APIRouter, Depends

from app.db import pool
from app.deps import get_current_user
from app.schemas import CajasPorHoraOut, ConteoColorOut, EventoCajaOut

router = APIRouter(prefix="/api", tags=["produccion"], dependencies=[Depends(get_current_user)])


@router.get("/eventos", response_model=list[EventoCajaOut])
async def listar_eventos(limite: int = 50) -> list[EventoCajaOut]:
    filas = await pool().fetch(
        """
        SELECT id, color, conteo, lote_completo, r, g, b, c, creado_en
        FROM eventos_caja
        ORDER BY creado_en DESC
        LIMIT $1
        """,
        limite,
    )
    return [EventoCajaOut(**dict(f)) for f in filas]


@router.get("/conteos", response_model=list[ConteoColorOut])
async def listar_conteos() -> list[ConteoColorOut]:
    """Conteo actual = el 'conteo' que mandó el ESP32 en la última caja de ese
    color (ya lo calculó el hardware para los LEDs). Total histórico y lotes
    salen agregando toda la tabla."""
    filas = await pool().fetch(
        """
        WITH ultimo AS (
            SELECT DISTINCT ON (color) color, conteo
            FROM eventos_caja
            ORDER BY color, creado_en DESC
        )
        SELECT
            t.color,
            u.conteo AS conteo_actual,
            COUNT(*) AS total_historico,
            COUNT(*) FILTER (WHERE t.lote_completo) AS lotes_completados
        FROM eventos_caja t
        JOIN ultimo u ON u.color = t.color
        GROUP BY t.color, u.conteo
        ORDER BY t.color
        """
    )
    return [ConteoColorOut(**dict(f)) for f in filas]


@router.get("/produccion/historico", response_model=list[CajasPorHoraOut])
async def historico_por_hora(horas: int = 24) -> list[CajasPorHoraOut]:
    filas = await pool().fetch(
        """
        SELECT date_trunc('hour', creado_en) AS hora, color, COUNT(*) AS cantidad
        FROM eventos_caja
        WHERE creado_en > now() - ($1 || ' hours')::interval
        GROUP BY hora, color
        ORDER BY hora
        """,
        str(horas),
    )
    return [CajasPorHoraOut(**dict(f)) for f in filas]
