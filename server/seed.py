"""Siembra los 2 usuarios del proyecto. Se puede correr varias veces: si el
usuario ya existe, solo actualiza su hash en vez de duplicarlo.

Un solo secreto por usuario (numérico): se teclea igual en el ESP32 y se
escribe igual en el formulario web — así lo pidió el docente."""

import asyncio

import asyncpg

from app.config import settings
from app.security import hash_secret


async def main() -> None:
    conn = await asyncpg.connect(settings.database_url)
    try:
        usuarios = [
            (settings.seed_usuario_1_nombre, settings.seed_usuario_1_secreto),
            (settings.seed_usuario_2_nombre, settings.seed_usuario_2_secreto),
        ]
        for nombre, secreto in usuarios:
            await conn.execute(
                """
                INSERT INTO usuarios (nombre, password_hash)
                VALUES ($1, $2)
                ON CONFLICT (nombre) DO UPDATE
                SET password_hash = EXCLUDED.password_hash
                """,
                nombre, hash_secret(secreto),
            )
            print(f"usuario '{nombre}' listo (clave={secreto})")
    finally:
        await conn.close()


if __name__ == "__main__":
    asyncio.run(main())
