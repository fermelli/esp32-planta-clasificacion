from fastapi import APIRouter, Depends

from app.deps import get_current_user
from app.mqtt_client import TOPIC_CMD, publish
from app.schemas import ComandoRequest

router = APIRouter(prefix="/api/comandos", tags=["comandos"], dependencies=[Depends(get_current_user)])


@router.post("")
async def enviar_comando(body: ComandoRequest) -> dict:
    """Publica en planta/cmd. El gateway (ESP32 #1) lo traduce a un CommandMsg
    por ESP-NOW hacia el sorter (ESP32 #2). cmd='puerta', arg=1 abre / 0 cierra."""
    publish(TOPIC_CMD, {"cmd": body.cmd, "arg": body.arg})
    return {"ok": True}
