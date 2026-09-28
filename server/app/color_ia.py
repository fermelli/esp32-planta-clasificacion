import json
import logging
from datetime import datetime, timezone

import cv2
import joblib
import numpy as np
from sklearn.metrics import accuracy_score, classification_report, confusion_matrix
from sklearn.model_selection import train_test_split
from sklearn.svm import SVC

from app.camara import capturas_dir, modelos_dir
from app.config import settings

logger = logging.getLogger("color_ia")

MIN_POR_CLASE = 10  # con menos, ni el split 80/20 ni la calibración de probabilidades son confiables

_modelo: dict | None = None


def _archivo_modelo():
    return modelos_dir() / "color.joblib"


def _archivo_metricas():
    return modelos_dir() / "color_metricas.json"


def _centro(jpeg: bytes) -> np.ndarray | None:
    """El recorte central de la foto (COLOR_RECORTE), reducido a 64x64 BGR."""
    imagen = cv2.imdecode(np.frombuffer(jpeg, np.uint8), cv2.IMREAD_COLOR)
    if imagen is None:
        return None
    alto, ancho = imagen.shape[:2]
    mx = int(ancho * (1 - settings.color_recorte) / 2)
    my = int(alto * (1 - settings.color_recorte) / 2)
    return cv2.resize(imagen[my : alto - my, mx : ancho - mx], (64, 64), interpolation=cv2.INTER_AREA)


def rgb_medio(jpeg: bytes) -> list[int] | None:
    """Color RGB medio de la zona central: lo que 've' el clasificador."""
    imagen = _centro(jpeg)
    if imagen is None:
        return None
    return [int(v) for v in imagen[:, :, ::-1].reshape(-1, 3).mean(axis=0)]


def caracteristicas(jpeg: bytes) -> np.ndarray | None:
    """Vector de 43 números de una foto, todos entre -1 y 1 (por eso el modelo no
    usa StandardScaler: sobre histogramas con bins casi vacíos amplifica el ruido
    y con pocas fotos empeoraba mucho el resultado):
      - histograma HSV: 16 + 8 + 8 bins, cada uno normalizado a suma 1
      - matiz medio de los píxeles saturados (como coseno y seno, porque el rojo
        está en los dos extremos de la escala), y qué fracción están saturados
      - saturación y brillo medios, y color RGB medio y mediano
    Solo mira el centro de la imagen (COLOR_RECORTE): ahí queda la caja, y el
    fondo de la cinta pesa menos."""
    imagen = _centro(jpeg)
    if imagen is None:
        return None

    hsv = cv2.cvtColor(imagen, cv2.COLOR_BGR2HSV)
    partes = []
    for canal, bins, maximo in ((0, 16, 180), (1, 8, 256), (2, 8, 256)):
        hist = cv2.calcHist([hsv], [canal], None, [bins], [0, maximo]).flatten()
        partes.append(hist / max(hist.sum(), 1))

    pixeles = hsv.reshape(-1, 3).astype(np.float64)
    saturados = pixeles[:, 1] > 60
    angulos = pixeles[saturados, 0] * 2 * np.pi / 180  # OpenCV guarda el matiz en 0-180
    matiz = [np.cos(angulos).mean(), np.sin(angulos).mean()] if saturados.any() else [0.0, 0.0]
    rgb = imagen[:, :, ::-1].reshape(-1, 3) / 255.0
    compactas = [*matiz, saturados.mean(), pixeles[:, 1].mean() / 255, pixeles[:, 2].mean() / 255,
                 *rgb.mean(axis=0), *np.median(rgb, axis=0)]
    return np.concatenate([*partes, compactas]).astype(np.float32)


def cargar() -> bool:
    """Carga modelos/color.joblib si existe. Se llama al arrancar y tras cada entrenamiento."""
    global _modelo
    if not _archivo_modelo().exists():
        _modelo = None
        return False
    _modelo = joblib.load(_archivo_modelo())
    logger.info("Modelo de color cargado (%d muestras)", _modelo["n_muestras"])
    return True


def hay_modelo() -> bool:
    return _modelo is not None


def metricas() -> dict | None:
    if not _archivo_metricas().exists():
        return None
    return json.loads(_archivo_metricas().read_text(encoding="utf-8"))


def predecir(jpeg: bytes) -> tuple[str, float, dict[str, float]] | None:
    """(color, confianza, probabilidades por clase), o None si no hay modelo o
    la foto no se puede leer. Bloqueante: llamar con to_thread."""
    if _modelo is None:
        return None
    vector = caracteristicas(jpeg)
    if vector is None:
        return None
    modelo = _modelo["modelo"]
    probas = modelo.predict_proba([vector])[0]
    por_clase = {str(c): round(float(p), 4) for c, p in zip(modelo.classes_, probas)}
    color = max(por_clase, key=por_clase.get)
    return color, por_clase[color], por_clase


def entrenar(muestras: list[tuple[str, str]]) -> dict:
    """Entrena con [(ruta_imagen_relativa, etiqueta)], guarda el modelo y las
    métricas, y lo deja cargado. Bloqueante: llamar con to_thread. Lanza
    ValueError con un mensaje legible si el dataset no alcanza.

    Las métricas salen de un split 80/20 estratificado; el modelo que se guarda
    se reentrena después con todas las muestras."""
    X, y = [], []
    for ruta, etiqueta in muestras:
        archivo = capturas_dir() / ruta
        if not archivo.exists():
            continue
        vector = caracteristicas(archivo.read_bytes())
        if vector is not None:
            X.append(vector)
            y.append(etiqueta)

    cuentas = {c: y.count(c) for c in sorted(set(y))}
    omitidas = {c: n for c, n in cuentas.items() if n < MIN_POR_CLASE}
    validas = [c for c in cuentas if c not in omitidas]
    if len(validas) < 2:
        raise ValueError(
            f"Hacen falta al menos 2 colores con {MIN_POR_CLASE} fotos cada uno. "
            f"Fotos por color hasta ahora: {cuentas or 'ninguna'}"
        )
    filtro = [i for i, etiqueta in enumerate(y) if etiqueta in validas]
    X = np.array([X[i] for i in filtro])
    y = np.array([y[i] for i in filtro])

    def nuevo_modelo():
        return SVC(kernel="rbf", C=10, gamma="scale", probability=True, class_weight="balanced", random_state=42)

    X_ent, X_test, y_ent, y_test = train_test_split(X, y, test_size=0.2, stratify=y, random_state=42)
    evaluado = nuevo_modelo().fit(X_ent, y_ent)
    y_pred = evaluado.predict(X_test)
    clases = [str(c) for c in evaluado.classes_]

    resultado = {
        "entrenado_en": datetime.now(timezone.utc).isoformat(),
        "n_muestras": int(len(y)),
        "n_entrenamiento": int(len(y_ent)),
        "n_prueba": int(len(y_test)),
        "accuracy": round(float(accuracy_score(y_test, y_pred)), 4),
        "clases": clases,
        "matriz_confusion": confusion_matrix(y_test, y_pred, labels=clases).tolist(),
        "reporte": classification_report(y_test, y_pred, labels=clases, output_dict=True, zero_division=0),
        "clases_omitidas": omitidas,
    }
    resultado["reporte"] = {c: resultado["reporte"][c] for c in clases}

    final = nuevo_modelo().fit(X, y)
    joblib.dump({"modelo": final, "n_muestras": int(len(y))}, _archivo_modelo())
    _archivo_metricas().write_text(json.dumps(resultado, ensure_ascii=False, indent=2), encoding="utf-8")
    cargar()
    return resultado
