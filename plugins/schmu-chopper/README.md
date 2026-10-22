# SCHMU Chopper

Sampler **VST3 / AU** para chopear tus *Vocal Chops* (o cualquier WAV) y tocarlos en Ableton: auto-chop por transientes,
16 pads, modo Key (afinado en el teclado, tonalidad leída del nombre `SCHMU_VC1_…_<key>_<bpm>.wav`), secuenciador de 16
pasos con swing y probabilidad, pitch sin cambiar la duración, y FX (tape, crush, delay, reverb). 16 presets.
Código abierto (AGPL-3.0); licencias y procedencia en `LICENCIAS.md`.

## Instalarlo en Ableton Live

Compilalo (sección **Compilar**) y copiá el plugin generado.

### Windows (Live 10/11/12, VST3)
1. Copiá la carpeta **`SCHMU Chopper.vst3`** a `C:\Program Files\Common Files\VST3\`.
2. En Ableton: *Options → Preferences → Plug-ins* y activá **"Use VST3 System Folders"**.
3. **Rescan** (o reiniciá Live). Buscalo en *Plug-ins → SCHMU Chopper*.
4. Arrastralo a una pista **MIDI** (es un instrumento).

### macOS (Live 11/12, AU o VST3)
1. **AU:** copiá `SCHMU Chopper.component` a `~/Library/Audio/Plug-Ins/Components/`. **VST3:** copiá `SCHMU Chopper.vst3` a `~/Library/Audio/Plug-Ins/VST3/`.
2. El binario no está firmado: `xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/Components/"SCHMU Chopper.component"` (y lo mismo con el `.vst3`).
3. *Preferences → Plug-ins* → activá **Use Audio Units** y/o **Use VST3 Plug-In System Folders** → **Rescan**.

## Usarlo
1. **Cargar:** arrastrá un WAV (o la carpeta `SCHMU_VC1…/01_Syllables`) sobre la ventana, o usá *Cargar WAV…* / *Cargar carpeta…*.
   Un WAV se corta solo; una carpeta arma un pad por archivo (máx. 16).
2. **Chop** (por defecto): cada pad es un slice, desde **C1 (nota 36)** hacia arriba — igual que el kit del pack.
   Ajustá cada slice abajo a la izquierda: Gain, Pan, Pitch, ADSR, filtro, *One-shot / Gate / Loop*, Reverse, Choke.
3. **Cortes:** *Sensitivity* + *Auto-chop* vuelve a cortar; *16 parejos* reparte parejo; doble clic en la forma de onda agrega un
   corte, arrastrarlo lo mueve, Alt+clic lo borra.
4. **Key:** tocá el slice elegido en el teclado; la tecla de su nota de origen suena a la altura original. *Keep Length* mantiene
   la duración al afinar (apagado = repitch). *Transpose* mueve todo ±12.
5. **Sequence:** con el transporte de Live andando, los 16 pasos disparan slices. Clic en un paso = poner el slice elegido;
   rueda = probabilidad; clic derecho = borrar. *Swing* en 62 % da el salto de garage.
6. El chop se guarda con el proyecto de Live (si pesa < 2 MB); si es más grande se vuelve a buscar por ruta.

## Compilar
```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j
ctest --test-dir build -C Release --output-on-failure
build/schmu_render demo.wav 126     # render de demostración (sin JUCE)
```
`-DSCHMU_JUCE_DIR=/ruta/a/JUCE` usa una copia local de JUCE; `-DSCHMU_BUILD_PLUGIN=OFF` compila solo el motor y los tests
(sin red). En Linux hacen falta: `libasound2-dev libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxext-dev
libfreetype-dev libfontconfig1-dev libgl-dev` (y `xvfb` para el test del editor).

## Binarios
El workflow `.github/workflows/schmu-chopper.yml` corre los tests de DSP en Linux y comprueba que el plugin compila en Windows y
macOS. **No sube binarios**: hasta confirmar el camino de licencia, los ejecutables no se reparten desde este repo.
