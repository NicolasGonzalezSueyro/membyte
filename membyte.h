// ===========================================================================
// membyte.h - "que sabe hacer" el programa (las declaraciones)
// ===========================================================================
// Este archivo es el INDICE: dice que funciones existen y para que sirven,
// pero no como estan hechas por dentro (eso esta en membyte.cpp).
//
// Sirve para que main.cpp pueda usar las funciones sin necesitar ver el
// codigo de cada una. Es la misma idea que el indice de un libro de recetas:
// leyendolo ya sabes que platos hay, sin leer las recetas enteras.
// ===========================================================================

#pragma once   // evita que este archivo se incluya dos veces por error

#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// COLORES DE LA INTERFAZ
// ---------------------------------------------------------------------------
// Codigos ANSI para darle color al menu. Son textos que la terminal
// interpreta como "cambia el color de aca en adelante" en vez de mostrarlos.
// Siempre hay que cerrar con C_OFF, si no se mancha el resto de la pantalla.
#define C_TITLE "\x1b[1;36m"   // cyan brillante: titulos
#define C_KEY   "\x1b[1;33m"   // amarillo: numeros de opcion
#define C_OK    "\x1b[1;32m"   // verde: valores activos
#define C_WARN  "\x1b[1;31m"   // rojo: avisos
#define C_DIM   "\x1b[90m"     // gris: texto secundario
#define C_OFF   "\x1b[0m"      // volver al color normal

// ---------------------------------------------------------------------------
// PALETAS: las dos formas distintas de pintar los bytes.
// ---------------------------------------------------------------------------
// Una "paleta" es la regla que decide de que color se pinta cada byte.
// Cambiando la paleta ves cosas distintas del mismo archivo.
enum Palette {
    PAL_HEAT = 0,   // mapa de calor: valor bajo = azul, medio = verde, alto = rojo
    PAL_TEXT = 1,   // resalta texto: verde = letras, gris = vacio, rojo = binario
    PAL_COUNT = 2   // cuantas paletas hay (para poder rotar entre ellas)
};

// ---------------------------------------------------------------------------
// CONSOLA
// ---------------------------------------------------------------------------

// Le pide permiso a Windows para mostrar colores. Llamarla UNA vez al arrancar.
void enable_ansi_on_windows();

// Muestra un mensaje y espera a que el usuario escriba algo y apriete Enter.
std::string ask(const char* prompt);

// Cuantas filas de alto tiene la ventana de la consola ahora mismo.
// Sirve para no imprimir mas de lo que entra en pantalla.
int console_rows();

// Cuantas columnas de ancho tiene la ventana de la consola.
int console_cols();

// Borra la pantalla, para que cada vista arranque limpia.
void clear_screen();

// ---------------------------------------------------------------------------
// COLORES DE LOS BYTES
// ---------------------------------------------------------------------------

const char* palette_name(int palette);

// Dice si un byte se puede mostrar como letra/numero/simbolo en pantalla.
bool printable(unsigned char c);

// EL CORAZON DEL PROGRAMA: convierte un byte (numero de 0 a 255) en un color.
// Deja el resultado en r, g y b (rojo, verde y azul, de 0 a 255 cada uno).
void byte_color(unsigned char value, int palette, int& r, int& g, int& b);

// ---------------------------------------------------------------------------
// ARCHIVOS
// ---------------------------------------------------------------------------

// Carga un archivo entero en memoria como lista de bytes crudos.
bool read_file(const std::string& path, std::vector<unsigned char>& out);

// Convierte un tamanio en bytes a algo legible: "1.4 MB", "823 KB".
std::string human_size(size_t bytes);

// ---------------------------------------------------------------------------
// LAS DOS FORMAS DE VER UN ARCHIVO
// ---------------------------------------------------------------------------

// VISTA DETALLE: byte por byte, de a una pantalla por vez.
// Se frena en cada pagina y espera Enter, asi no se te escapa el contenido
// por arriba de la pantalla. Con 'q' volves al menu.
void render_paged(const std::vector<unsigned char>& data, int bytes_per_row,
                  bool show_text, int palette);

// VISTA PANORAMICA: el archivo ENTERO comprimido en una sola pantalla.
// Cada cuadradito representa un bloque grande de bytes. Sirve para ver la
// estructura general de un archivo pesado sin scrollear miles de lineas.
void render_overview(const std::vector<unsigned char>& data, int palette);

// Barra de colores de referencia + explicacion de las columnas.
void print_legend(int palette, bool show_text);

// Numeros sobre el archivo: tamanio, cuanto es texto, byte mas comun.
void print_stats(const std::vector<unsigned char>& data);

// Explicacion desde cero, para alguien que nunca escucho hablar de bytes.
void print_explainer();

// ---------------------------------------------------------------------------
// NAVEGADOR DE CARPETAS
// ---------------------------------------------------------------------------

// Explorador dentro de la consola para elegir un archivo.
// Devuelve la ruta elegida, o "" (vacio) si el usuario cancela.
std::string browse(const std::string& start_dir);

// Carpeta donde esta parado el programa ahora mismo.
std::string current_dir();

// Carpeta que contiene a un archivo.
std::string parent_dir(const std::string& path);

// ---------------------------------------------------------------------------
// CHEQUEO INTERNO
// ---------------------------------------------------------------------------

// Verifica que los colores se calculen bien. Devuelve true si esta todo ok.
bool selftest();
