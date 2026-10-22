# Licencias de SCHMU Analyzer

> Resumen informativo, **no es asesoría legal**. Verificá las condiciones vigentes en juce.com/legal.

- **Código propio** (`Source/`, `Tests/`): AGPL-3.0-or-later (ver `LICENSE` en la raíz del repo).
- **JUCE 8.0.8**: se usa bajo su opción **AGPLv3**; por eso el código fuente completo del plugin es público en este repo.
- **VST3**: el SDK de Steinberg viene con JUCE bajo licencia dual (GPLv3 / acuerdo de Steinberg). No se usa el logo "VST".
- **AU** (macOS): sin restricciones adicionales.
- El DSP (filtro K y compuertas de ITU-R BS.1770-4 / EBU R128, true peak 4x, FFT, correlación, tonalidad) es propio;
  no se copió código ni gráficos de otros analizadores. Los nombres de normas se citan solo como referencia.
- Windows sin firma muestra SmartScreen; en macOS hay que usar "Abrir igualmente" (sin notarizar).
