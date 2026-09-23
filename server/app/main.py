import asyncio
from contextlib import asynccontextmanager

from fastapi import FastAPI, WebSocket, WebSocketDisconnect
from fastapi.middleware.cors import CORSMiddleware

from app import db, mqtt_client
from app.routers import auth, comandos, intentos
from app.ws import manager


@asynccontextmanager
async def lifespan(app: FastAPI):
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
app.include_router(intentos.router)
app.include_router(comandos.router)


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
