import { defineConfigWithVueTs, vueTsConfigs } from '@vue/eslint-config-typescript'
import pluginVue from 'eslint-plugin-vue'
import skipFormatting from '@vue/eslint-config-prettier/skip-formatting'

export default defineConfigWithVueTs(
  {
    name: 'app/files-to-lint',
    files: ['**/*.{ts,mts,tsx,vue}'],
  },
  {
    name: 'app/files-to-ignore',
    ignores: ['**/dist/**', '**/dist-ssr/**', '**/coverage/**', '**/.vitepress/**'],
  },
  pluginVue.configs['flat/essential'],
  vueTsConfigs.recommended,
  {
    // Primitivos de UI estilo shadcn-vue: nombre de una sola palabra por
    // convención (Button, Card, Input...) — nunca se usan como elemento
    // nativo suelto, siempre se importan con nombre desde '@/components/ui'.
    // Va después de los presets: en flat config, el último gana.
    name: 'app/ui-primitives',
    files: ['src/components/ui/**/*.vue'],
    rules: { 'vue/multi-word-component-names': 'off' },
  },
  skipFormatting,
)
