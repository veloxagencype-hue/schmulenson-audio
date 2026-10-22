# SCHMU Analyzer

Analizador de master gratis (VST3 / AU) de Schmulenson: espectro con pendiente ajustable, loudness **LUFS** (momentáneo,
corto plazo e integrado, EBU R128 / BS.1770-4), **true peak** 4x, correlación estéreo y tonalidad estimada. Incluye un modo
**VIDEO** pensado para grabar la pantalla. Cuatro esquemas de color: Ácido, Ámbar, Hielo y Mono.

## Compilar
```
cmake -S plugins/schmu-analyzer -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
```
Solo los tests de DSP (sin JUCE, cualquier sistema): `-DSCHMU_BUILD_PLUGIN=OFF`, luego `./build/schmu_dsp_tests`.
Requiere CMake 3.22+ y un compilador C++17. JUCE 8.0.8 se descarga solo (o `-DSCHMU_JUCE_DIR=<carpeta>`).

## Uso en Ableton Live
Copiá `SCHMU Analyzer.vst3` a `C:\Program Files\Common Files\VST3\` (Windows) o `/Library/Audio/Plug-Ins/VST3/` (macOS),
hacé *Rescan* en Preferencias → Plug-ins y arrastralo al **canal Master**, al final de la cadena.

## Licencia
AGPL-3.0-or-later. Ver `LICENCIAS.md`.
