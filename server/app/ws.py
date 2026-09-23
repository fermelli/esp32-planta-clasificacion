import json

from fastapi import WebSocket


class ConnectionManager:
    def __init__(self) -> None:
        self._clients: set[WebSocket] = set()

    async def connect(self, ws: WebSocket) -> None:
        await ws.accept()
        self._clients.add(ws)

    def disconnect(self, ws: WebSocket) -> None:
        self._clients.discard(ws)

    async def broadcast(self, payload: dict) -> None:
        muertos = []
        data = json.dumps(payload)
        # Copia de la lista: connect()/disconnect() pueden mutar self._clients
        # mientras este bucle está en un await (send_text), y set() no tolera
        # cambiar de tamaño durante la iteración.
        for ws in list(self._clients):
            try:
                await ws.send_text(data)
            except Exception:
                muertos.append(ws)
        for ws in muertos:
            self._clients.discard(ws)


manager = ConnectionManager()
