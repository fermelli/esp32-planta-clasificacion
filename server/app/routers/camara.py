from fastapi import APIRouter, Depends

from app import mqtt_client
from app.deps import get_current_user
from app.schemas import CamaraConfigOut, CamaraVersionOut

router = APIRouter(prefix="/api/camara", tags=["camara"], dependencies=[Depends(get_current_user)])


def _version(nombre: str) -> CamaraVersionOut:
    flag = mqtt_client.flag_activo(nombre)
    online = mqtt_client.camaras_online[nombre]
    return CamaraVersionOut(flag=flag, online=online, activo=flag and online)


@router.get("/config", response_model=CamaraConfigOut)
async def configuracion() -> CamaraConfigOut:
    """Qué versiones de la cámara están activadas y cuáles tienen una placa
    conectada. El dashboard lo usa para mostrar solo lo que corresponde."""
    return CamaraConfigOut(rostro=_version("rostro"), color=_version("color"))
