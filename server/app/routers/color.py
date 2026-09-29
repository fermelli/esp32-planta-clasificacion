import asyncio
import json

from fastapi import APIRouter, Depends, HTTPException, Request, status

from app import color_ia
from app.camara import guardar_jpeg, token_camara
from app.config import settings
from app.db import pool
from app.deps import get_current_user
from app.schemas import CapturaColorOut, ResumenColorOut
from app.ws import manager

# La placa no tiene JWT: /captura se autentica con X-Camara-Token.
router_camara = APIRouter(prefix="/api/color", tags=["color-camara"], dependencies=[Depends(token_camara)])
router = APIRouter(prefix="/api/color", tags=["color"], dependencies=[Depends(get_current_user)])

_entrenando = asyncio.Lock()


async def procesar_captura(evento_id: int, jpeg: bytes) -> dict:
    """Guarda la foto de una caja, la clasifica y actualiza el evento. Usado
    tanto por el endpoint HTTP de la placa como por la captura directa desde
    la webcam (ver mqtt_client._manejar_sorter_evento)."""
    if not jpeg:
        raise HTTPException(status.HTTP_400_BAD_REQUEST, "Cuerpo vacío: se esperaba un JPEG")
    evento = await pool().fetchrow("SELECT color FROM eventos_caja WHERE id = $1", evento_id)
    if evento is None:
        raise HTTPException(status.HTTP_404_NOT_FOUND, "Evento inexistente")

    ruta = guardar_jpeg("color", f"{evento_id}.jpg", jpeg)
    prediccion = await asyncio.to_thread(color_ia.predecir, jpeg)
    color, confianza, probabilidades = prediccion or (None, None, None)

    await pool().execute(
        """
        INSERT INTO capturas_camara (evento_id, imagen, color_ia, confianza, probabilidades)
        VALUES ($1, $2, $3, $4, $5::jsonb)
        ON CONFLICT (evento_id) DO UPDATE
        SET imagen = EXCLUDED.imagen, color_ia = EXCLUDED.color_ia,
            confianza = EXCLUDED.confianza, probabilidades = EXCLUDED.probabilidades
        """,
        evento_id, ruta, color, confianza, json.dumps(probabilidades) if probabilidades else None,
    )
    if color:
        await pool().execute("UPDATE eventos_caja SET color_ml = $1 WHERE id = $2", color, evento_id)

    resultado = {
        "evento_id": evento_id,
        "color_sensor": evento["color"],
        "color_ia": color,
        "confianza": confianza,
        "coincide": None if color is None else color == evento["color"],
        "imagen": ruta,
    }
    await manager.broadcast({"type": "clasificacion_camara", **resultado})
    return resultado


@router_camara.post("/captura")
async def captura(evento_id: int, request: Request) -> dict:
    return await procesar_captura(evento_id, await request.body())


@router.get("/capturas", response_model=list[CapturaColorOut])
async def listar_capturas(limite: int = 20) -> list[CapturaColorOut]:
    filas = await pool().fetch(
        """
        SELECT c.evento_id, e.color AS color_sensor, c.color_ia, c.confianza, c.imagen, c.creado_en
        FROM capturas_camara c
        JOIN eventos_caja e ON e.id = c.evento_id
        ORDER BY c.creado_en DESC
        LIMIT $1
        """,
        limite,
    )
    return [
        CapturaColorOut(
            **dict(f), coincide=None if f["color_ia"] is None else f["color_ia"] == f["color_sensor"]
        )
        for f in filas
    ]


@router.get("/resumen", response_model=ResumenColorOut)
async def resumen() -> ResumenColorOut:
    fila = await pool().fetchrow(
        """
        SELECT COUNT(*) AS total, COUNT(c.color_ia) AS con_ia,
               COUNT(*) FILTER (WHERE c.color_ia = e.color) AS coinciden
        FROM capturas_camara c JOIN eventos_caja e ON e.id = c.evento_id
        """
    )
    dataset = await pool().fetch(
        """
        SELECT COALESCE(e.etiqueta_real, e.color) AS etiqueta, COUNT(*) AS n
        FROM capturas_camara c JOIN eventos_caja e ON e.id = c.evento_id
        GROUP BY 1 ORDER BY 1
        """
    )
    return ResumenColorOut(
        total=fila["total"],
        con_ia=fila["con_ia"],
        coinciden=fila["coinciden"],
        acuerdo_pct=round(100 * fila["coinciden"] / fila["con_ia"], 1) if fila["con_ia"] else None,
        dataset={d["etiqueta"]: d["n"] for d in dataset},
        modelo_entrenado=color_ia.hay_modelo(),
        camara_color=settings.camara_color,
        min_por_clase=color_ia.MIN_POR_CLASE,
        metricas=color_ia.metricas(),
    )


@router.post("/entrenar")
async def entrenar() -> dict:
    if _entrenando.locked():
        raise HTTPException(status.HTTP_409_CONFLICT, "Ya hay un entrenamiento en curso")
    async with _entrenando:
        filas = await pool().fetch(
            """
            SELECT c.imagen, COALESCE(e.etiqueta_real, e.color) AS etiqueta
            FROM capturas_camara c JOIN eventos_caja e ON e.id = c.evento_id
            """
        )
        try:
            return await asyncio.to_thread(color_ia.entrenar, [(f["imagen"], f["etiqueta"]) for f in filas])
        except ValueError as e:
            raise HTTPException(status.HTTP_400_BAD_REQUEST, str(e))
