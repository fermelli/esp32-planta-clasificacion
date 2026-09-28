"""Baja los dos modelos ONNX de rostro (OpenCV Zoo) a server/modelos/.

Se corre una vez, con internet (en casa). Después el servidor funciona sin red.
Uso, desde server/:  .venv/Scripts/python ml/descargar_modelos.py
"""
import sys
import urllib.request
from pathlib import Path

BASE = "https://github.com/opencv/opencv_zoo/raw/main/models"
MODELOS = {
    "face_detection_yunet_2023mar.onnx": f"{BASE}/face_detection_yunet/face_detection_yunet_2023mar.onnx",
    "face_recognition_sface_2021dec.onnx": f"{BASE}/face_recognition_sface/face_recognition_sface_2021dec.onnx",
}
DESTINO = Path(__file__).resolve().parent.parent / "modelos"
TAMANO_MINIMO = 100_000  # un puntero de git-lfs pesa ~130 bytes: si baja eso, algo salió mal


def main() -> int:
    DESTINO.mkdir(exist_ok=True)
    for nombre, url in MODELOS.items():
        destino = DESTINO / nombre
        if destino.exists() and destino.stat().st_size > TAMANO_MINIMO:
            print(f"ya existe: {nombre}")
            continue
        print(f"bajando {nombre} ...")
        urllib.request.urlretrieve(url, destino)
        if destino.stat().st_size <= TAMANO_MINIMO:
            destino.unlink()
            print(f"ERROR: {nombre} bajó incompleto (¿puntero de git-lfs?)", file=sys.stderr)
            return 1
        print(f"  ok ({destino.stat().st_size // 1024} KB)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
