import asyncio
import logging
import threading
import time
from dataclasses import dataclass

import cv2
import numpy as np

from app import mqtt_client
from app.camara import guardar_jpeg, modelos_dir
from app.config import settings
from app.db import pool
from app.ws import manager

logger = logging.getLogger("rostro")

YUNET = "face_detection_yunet_2023mar.onnx"
SFACE = "face_recognition_sface_2021dec.onnx"
TOPIC_CAMARA_ROSTRO = "planta/camara/rostro"

_detector = None
_reconocedor = None
# Los objetos DNN de OpenCV no son thread-safe y las inferencias van a to_thread.
_lock = threading.Lock()


@dataclass
class LoginPendiente:
    usuario_id: int
    nombre: str
    intento: int
    rechazos: int = 0
    temporizador: asyncio.Task | None = None


_pendiente: LoginPendiente | None = None


def modelos_disponibles() -> bool:
    return (modelos_dir() / YUNET).exists() and (modelos_dir() / SFACE).exists()


def cargar() -> bool:
    """Carga YuNet (detector) y SFace (embeddings). Devuelve False si faltan
    los .onnx — se bajan una vez con server/ml/descargar_modelos.py."""
    global _detector, _reconocedor
    if not modelos_disponibles():
        return False
    _detector = cv2.FaceDetectorYN.create(str(modelos_dir() / YUNET), "", (320, 320), 0.8, 0.3, 5000)
    _reconocedor = cv2.FaceRecognizerSF.create(str(modelos_dir() / SFACE), "")
    logger.info("Modelos de rostro cargados")
    return True


def listo() -> bool:
    return _detector is not None and _reconocedor is not None


def _procesar(jpeg: bytes) -> tuple[int, float | None, np.ndarray | None]:
    """(caras detectadas, score de la más clara, su embedding SFace de 128
    floats normalizados). Bloqueante: llamar con to_thread."""
    imagen = cv2.imdecode(np.frombuffer(jpeg, np.uint8), cv2.IMREAD_COLOR)
    if imagen is None:
        return 0, None, None
    alto, ancho = imagen.shape[:2]
    with _lock:
        _detector.setInputSize((ancho, alto))
        _, caras = _detector.detect(imagen)
        if caras is None or len(caras) == 0:
            return 0, None, None
        cara = caras[int(np.argmax(caras[:, 14]))]  # la de mayor score
        alineada = _reconocedor.alignCrop(imagen, cara)
        vector = _reconocedor.feature(alineada).flatten().astype(np.float32)
    norma = np.linalg.norm(vector)
    return len(caras), float(cara[14]), (vector / norma if norma > 0 else None)


def embedding(jpeg: bytes) -> np.ndarray | None:
    """Embedding de la cara más clara de la foto, o None si no hay ninguna."""
    return _procesar(jpeg)[2]


async def analizar(jpeg: bytes) -> dict:
    """Para la foto de prueba: cuántas caras ve y a quién de los enrolados se
    parece (la misma comparación que hace el login, sin decidir nada)."""
    caras, score, vector = await asyncio.to_thread(_procesar, jpeg)
    coincidencias = []
    if vector is not None:
        filas = await pool().fetch(
            "SELECT u.nombre, r.embedding FROM rostros r JOIN usuarios u ON u.id = r.usuario_id"
        )
        mejor: dict[str, float] = {}
        for f in filas:
            ref = np.array(f["embedding"], dtype=np.float32)
            norma = np.linalg.norm(ref)
            sim = float((ref / norma) @ vector) if norma > 0 else 0.0
            mejor[f["nombre"]] = max(sim, mejor.get(f["nombre"], -1.0))
        coincidencias = [
            {"nombre": n, "similitud": round(s, 3)} for n, s in sorted(mejor.items(), key=lambda kv: -kv[1])
        ]
    return {
        "caras": caras,
        "score": None if score is None else round(score, 3),
        "umbral": settings.rostro_umbral,
        "coincidencias": coincidencias,
    }


async def _embeddings_de(usuario_id: int) -> np.ndarray:
    filas = await pool().fetch("SELECT embedding FROM rostros WHERE usuario_id = $1", usuario_id)
    if not filas:
        return np.empty((0, 128), dtype=np.float32)
    matriz = np.array([f["embedding"] for f in filas], dtype=np.float32)
    normas = np.linalg.norm(matriz, axis=1, keepdims=True)
    return matriz / np.where(normas == 0, 1, normas)


