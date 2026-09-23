"""Siembra los 2 usuarios del proyecto. Se puede correr varias veces: si el
usuario ya existe, solo actualiza sus hashes en vez de duplicarlo."""

import asyncio

import asyncpg

from app.config import settings
from app.security import hash_secret


async def main() -> None:
    conn = await asyncpg.connect(settings.database_url)
    try:
        usuarios = [
            (settings.seed_usuario_1_nombre, settings.seed_usuario_1_pin, settings.seed_usuario_1_password),
            (settings.seed_usuario_2_nombre, settings.seed_usuario_2_pin, settings.seed_usuario_2_password),
        ]
        for nombre, pin, password in usuarios:
            await conn.execute(
                """
                INSERT INTO usuarios (nombre, pin_hash, password_hash)
                VALUES ($1, $2, $3)
                ON CONFLICT (nombre) DO UPDATE
                SET pin_hash = EXCLUDED.pin_hash, password_hash = EXCLUDED.password_hash
                """,
                nombre, hash_secret(pin), hash_secret(password),
            )
            print(f"usuario '{nombre}' listo (PIN={pin})")
    finally:
        await conn.close()


if __name__ == "__main__":
    asyncio.run(main())
