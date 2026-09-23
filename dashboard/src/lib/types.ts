export interface LoginResponse {
  access_token: string
  token_type: string
  nombre: string
}

export interface IntentoLogin {
  id: number
  usuario_nombre: string | null
  exito: boolean
  origen: 'keypad' | 'web'
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

export type WsMensaje =
  | ({ type: 'intento_login' } & Omit<IntentoLogin, 'id' | 'usuario_nombre' | 'creado_en'> & {
        nombre: string | null
        bloqueado: boolean
        intento: number
      })
  | ({ type: 'sorter_estado' } & SorterEstado)
  | ({ type: 'evento_caja' } & Omit<EventoCaja, 'id' | 'creado_en'>)
  | ({ type: 'alerta' } & Pick<Alerta, 'tipo' | 'mensaje'>)
