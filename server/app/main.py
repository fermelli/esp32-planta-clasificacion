import asyncio
from contextlib import asynccontextmanager

from fastapi import FastAPI, WebSocket, WebSocketDisconnect
from fastapi.middleware.cors import CORSMiddleware
from fastapi.staticfiles import StaticFiles

from app import camara, color_ia, db, mqtt_client, rostro
from app.config import settings
from app.routers import auth, comandos, intentos, produccion, usuarios
from app.routers import camara as camara_router
from app.routers import color as color_router
from app.routers import rostro as rostro_router
from app.ws import manager


@asynccontextmanager
async def lifespan(app: FastAPI):
    cargados = await asyncio.to_thread(rostro.cargar)
    if settings.login_rostro and not cargados:
        raise RuntimeError(
            "LOGIN_ROSTRO=true pero faltan los modelos en server/modelos/: "
            "correr python ml/descargar_modelos.py"
        )
    await asyncio.to_thread(color_ia.cargar)
    await db.connect()
    mqtt_client.start(asyncio.get_event_loop())
    yield
    mqtt_client.stop()
    await db.disconnect()


app = FastAPI(title="Planta de clasificación", lifespan=lifespan)

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_methods=["*"],
    allow_headers=["*"],
)

app.include_router(auth.router)
app.include_router(usuarios.router)
app.include_router(intentos.router)
app.include_router(comandos.router)
app.include_router(produccion.router)
app.include_router(camara_router.router)
app.include_router(camara_router.router_placa)
app.include_router(rostro_router.router_camara)
app.include_router(rostro_router.router)
app.include_router(color_router.router_camara)
app.include_router(color_router.router)
app.mount("/capturas", StaticFiles(directory=camara.capturas_dir()), name="capturas")


@app.websocket("/ws")
async def websocket_endpoint(ws: WebSocket) -> None:
    await manager.connect(ws)
    try:
        while True:
            await ws.receive_text()  # el dashboard no manda nada, solo escucha
    except WebSocketDisconnect:
        manager.disconnect(ws)


@app.get("/api/salud")
async def salud() -> dict:
    return {"ok": True}
