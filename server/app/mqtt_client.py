import asyncio
import json
import logging

import bcrypt
import paho.mqtt.client as mqtt

from app.config import settings
from app.db import pool
from app.ws import manager

logger = logging.getLogger("mqtt")

TOPIC_LOGIN_INTENTO = "planta/login/intento"
TOPIC_LOGIN_RESULTADO = "planta/login/resultado"
TOPIC_SORTER_ESTADO = "planta/sorter/estado"
TOPIC_SORTER_EVENTO = "planta/sorter/evento"
TOPIC_SORTER_ALERTA = "planta/sorter/alerta"
TOPIC_CMD = "planta/cmd"

_client: mqtt.Client | None = None
_loop: asyncio.AbstractEventLoop | None = None


async def _manejar_login_intento(payload: dict) -> None:
    """El ESP32 #1 manda {"pin": "1234", "intento": 1|2}. Valida contra la BBDD
    (mismo password_hash que el login web — un solo secreto por usuario),
    registra el intento y responde por MQTT con el resultado para el LCD."""
    pin = str(payload.get("pin", ""))
    numero_intento = int(payload.get("intento", 1))

    usuario_id = None
    nombre = None
    exito = False

    filas = await pool().fetch("SELECT id, nombre, password_hash FROM usuarios")
    for f in filas:
        # bcrypt es CPU-bound y tarda ~100-300ms: en un hilo aparte para no
        # congelar el event loop (MQTT y WebSocket) mientras se valida el PIN.
        coincide = await asyncio.to_thread(bcrypt.checkpw, pin.encode(), f["password_hash"].encode())
        if coincide:
            usuario_id, nombre, exito = f["id"], f["nombre"], True
            break

    bloqueado = (not exito) and numero_intento >= 2

    await pool().execute(
        "INSERT INTO intentos_login (usuario_id, exito, origen) VALUES ($1, $2, 'keypad')",
        usuario_id, exito,
    )

    if bloqueado:
        await pool().execute(
            "INSERT INTO alertas (tipo, mensaje) VALUES ('login_bloqueado', $1)",
            f"Login bloqueado tras {numero_intento} intentos fallidos",
        )

    resultado = {
        "exito": exito,
        "nombre": nombre,
        "bloqueado": bloqueado,
        "intento": numero_intento,
    }
    publish(TOPIC_LOGIN_RESULTADO, resultado)
    await manager.broadcast({"type": "intento_login", **resultado})


async def _manejar_sorter_estado(payload: dict) -> None:
    """Estado instantáneo (puerta, cinta) del ESP32 #2. No se persiste, solo se
    reenvía al dashboard por WebSocket — vive en el hardware, no en la BBDD."""
    await manager.broadcast({"type": "sorter_estado", **payload})


async def _manejar_sorter_evento(payload: dict) -> None:
    """Cada caja que pasa por el sensor. 'conteo' y 'lote_completo' ya vienen
    calculados por el ESP32 (es quien enciende los LEDs binarios); el
    servidor solo los guarda junto con la lectura cruda del sensor."""
    color = str(payload.get("color", "desconocido"))
    conteo = int(payload.get("conteo", 0))
    lote_completo = bool(payload.get("lote_completo", False))
    r, g, b, c = (int(payload.get(k, 0)) for k in ("r", "g", "b", "c"))

    await pool().execute(
        """
        INSERT INTO eventos_caja (color, conteo, lote_completo, r, g, b, c)
        VALUES ($1, $2, $3, $4, $5, $6, $7)
        """,
        color, conteo, lote_completo, r, g, b, c,
    )

    evento = {
        "color": color, "conteo": conteo, "lote_completo": lote_completo,
        "r": r, "g": g, "b": b, "c": c,
    }
    await manager.broadcast({"type": "evento_caja", **evento})

    if lote_completo:
        mensaje = f"Lote de 5 cajas {color} completado"
        await pool().execute(
            "INSERT INTO alertas (tipo, mensaje) VALUES ('lote_completo', $1)", mensaje,
        )
        await manager.broadcast({"type": "alerta", "tipo": "lote_completo", "mensaje": mensaje})


async def _manejar_sorter_alerta(payload: dict) -> None:
    """Alertas explícitas que manda el sorter (fuera de lote_completo, que se
    deriva del evento). Por ahora no hay ninguna en el Bloque 2, pero el
    topic ya queda listo para futuras alertas de hardware (atasco, etc.)."""
    tipo = str(payload.get("tipo", "sorter"))
    mensaje = str(payload.get("mensaje", ""))
    await pool().execute("INSERT INTO alertas (tipo, mensaje) VALUES ($1, $2)", tipo, mensaje)
    await manager.broadcast({"type": "alerta", "tipo": tipo, "mensaje": mensaje})


_HANDLERS = {
    TOPIC_LOGIN_INTENTO: _manejar_login_intento,
    TOPIC_SORTER_ESTADO: _manejar_sorter_estado,
    TOPIC_SORTER_EVENTO: _manejar_sorter_evento,
    TOPIC_SORTER_ALERTA: _manejar_sorter_alerta,
}


def _on_connect(client: mqtt.Client, userdata, flags, reason_code, properties=None) -> None:
    logger.info("Conectado al broker MQTT (%s)", reason_code)
    for topic in _HANDLERS:
        client.subscribe(topic)


def _on_message(client: mqtt.Client, userdata, msg: mqtt.MQTTMessage) -> None:
    handler = _HANDLERS.get(msg.topic)
    if handler is None or _loop is None:
        return
    try:
        payload = json.loads(msg.payload.decode())
    except json.JSONDecodeError:
        logger.warning("Mensaje no-JSON en %s: %r", msg.topic, msg.payload)
        return
    asyncio.run_coroutine_threadsafe(handler(payload), _loop)


def publish(topic: str, payload: dict) -> None:
    if _client is None:
        return
    _client.publish(topic, json.dumps(payload))


def start(loop: asyncio.AbstractEventLoop) -> None:
    global _client, _loop
    _loop = loop
    _client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
    _client.on_connect = _on_connect
    _client.on_message = _on_message
    _client.connect(settings.mqtt_host, settings.mqtt_port)
    _client.loop_start()


def stop() -> None:
    if _client is not None:
        _client.loop_stop()
        _client.disconnect()