async def enrolar_muestra(usuario_id: int, jpeg: bytes) -> dict:
    vector = await asyncio.to_thread(embedding, jpeg)
    if vector is None:
        return {"ok": False, "motivo": "sin_rostro"}
    ruta = guardar_jpeg("rostro", f"e{usuario_id}_{int(time.time() * 1000)}.jpg", jpeg)
    await pool().execute(
        "INSERT INTO rostros (usuario_id, embedding, imagen) VALUES ($1, $2, $3)",
        usuario_id, vector.tolist(), ruta,
    )
    total = await pool().fetchval("SELECT COUNT(*) FROM rostros WHERE usuario_id = $1", usuario_id)
    return {"ok": True, "muestras": total}


async def iniciar_login(usuario_id: int, nombre: str, intento: int) -> None:
    """PIN correcto con LOGIN_ROSTRO activo: el login queda pendiente hasta que
    la cámara confirme la cara o venza la ventana."""
    global _pendiente
    if _pendiente is not None:
        await _cerrar(exito=False, motivo="rostro")

    pendiente = LoginPendiente(usuario_id=usuario_id, nombre=nombre, intento=intento)
    pendiente.temporizador = asyncio.create_task(_vencer(pendiente))
    _pendiente = pendiente

    mqtt_client.publish(
        mqtt_client.TOPIC_LOGIN_RESULTADO,
        {"exito": False, "requiere_rostro": True, "nombre": nombre, "bloqueado": False, "intento": intento},
    )
    mqtt_client.publish(TOPIC_CAMARA_ROSTRO, {"modo": "verificar", "usuario_id": usuario_id})


async def _vencer(pendiente: LoginPendiente) -> None:
    await asyncio.sleep(settings.rostro_ventana_s)
    if _pendiente is pendiente:
        await _cerrar(exito=False, motivo="rostro")


async def _cerrar(exito: bool, motivo: str | None = None) -> None:
    global _pendiente
    pendiente, _pendiente = _pendiente, None
    if pendiente is None:
        return
    if pendiente.temporizador and pendiente.temporizador is not asyncio.current_task():
        pendiente.temporizador.cancel()
    await mqtt_client.finalizar_login(
        pendiente.usuario_id, pendiente.nombre, exito, pendiente.intento,
        origen="keypad_rostro", motivo=None if exito else motivo,
    )


async def verificar(usuario_id: int, jpeg: bytes) -> dict:
    """Una foto de la cámara durante un login pendiente. `listo` le dice a la
    placa si debe dejar de mandar fotos."""
    pendiente = _pendiente
    if pendiente is None or pendiente.usuario_id != usuario_id:
        return {"listo": True, "motivo": "sin_login_pendiente"}

    vector = await asyncio.to_thread(embedding, jpeg)
    if vector is None:
        return {"listo": False, "motivo": "sin_rostro"}
    if _pendiente is not pendiente:  # venció o lo resolvió otra foto mientras se procesaba
        return {"listo": True, "motivo": "sin_login_pendiente"}

    muestras = await _embeddings_de(usuario_id)
    similitud = float((muestras @ vector).max()) if len(muestras) else None
    exito = similitud is not None and similitud >= settings.rostro_umbral

    ruta = guardar_jpeg("rostro", f"v{usuario_id}_{int(time.time() * 1000)}.jpg", jpeg)
    await pool().execute(
        "INSERT INTO verificaciones_rostro (usuario_id, similitud, exito, imagen) VALUES ($1, $2, $3, $4)",
        usuario_id, similitud, exito, ruta,
    )
    await manager.broadcast({
        "type": "verificacion_rostro", "usuario_id": usuario_id, "nombre": pendiente.nombre,
        "similitud": similitud, "exito": exito, "umbral": settings.rostro_umbral, "imagen": ruta,
    })

    if exito:
        await _cerrar(exito=True)
        return {"listo": True, "exito": True, "similitud": similitud}

    pendiente.rechazos += 1
    if pendiente.rechazos >= settings.rostro_max_rechazos or not len(muestras):
        await _cerrar(exito=False, motivo="rostro")
        return {"listo": True, "exito": False, "similitud": similitud}
    return {"listo": False, "exito": False, "similitud": similitud}
