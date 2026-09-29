"""Captura de fotos con una webcam USB conectada a esta misma laptop, en vez
de esperar a que una ESP32-CAM las suba por HTTP. Mismo formato de salida
(bytes JPEG) que las placas, así el resto del sistema (calidad, análisis de
rostro/color, guardado, WebSocket) no se entera de dónde vino la foto.
"""
import logging
import threading

import cv2

from app.config import settings

logger = logging.getLogger("webcam")

# Abrir un VideoCapture tarda 1-3s en Windows -- se abre una sola vez y se
# reutiliza, en vez de abrir/cerrar en cada foto.
_captura: cv2.VideoCapture | None = None
_lock = threading.Lock()


def _abrir() -> cv2.VideoCapture | None:
    global _captura
    cap = cv2.VideoCapture(settings.webcam_indice, cv2.CAP_DSHOW)
    if not cap.isOpened():
        cap.release()
        logger.error("No se pudo abrir la webcam (indice %d)", settings.webcam_indice)
        return None
    _captura = cap
    return cap


def capturar_jpeg() -> bytes | None:
    """Un frame de la webcam, codificado a JPEG. None si la webcam no
    responde (se reintenta reabrirla en la siguiente llamada).
    Bloqueante: llamar con asyncio.to_thread."""
    global _captura
    with _lock:
        cap = _captura if _captura is not None and _captura.isOpened() else _abrir()
        if cap is None:
            return None
        ok, frame = cap.read()
        if not ok or frame is None:
            logger.warning("La webcam no devolvió un frame; se reabrirá en el próximo intento")
            cap.release()
            _captura = None
            return None
        ok, buf = cv2.imencode(".jpg", frame)
        return buf.tobytes() if ok else None
