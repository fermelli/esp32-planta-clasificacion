-- Esquema del Bloque 1 (Acceso y puerta). Las tablas de los siguientes bloques
-- viven en server/migrations/ y se aplican cuando toca ese bloque, no antes.

-- QUIÉN PUEDE ENTRAR. Solo 2 filas (los 2 usuarios de la pizarra).
-- Un solo secreto por usuario (pedido del docente: misma contraseña para
-- teclado y web) — numérico, porque el teclado 4x4 solo escribe dígitos.
-- Se teclea igual en el ESP32 y se escribe igual en el formulario web.
CREATE TABLE usuarios (
  id            SERIAL PRIMARY KEY,
  nombre        TEXT NOT NULL UNIQUE,
  password_hash TEXT NOT NULL,
  creado_en     TIMESTAMPTZ NOT NULL DEFAULT now()
);

-- CADA INTENTO DE LOGIN, exitoso o fallido.
-- usuario_id es NULL cuando el PIN tecleado no corresponde a nadie.
-- origen distingue si entró por el teclado del ESP32 o por la web.
CREATE TABLE intentos_login (
  id         SERIAL PRIMARY KEY,
  usuario_id INTEGER REFERENCES usuarios(id),
  exito      BOOLEAN NOT NULL,
  origen     TEXT NOT NULL,            -- 'keypad' | 'web'
  creado_en  TIMESTAMPTZ NOT NULL DEFAULT now()
);

CREATE INDEX idx_intentos_login_creado_en ON intentos_login (creado_en DESC);

-- ALERTAS mostradas en LCD y dashboard. En el Bloque 1 solo se usa 'login_bloqueado';
-- 'lote_completo' lo empieza a producir el Bloque 2, pero la tabla ya existe desde ahora.
CREATE TABLE alertas (
  id        SERIAL PRIMARY KEY,
  tipo      TEXT NOT NULL,             -- 'login_bloqueado' | 'lote_completo'
  mensaje   TEXT NOT NULL,
  creado_en TIMESTAMPTZ NOT NULL DEFAULT now()
);

CREATE INDEX idx_alertas_creado_en ON alertas (creado_en DESC);
