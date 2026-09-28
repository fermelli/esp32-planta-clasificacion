-- Versión A de la cámara: login por PIN + rostro. docker-compose.yml la monta
-- en docker-entrypoint-initdb.d; si el volumen pgdata ya existía, aplicar a mano:
-- docker compose exec -T postgres psql -U planta -d planta -f /docker-entrypoint-initdb.d/03-rostro.sql

-- MUESTRAS DE ROSTRO ENROLADAS. Se guarda el embedding de SFace (128 floats),
-- no la foto como dato de comparación: la foto es solo evidencia para el dashboard.
CREATE TABLE rostros (
  id         SERIAL PRIMARY KEY,
  usuario_id INTEGER NOT NULL REFERENCES usuarios(id) ON DELETE CASCADE,
  embedding  REAL[] NOT NULL,
  imagen     TEXT,
  creado_en  TIMESTAMPTZ NOT NULL DEFAULT now()
);

CREATE INDEX idx_rostros_usuario ON rostros (usuario_id);

-- CADA FOTO CON CARA QUE LLEGÓ A VERIFICARSE durante un login (éxito o no).
-- similitud es el coseno máximo contra las muestras del usuario (NULL si no
-- tenía ninguna enrolada).
CREATE TABLE verificaciones_rostro (
  id         SERIAL PRIMARY KEY,
  usuario_id INTEGER NOT NULL REFERENCES usuarios(id) ON DELETE CASCADE,
  similitud  REAL,
  exito      BOOLEAN NOT NULL,
  imagen     TEXT,
  creado_en  TIMESTAMPTZ NOT NULL DEFAULT now()
);

CREATE INDEX idx_verificaciones_rostro_creado_en ON verificaciones_rostro (creado_en DESC);
