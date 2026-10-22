# SCHMU 90s

Synth **VST3 / AU** de house clásico, garage y 2-step, con síntesis propia (sin samples de ROMs ni librerías
comerciales). Hecho con JUCE + un motor DSP propio sin dependencias (`Source/dsp/`).

**Motores:** Organ (aditivo + click + percusión) · Piano (FM estilo piano house clásico, brillante, con chorus) · Stab (saw/pulse con
filtro resonante y chord memory m7/9/11) · Bass 2-step (seno con pitch drop, reese, glide) · Drums (909/808 modelados:
kick, snare, clap, rim, hats con choke, toms) · Chops (slot para tus Vocal Chops, modo teclas o slices).
**FX:** tape, chorus, delay sincronizado ping-pong, reverb, swing global. **45 presets** de fábrica.

## Instalarlo en Ableton Live

Bajá el paquete de la corrida de Actions (`schmu90s-windows` o `schmu90s-macos`; ver "Binarios" abajo) y descomprimilo.

### Windows (Live 10/11/12, VST3)
1. Copiá la carpeta **`SCHMU 90s.vst3`** a `C:\Program Files\Common Files\VST3\`.
2. En Ableton: *Options → Preferences → Plug-ins* y activá **"Use VST3 System Folders"** (si no está activo).
3. Tocá **Rescan** (o reiniciá Live). Buscalo en el navegador: *Plug-ins → SCHMU 90s*.
4. Arrastralo a una pista **MIDI** (es un instrumento). Elegí un preset arriba a la derecha y tocá.

### macOS (Live 11/12, AU o VST3)
1. **AU:** copiá `SCHMU 90s.component` a `~/Library/Audio/Plug-Ins/Components/`. **VST3:** copiá `SCHMU 90s.vst3` a
   `~/Library/Audio/Plug-Ins/VST3/`.
2. El binario no está firmado/notarizado: abrí Terminal y ejecutá
   `xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/Components/"SCHMU 90s.component"` (y lo mismo con el `.vst3`).
3. En Ableton: *Preferences → Plug-ins* → activá **"Use Audio Units"** y/o **"Use VST3 Plug-In System Folders"** → **Rescan**.
4. Arrastralo a una pista MIDI.

### Usarlo
- **Swing** (barra de FX): retrasa las notas en contratiempo de 16avos mientras corre el transporte de Live (50 % recto,
  ~62 % garage, 66 % tresillo). Se aplica a lo que grabes o toques con el transporte andando.
- **Drums**: mapa GM → C1 bombo, C#1 rim, D1 snare, D#1 clap, F#1/G#1 hat cerrado, A#1 hat abierto, toms en F1/G1/A1.
- **Chops**: en la pestaña CHOPS, *Cargar chop…* y elegí un WAV/AIFF/FLAC/OGG (por ejemplo tus *Vocal Chops Vol.1*).
  *Keys*: C3 = velocidad original y el resto transpone. *Slices*: 4/8/16 cortes parejos desde C1 (36) hacia arriba.
  El archivo se recuerda con el proyecto de Live (por ruta; si lo movés, vuelve al chop de fábrica).
- **Chord memory** (Organ/Piano/Stab): una tecla = acorde (m7, maj7, dom7, m9, 9, maj9, m11, sus4, m9 open, power).
- **Bass** es monofónico: legato + *Glide* desliza entre notas. Grabá el bajo una octava abajo (C1–C2).

## Compilar

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release          # baja JUCE 8.0.8 (o: -DSCHMU_JUCE_DIR=/ruta/JUCE)
cmake --build build --config Release -j
ctest --test-dir build -C Release --output-on-failure
```
- Solo motor + tests + render, sin JUCE ni red: `-DSCHMU_BUILD_PLUGIN=OFF`.
- Linux necesita: `libasound2-dev libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxext-dev libfreetype-dev libfontconfig1-dev libgl-dev` (+ `xvfb` para el test del editor).
- Demo offline de 8 compases hecho solo con el synth: `build/schmu_render demo.wav 132`.
- Binarios VST3/AU: **GitHub Actions → workflow `vst-90s` → Artifacts** (`schmu90s-windows`, `schmu90s-macos`).

## Estructura
```
Source/dsp/      motor sin JUCE: Common, Params (tabla única), Engine (voces), Drums, Sampler, Effects, Presets
Source/Plugin*   wrapper JUCE: procesador (parámetros, MIDI+swing, programas, estado) y editor
Tests/           dsp_tests.cpp (≈1,1 millones de chequeos de DSP) y plugin_smoke.cpp (plugin real + editor a PNG)
Tools/           render_demo.cpp (tema de garage offline)
SPEC.md PLAN.md ENTREGA.md REELS.md LICENCIAS.md
```
