-- Bloque 2 (Producción y conteo). NO aplicar todavía: se corre a mano
-- (docker compose exec -T postgres psql -U planta -d planta -f ...) recién
-- cuando empecemos ese bloque.

-- CADA CAJA QUE PASA POR EL SENSOR. Es la tabla central: inventario y dataset a la vez.
-- 'conteo' es el valor del contador del ESP32 en ese momento, o sea lo que muestran los LEDs.
-- 'lote_completo' marca la caja que cerró las 5 y disparó el reset de la pizarra.
-- r,g,b,c es la lectura cruda del TCS3472: sin esos números no se puede entrenar nada en el bloque 3.
CREATE TABLE eventos_caja (
  id            SERIAL PRIMARY KEY,
  color         TEXT NOT NULL,          -- 'rojo' | 'verde' | 'azul' | 'desconocido'
  conteo        SMALLINT NOT NULL,      -- 0-5, tal como lo mandó el ESP32
  lote_completo BOOLEAN NOT NULL DEFAULT false,
  r             INTEGER NOT NULL,
  g             INTEGER NOT NULL,
  b             INTEGER NOT NULL,
  c             INTEGER NOT NULL,       -- clear (intensidad total)
  color_ml      TEXT,                   -- bloque 3: lo que dijo el modelo
  etiqueta_real TEXT,                   -- bloque 3: corregido a mano desde el dashboard
  creado_en     TIMESTAMPTZ NOT NULL DEFAULT now()
);

CREATE INDEX idx_eventos_caja_creado_en ON eventos_caja (creado_en DESC);
CREATE INDEX idx_eventos_caja_color ON eventos_caja (color, creado_en DESC);
