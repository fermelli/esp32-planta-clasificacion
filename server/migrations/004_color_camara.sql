-- Versión B de la cámara: foto de cada caja, clasificada por un modelo scikit-learn
-- en paralelo al sensor TCS3472. docker-compose.yml la monta en
-- docker-entrypoint-initdb.d; si el volumen pgdata ya existía, aplicar a mano:
-- docker compose exec -T postgres psql -U planta -d planta -f /docker-entrypoint-initdb.d/04-color-camara.sql

-- UNA FOTO POR CAJA, enlazada al evento que disparó el sensor. La etiqueta con
-- la que se entrena el modelo sale de eventos_caja (COALESCE(etiqueta_real, color)):
-- el sensor etiqueta solo el dataset, y etiqueta_real permite corregirlo a mano.
-- color_ia es NULL mientras todavía no hay un modelo entrenado (modo "juntar dataset").
CREATE TABLE capturas_camara (
  id             SERIAL PRIMARY KEY,
  evento_id      INTEGER NOT NULL UNIQUE REFERENCES eventos_caja(id) ON DELETE CASCADE,
  imagen         TEXT NOT NULL,
  color_ia       TEXT,
  confianza      REAL,
  probabilidades JSONB,
  creado_en      TIMESTAMPTZ NOT NULL DEFAULT now()
);

CREATE INDEX idx_capturas_camara_creado_en ON capturas_camara (creado_en DESC);
