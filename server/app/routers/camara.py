import asyncio
import time

from fastapi import APIRouter, Depends, HTTPException, Request, status

from app import color_ia, mqtt_client, rostro, webcam
from app.camara import avisos, calidad, guardar_jpeg, token_camara
from app.config import settings
from app.deps import get_current_user
from app.schemas import CamaraConfigOut, CamaraVersionOut
from app.ws import manager

router = APIRouter(prefix="/api/camara", tags=["camara"], dependencies=[Depends(get_current_user)])
# La placa se autentica con X-Camara-Token, no con JWT.
router_placa = APIRouter(prefix="/api/camara", tags=["camara-placa"], dependencies=[Depends(token_camara)])

VERSIONES = ("rostro", "color")
_ultima_prueba: dict[str, dict] = {}


def usa_webcam(version: str) -> bool:
    """True si esta versión saca sus fotos de la webcam de la laptop en vez
    de esperar a que las suba una ESP32-CAM."""
    return settings.rostro_webcam if version == "rostro" else settings.color_webcam


def _version(nombre: str) -> CamaraVersionOut:
    flag = mqtt_client.flag_activo(nombre)
    online = usa_webcam(nombre) or mqtt_client.camaras_online[nombre]
    return CamaraVersionOut(flag=flag, online=online, activo=flag and online)


def _validar(version: str) -> None:
    if version not in VERSIONES:
        raise HTTPException(status.HTTP_404_NOT_FOUND, "Versión de cámara inexistente")


async def procesar_prueba(version: str, jpeg: bytes) -> dict:
    """Calidad + análisis (rostro o color) de una foto de prueba, venga de
    donde venga (placa o webcam). Guarda, deja en /prueba/{version} y avisa
    por WebSocket."""
    cal = await asyncio.to_thread(calidad, jpeg) if jpeg else None
    if cal is None:
        raise HTTPException(status.HTTP_400_BAD_REQUEST, "El cuerpo no es un JPEG legible")

    ruta = guardar_jpeg("prueba", f"{version}.jpg", jpeg)
    lista_avisos = avisos(cal)
    resultado = {
        "version": version,
        "imagen": ruta,
        "ts": int(time.time() * 1000),
        **cal,
        "avisos": lista_avisos,
        "rostro": None,
        "color": None,
    }
    if version == "rostro":
        if rostro.listo():
            resultado["rostro"] = await rostro.analizar(jpeg)
        else:
            lista_avisos.append("Modelos de rostro no cargados: correr ml/descargar_modelos.py")
    else:
        prediccion = await asyncio.to_thread(color_ia.predecir, jpeg)
        resultado["color"] = {
            "color_ia": prediccion[0] if prediccion else None,
            "confianza": prediccion[1] if prediccion else None,
            "rgb_medio": await asyncio.to_thread(color_ia.rgb_medio, jpeg),
        }

    _ultima_prueba[version] = resultado
    await manager.broadcast({"type": "prueba_camara", **resultado})
    return resultado


@router.get("/config", response_model=CamaraConfigOut)
async def configuracion() -> CamaraConfigOut:
    """Qué versiones de la cámara están activadas y cuáles tienen una placa
    (o la webcam) disponible. El dashboard lo usa para mostrar solo lo que
    corresponde."""
    return CamaraConfigOut(rostro=_version("rostro"), color=_version("color"))


@router.post("/probar/{version}")
async def probar(version: str) -> dict:
    """Con webcam: saca la foto ahora mismo y devuelve el resultado ya
    procesado. Con ESP32-CAM: le pide la foto a la placa por MQTT; el
    resultado llega después por WebSocket (`prueba_camara`) y queda en
    GET /prueba/{version}."""
    _validar(version)
    if usa_webcam(version):
        jpeg = await asyncio.to_thread(webcam.capturar_jpeg)
        if jpeg is None:
            raise HTTPException(status.HTTP_502_BAD_GATEWAY, "No se pudo capturar desde la webcam")
        return await procesar_prueba(version, jpeg)
    if not mqtt_client.camaras_online[version]:
        raise HTTPException(status.HTTP_409_CONFLICT, f"La cámara de {version} no está conectada")
    mqtt_client.publish(mqtt_client.TOPIC_CAMARA_PROBAR, {"version": version})
    return {"ok": True}


@router.get("/prueba/{version}")
async def ultima_prueba(version: str) -> dict | None:
    _validar(version)
    return _ultima_prueba.get(version)


@router_placa.post("/prueba")
async def recibir_prueba(version: str, request: Request) -> dict:
    _validar(version)
    return await procesar_prueba(version, await request.body())
