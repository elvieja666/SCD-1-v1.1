# SCD-1 v1.10.1 (sidechain only)

Silencia el audio que entra, en full range, siguiendo los cycles de la señal que le mandes por el **sidechain input**. Interfaz propia (fondo gris oscuro, perillas con aro, fader vertical y vúmetro de salida), fuente Inter incluida en `Resources/` (licencia OFL).

## Controles (en este orden)

| Control | Qué hace |
|---|---|
| **Depth** | De -inf (silencio, sin efecto) a +6 dB. Por defecto 0 dB, que queda al 60 % del slider: el tramo de 0 a +6 dB ocupa el 40 % del recorrido, y de -24 a 0 dB el 45 %.  A 0 dB, un key a 0 dBFS silencia el audio por completo; con valores positivos, el silence total se alcanza con keys más bajos |
| **Depth Bypass** | Apaga el efecto (con un fade de 5 ms, sin clicks). El Output sigue aplicándose |
| **Key Listen** | Solo se escucha el key (el sidechain input), en lugar del audio. Sirve para comprobar que el sidechain llega y a qué level. Sin nada conectado, queda en silencio. El Output no se aplica en este modo |
| **Mode** | **Half** (por defecto): silencia un solo semiciclo de cada cycle. **Whole**: silencia un cycle completo, deja pasar el siguiente, y así alternando |
| **Polarity** | De 0° (Positive) a 180° (Negative). Mueve dónde del waveform cae el silence |
| **Output** | Volumen final (-12 a +6 dB), escala lineal: cada sexto del fader son 3 dB (marcas en +6, +3, 0, -3, -6, -9, -12). Por defecto 0 dB, al 67 % del recorrido |

**Escribir valores exactos:** en Depth, Polarity y Output, hacé doble clic sobre la perilla, el fader o el valor numérico, escribí el valor y apretá Enter. En Depth podés escribir `-inf`; en Polarity, grados (`90` o `90°`) o `pos` / `neg`. Depth Bypass, Key Listen y Mode son opciones fijas: se cambian con un clic.

## Cómo se calcula el efecto

La reducción **sigue el level del sidechain**: la cantidad de efecto es proporcional a la amplitud del key (en cada cycle se toma su peak) y se multiplica por **Depth**. Un key a -6 dBFS da la mitad del efecto de uno a 0 dBFS, y a -12 dBFS un cuarto. No hay threshold que lo corte de golpe.

## Cómo usarlo

1. Poné SCD-1 en la pista (o el master) que querés afectar.
2. Mandá al **sidechain input** una señal con solo el **sub** (un kick o un 808 con low-pass), a un level razonable. El plugin no tiene filtros ni ganancia de entrada.
3. Activá **Key Listen** para comprobar qué le llega. Si no escuchás nada, el problema está en el ruteo del DAW. Si lo escuchás muy bajo, el efecto va a ser muy leve: subí el level de la señal que mandás, o usá Depth positivo (hasta +6 dB).
4. Sin señal en el sidechain, el audio pasa sin cambios.

Si mandás un bus completo (con medios y agudos), el plugin detecta cycles en todas las frecuencias y el patrón se vuelve irregular. Para que el ritmo sea limpio, el key tiene que contener solo el sub.

### Logic Pro

- El sidechain se elige en el menú **Side Chain** del header de la ventana del plugin.
- Si no aparece: cerrá Logic, reemplazá `SCD-1.component`, y en Logic Pro > Settings > Plug-in Manager seleccioná SCD-1 y tocá **Reset & Rescan Selection**.

## Notas

- El vúmetro de la derecha muestra el nivel de salida (+6 a -inf, cada 3 dB; el segmento de abajo se enciende cuando hay señal; la marca blanca es el pico).
- Output cambió a escala lineal: los proyectos guardados con el valor en dB se restauran igual (el parámetro guarda dB), solo cambia la posición del fader.
- Solo atenúa, nunca amplifica. No agrega latency ni filtros en el camino del audio.
- El sidechain se detecta en mono (promedio de L y R), y la misma bajada se aplica a todos los canales.
- Cuando el sidechain se apaga (entre golpes), el contador se reinicia y cada golpe arranca igual (el primer cycle se silencia).
- Depth ahora está en dB y es un parámetro nuevo: los proyectos guardados con una versión anterior arrancan con Depth en 0 dB.

## Compilar e instalar

1. Reemplazá los archivos de tu carpeta del proyecto por los de este zip (incluida la carpeta oculta `.github`).
2. En GitHub Desktop: escribí un Summary, **Commit to main**, **Push origin**.
3. Esperá la compilación en **Actions**, descargá el artifact de Windows o Mac.
4. Mac: `.vst3` a `~/Library/Audio/Plug-Ins/VST3/` y `.component` a `~/Library/Audio/Plug-Ins/Components/`, después `xattr -cr` sobre cada uno.
   Windows: `.vst3` a `C:\Program Files\Common Files\VST3\`

## Cambio de nombre

El plugin ahora se llama **SCD-1** (antes Phantom Asym). Aparece con el nombre nuevo en Logic. Los archivos instalados se llaman `SCD-1.component` y `SCD-1.vst3`: **borrá los viejos `Phantom Asym.component` / `Phantom Asym.vst3`** de sus carpetas para que no queden duplicados. El identificador interno no cambió, así que los proyectos guardados siguen cargando el plugin con sus valores.
