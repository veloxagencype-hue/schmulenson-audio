# Licencias de SCHMU Chopper

> Resumen informativo, **no es asesoría legal**. Verificá las condiciones vigentes en juce.com/legal.

- **Código propio** (`Source/`, `Tests/`, `Tools/`): AGPL-3.0-or-later (ver `LICENSE` en la raíz del repo).
- **JUCE 8.0.8**: se usa bajo su opción **AGPLv3**; por eso el código fuente completo del plugin es público en este repo.
  Se baja con CMake (`FetchContent`), no se commitea.
- **VST3**: el SDK de Steinberg viene con JUCE bajo licencia dual (GPLv3 / acuerdo de Steinberg). No se usa el logo "VST".
- **AU** (macOS): sin restricciones adicionales.
- El DSP (auto-chop por transientes, detección de tono, voces, pitch sin cambiar la duración, secuenciador, bitcrusher, reverb)
  es propio y usa técnicas públicas y estándar (detección de onsets por energía, autocorrelación, síntesis granular con ventana
  Hann, interpolación cúbica, filtro de variable de estado TPT, reverb de combs + allpass). No se copió código ni gráficos
  de otros samplers. No se usa Rubber Band ni SoundTouch.
- **Sin audio de terceros**: el repo no tiene samples; los tests generan audio sintético en memoria. El plugin carga archivos del
  usuario y el audio no viaja dentro del plugin.
- Windows sin firma muestra SmartScreen; en macOS hay que usar "Abrir igualmente" (sin notarizar).
