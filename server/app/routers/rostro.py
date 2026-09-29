import asyncio

from fastapi import APIRouter, Depends, HTTPException, Request, status

from app import mqtt_client, rostro
from app.camara import token_camara
from app.config import settings
from app.db import pool
from app.deps import get_current_user
from app.schemas import RostroConfigOut, RostroUsuarioOut, VerificacionRostroOut

# La placa no tiene JWT: estos dos endpoints se autentican con X-Camara-Token.
router_camara = APIRouter(prefix="/api/rostro", tags=["rostro-camara"], dependencies=[Depends(token_camara)])
router = APIRouter(prefix="/api/rostro", tags=["rostro"], dependencies=[Depends(get_current_user)])


def _exigir_modelos() -> None:
    if not rostro.listo():
        raise HTTPException(
            status.HTTP_503_SERVICE_UNAVAILABLE,
            "Modelos de rostro no cargados: correr server/ml/descargar_modelos.py y reiniciar",
        )


@router_camara.post("/verificar")
async def verificar(usuario_id: int, request: Request) -> dict:
    _exigir_modelos()
    return await rostro.verificar(usuario_id, await request.body())


@router_camara.post("/muestra")
async def muestra(usuario_id: int, request: Request) -> dict:
    _exigir_modelos()
    return await rostro.enrolar_muestra(usuario_id, await request.body())


@router.get("/config", response_model=RostroConfigOut)
async def configuracion() -> RostroConfigOut:
    return RostroConfigOut(
        login_rostro=settings.login_rostro,
        umbral=settings.rostro_umbral,
        modelos_cargados=rostro.listo(),
    )


@router.get("/usuarios", response_model=list[RostroUsuarioOut])
async def listar_usuarios() -> list[RostroUsuarioOut]:
    filas = await pool().fetch(
        """
        SELECT u.id AS usuario_id, u.nombre, COUNT(r.id) AS muestras
        FROM usuarios u
        LEFT JOIN rostros r ON r.usuario_id = u.id
        GROUP BY u.id, u.nombre
        ORDER BY u.id
        """
    )
    return [RostroUsuarioOut(**dict(f)) for f in filas]


@router.post("/enrolar/{usuario_id}")
async def enrolar(usuario_id: int) -> dict:
    existe = await pool().fetchval("SELECT 1 FROM usuarios WHERE id = $1", usuario_id)
    if not existe:
        raise HTTPException(status.HTTP_404_NOT_FOUND, "Usuario inexistente")
    if settings.rostro_webcam:
        # Tarea de fondo: no bloquea la respuesta, el dashboard se entera
        # sondeando /usuarios (ver RostroView.vue) igual que con la placa.
        asyncio.create_task(rostro.enrolar_con_webcam(usuario_id))
    else:
        mqtt_client.publish(rostro.TOPIC_CAMARA_ROSTRO, {"modo": "enrolar", "usuario_id": usuario_id})
    return {"ok": True}


@router.delete("/muestras/{usuario_id}")
async def borrar_muestras(usuario_id: int) -> dict:
    estado = await pool().execute("DELETE FROM rostros WHERE usuario_id = $1", usuario_id)
    return {"ok": True, "borradas": int(estado.split()[-1])}


@router.get("/verificaciones", response_model=list[VerificacionRostroOut])
async def listar_verificaciones(limite: int = 30) -> list[VerificacionRostroOut]:
    filas = await pool().fetch(
        """
        SELECT v.id, u.nombre AS usuario_nombre, v.similitud, v.exito, v.imagen, v.creado_en
        FROM verificaciones_rostro v
        JOIN usuarios u ON u.id = v.usuario_id
        ORDER BY v.creado_en DESC
        LIMIT $1
        """,
        limite,
    )
    return [VerificacionRostroOut(**dict(f)) for f in filas]
