export interface LoginResponse {
  access_token: string
  token_type: string
  nombre: string
}

export interface IntentoLogin {
  id: number
  usuario_nombre: string | null
  exito: boolean
  origen: 'keypad' | 'web' | 'keypad_rostro'
  creado_en: string
}

export interface Alerta {
  id: number
  tipo: string
  mensaje: string
  creado_en: string
}

export interface EventoCaja {
  id: number
  color: string
  conteo: number
  lote_completo: boolean
  r: number
  g: number
  b: number
  c: number
  creado_en: string
}

export interface ConteoColor {
  color: string
  conteo_actual: number
  total_historico: number
  lotes_completados: number
}

export interface CajaPorHora {
  hora: string
  color: string
  cantidad: number
}

export interface SorterEstado {
  puerta_abierta?: boolean
  cinta_estado?: 'off' | 'low' | 'full'
}

export interface CamaraVersion {
  flag: boolean
  online: boolean
  activo: boolean
}

export interface CamaraConfig {
  rostro: CamaraVersion
  color: CamaraVersion
}

export interface RostroConfig {
  login_rostro: boolean
  umbral: number
  modelos_cargados: boolean
}

export interface RostroUsuario {
  usuario_id: number
  nombre: string
  muestras: number
}

export interface VerificacionRostro {
  id: number
  usuario_nombre: string
  similitud: number | null
  exito: boolean
  imagen: string | null
  creado_en: string
}

export interface CapturaColor {
  evento_id: number
  color_sensor: string
  color_ia: string | null
  confianza: number | null
  coincide: boolean | null
  imagen: string
  creado_en: string
}

export interface MetricasColor {
  entrenado_en: string
  n_muestras: number
  n_entrenamiento: number
  n_prueba: number
  accuracy: number
  clases: string[]
  matriz_confusion: number[][]
  reporte: Record<
    string,
    { precision: number; recall: number; 'f1-score': number; support: number }
  >
  clases_omitidas: Record<string, number>
}

export interface ResumenColor {
  total: number
  con_ia: number
  coinciden: number
  acuerdo_pct: number | null
  dataset: Record<string, number>
  modelo_entrenado: boolean
  camara_color: boolean
  min_por_clase: number
  metricas: MetricasColor | null
}

export type WsMensaje =
  | ({ type: 'intento_login' } & Omit<IntentoLogin, 'id' | 'usuario_nombre' | 'creado_en'> & {
        nombre: string | null
        bloqueado: boolean
        intento: number
      })
  | ({ type: 'sorter_estado' } & SorterEstado)
  | ({ type: 'evento_caja' } & Omit<EventoCaja, 'id' | 'creado_en'>)
  | ({ type: 'alerta' } & Pick<Alerta, 'tipo' | 'mensaje'>)
  | { type: 'camara_estado'; version: 'rostro' | 'color'; online: boolean }
  | {
      type: 'clasificacion_camara'
      evento_id: number
      color_sensor: string
      color_ia: string | null
      confianza: number | null
      coincide: boolean | null
      imagen: string
    }
  | {
      type: 'verificacion_rostro'
      usuario_id: number
      nombre: string
      similitud: number | null
      exito: boolean
      umbral: number
      imagen: string
    }
