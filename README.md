# membyte

Explorador visual de bytes para la terminal. Le das cualquier archivo de tu
computadora y te lo dibuja como un mapa de colores, donde cada cuadradito es
un byte.

Sirve para ver de un vistazo cosas que en un editor de texto son invisibles:
donde hay relleno, donde hay texto escondido dentro de un binario, donde
empiezan y terminan las secciones de un ejecutable, si un archivo esta
comprimido o no.

```
  +-------------------------------------------------------+
  |            m e m b y t e                              |
  |        explorador visual de bytes                     |
  +-------------------------------------------------------+

   archivo:  C:\Windows\notepad.exe
   tamanio:  360.4 KB
   paleta:   texto (verde = letras, gris = vacio, rojo = binario)

  ------------------------ VER ------------------------
   1)  elegir archivo   (explorador de carpetas)
   2)  vista PANORAMICA (todo el archivo en 1 pantalla)
   3)  vista DETALLE    (byte por byte, de a paginas)
   4)  estadisticas     (que tipo de archivo es)
```

## Que hace

- **Vista panoramica**: el archivo entero comprimido en una sola pantalla.
  Cada cuadradito resume un bloque de bytes, asi entra un ejecutable de 50 MB
  sin scrollear.
- **Vista detalle**: byte por byte, paginada segun el alto real de tu consola.
  Nunca se te escapa el contenido por arriba de la pantalla.
- **Dos paletas**: una de calor (por valor del byte) y una que resalta el
  texto legible dentro de datos binarios.
- **Explorador de carpetas** integrado: navegas tu disco sin salir del
  programa, o pegas una ruta directamente.
- **Estadisticas**: tamanio, porcentaje de texto legible, byte mas frecuente,
  y un veredicto de si parece texto o binario.
- **Explicacion desde cero** al arrancar, para quien nunca vio un byte en su
  vida: que es un byte, como se arma un color con RGB, y que mirar en el mapa.

## Probalo en 30 segundos

Abri el propio ejecutable del programa con la paleta de texto (opcion 5 para
cambiarla): vas a ver en verde todos los mensajes del menu incrustados dentro
del binario, y en rojo el codigo maquina.

Despues compara un `.txt`, una foto `.jpg` y un `.exe`. Se ven completamente
distintos, y en un par de segundos entendes por que.

## Compilar

Necesita un compilador con soporte de **C++17** (usa `<filesystem>`).
No tiene ninguna dependencia externa: solo la libreria estandar.

### Visual Studio (Windows)

**Archivo > Abrir > Carpeta...** y elegi la carpeta del proyecto. Visual
Studio detecta el `CMakeLists.txt` y configura todo solo. Despues `Ctrl+F5`.

### CMake (cualquier sistema)

```bash
cmake -B build
cmake --build build
```

### g++ / clang directo

```bash
g++ -std=c++17 -O2 -o membyte main.cpp membyte.cpp
```

## Usar

```bash
./membyte                 # arranca el menu interactivo
./membyte archivo.exe     # abre ese archivo directamente
./membyte --selftest      # verifica que las formulas de color esten bien
```

En Windows tambien podes hacer doble click en `membyte.exe`: arranca con el
menu y no se cierra solo al terminar.

## Estructura

| Archivo | Que tiene |
|---|---|
| `membyte.h` | Las declaraciones: que funciones existen y para que sirven. |
| `membyte.cpp` | El motor: colores, lectura de archivos, las dos vistas, estadisticas, navegador. |
| `main.cpp` | Solo el menu interactivo. No sabe nada de bytes ni de colores. |
| `CMakeLists.txt` | Configuracion de compilacion (7 lineas). |

## Notas tecnicas

- Todo el codigo y la salida estan en **ASCII puro**: la consola de Windows no
  decodifica UTF-8 por defecto y los acentos saldrian como basura.
- En Windows hay que pedirle permiso a la consola para mostrar colores ANSI
  (`ENABLE_VIRTUAL_TERMINAL_PROCESSING`); el programa lo hace solo al arrancar.
- `NOMINMAX` es obligatorio en Windows: `windows.h` define `min`/`max` como
  macros y rompe `std::min`.
- La paginacion consulta el alto real de la consola
  (`GetConsoleScreenBufferInfo` en Windows, `ioctl(TIOCGWINSZ)` en Unix).
- El archivo se carga entero en memoria. Para archivos de varios GB habria que
  leer de a pedazos.

## Licencia

MIT. Hace lo que quieras con esto.
