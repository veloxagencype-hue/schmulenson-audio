# SCHMU 90s · licencias y procedencia

## El sonido es propio
Todo el audio se genera por síntesis en código propio (`Source/dsp/`). **No** hay samples de ROMs de Korg/Roland ni de
librerías comerciales, ni código copiado de Surge, Vital, Dexed o Helm (son GPL). Los nombres "909" y "808" en los
presets/documentos se usan solo como **referencia estilística** ("estilo"), no como emulación oficial ni marca del producto.
Los algoritmos son técnicas públicas y estándar: aditivo, FM de 2 operadores, PolyBLEP, filtro de variable de estado TPT
(Zavalishin, *The Art of VA Filter Design*), chorus con línea de retardo modulada, delay ping-pong, reverb de combs +
allpass (esquema de Schroeder/Freeverb, reimplementado), modelos de batería con osciladores y ruido (las 6
frecuencias de los hats son valores de circuito del 808 ampliamente documentados: son datos, no código ni samples). Surge/Vital/Dexed/Helm **no se
usaron** ni como código ni como base: solo como referencia general de diseño de sintes.

## Dependencias
| Qué | Licencia | Cómo se usa |
|---|---|---|
| **JUCE 8.0.8** (https://github.com/juce-framework/JUCE, tag `8.0.8`) | Doble licencia según su `LICENSE.md`: **AGPLv3** o la **JUCE 8 EULA** comercial (https://juce.com/legal/juce-8-licence/) | Se baja con CMake (`FetchContent`); no se commitea. |
| SDK VST3 que trae JUCE | el que JUCE declara para su copia del SDK | Lo incluye JUCE; ver los avisos dentro de `modules/juce_audio_processors/format_types/VST3_SDK` de la versión usada. |
| Audio Unit | SDK de Apple (headers del sistema) | Solo en macOS. |

## Decisión pendiente (importante para repartir el plugin)
JUCE 8 se puede usar bajo **AGPLv3** (en ese caso el plugin y su código fuente deben publicarse bajo AGPLv3 si se
distribuye) o bajo la **licencia JUCE 8**, que tiene niveles (incluido uno gratuito con límites de facturación y con
pantalla de inicio de JUCE). **Verificá en https://juce.com/legal/juce-8-licence/ y https://juce.com/get-juce/ qué nivel
corresponde** a un lead magnet gratis y a una futura versión paga antes de publicar binarios. Esto no se resolvió acá a
propósito: no se inventan condiciones de licencia. El código propio de esta carpeta no tiene licencia asignada todavía;
Tomás decide (por ejemplo, propietario cerrado + licencia JUCE, o AGPLv3 abierto). El roadmap de productos y la matriz
de licencias de otros repos está en la tarea N13 (`docs/LAB-REPOS.md`).

`JUCE_DISPLAY_SPLASH_SCREEN` queda en su valor por defecto (1): con licencias que no permiten quitarla, JUCE muestra su
pantalla de inicio; con AGPLv3 o licencias que lo permiten puede ponerse en 0.

## Samples del usuario
El slot de chops carga archivos del usuario (p. ej. *Schmulenson Vocal Chops Vol.1*). Esos audios son contenido de
Schmulenson (o de un/a cantante con permiso firmado, ver `career-os/05-vst-manychat.md`); **no viajan dentro del plugin**.

## Marcas
"909" y "808" se usan solo para describir un estilo de sonido. Roland, TR-909, TR-808, Korg y M1 son marcas de sus dueños; SCHMU 90s no está afiliado ni avalado por ellos y no reproduce samples ni código de esos productos.
