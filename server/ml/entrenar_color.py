"""Entrena el clasificador de color con las fotos guardadas y muestra las métricas.

Es lo mismo que hace el botón "Entrenar" del dashboard (/camara-color), pero
desde la terminal — sirve para mostrar el entrenamiento en la demo o para
iterar sin abrir el navegador.
Uso, desde server/:  .venv/Scripts/python ml/entrenar_color.py
"""
import asyncio
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import asyncpg  # noqa: E402

from app import color_ia  # noqa: E402
from app.config import settings  # noqa: E402


async def leer_muestras() -> list[tuple[str, str]]:
    conexion = await asyncpg.connect(settings.database_url)
    try:
        filas = await conexion.fetch(
            """
            SELECT c.imagen, COALESCE(e.etiqueta_real, e.color) AS etiqueta
            FROM capturas_camara c JOIN eventos_caja e ON e.id = c.evento_id
            """
        )
    finally:
        await conexion.close()
    return [(f["imagen"], f["etiqueta"]) for f in filas]


def main() -> int:
    muestras = asyncio.run(leer_muestras())
    print(f"{len(muestras)} fotos en la base")
    try:
        m = color_ia.entrenar(muestras)
    except ValueError as e:
        print(f"No se puede entrenar: {e}", file=sys.stderr)
        return 1

    print(f"\nEntrenadas {m['n_entrenamiento']} / probadas {m['n_prueba']}  ->  accuracy {m['accuracy']:.1%}")
    if m["clases_omitidas"]:
        print(f"Colores omitidos por tener pocas fotos: {m['clases_omitidas']}")
    print("\nMatriz de confusión (filas = real, columnas = predicho):")
    print("           " + " ".join(f"{c:>9}" for c in m["clases"]))
    for clase, fila in zip(m["clases"], m["matriz_confusion"]):
        print(f"{clase:>10} " + " ".join(f"{n:>9}" for n in fila))
    print("\nPor color:")
    for clase in m["clases"]:
        r = m["reporte"][clase]
        print(f"  {clase:>10}  precisión {r['precision']:.2f}  recall {r['recall']:.2f}  f1 {r['f1-score']:.2f}  ({int(r['support'])} de prueba)")
    print("\nModelo guardado en server/modelos/color.joblib (el servidor lo recarga solo al reiniciar).")
    return 0


if __name__ == "__main__":
    sys.exit(main())
