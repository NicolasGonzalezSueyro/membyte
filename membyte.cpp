// ===========================================================================
// membyte.cpp - "como lo hace" (el motor del programa)
// ===========================================================================
// Todo el archivo esta escrito en ASCII puro a proposito: la consola de
// Windows no decodifica UTF-8 por defecto y los acentos saldrian como basura.
// Por eso vas a leer "tamanio" y no la palabra con enie.
// ===========================================================================

#ifdef _WIN32
// NOMINMAX: windows.h define min y max como macros de texto y eso rompe
// std::min. Sin esta linea el programa ni compila en Visual Studio.
// _CRT_SECURE_NO_WARNINGS: calla el aviso de Microsoft por usar fopen
// (que funciona en todos lados) en vez de fopen_s (que es solo de ellos).
#define NOMINMAX
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "membyte.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/ioctl.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;   // apodo corto para no escribir tanto

// ---------------------------------------------------------------------------
// RUTAS CON CARACTERES RAROS (acentos, enies, alfabetos no latinos)
// ---------------------------------------------------------------------------
// Windows guarda las rutas internamente en UTF-16. Convertirlas a texto comun
// con path.string() FALLA (lanza una excepcion y cierra el programa de golpe)
// si el nombre tiene algun caracter que no entra en la codificacion vieja del
// sistema. Es el clasico "C:\Users\Nicolas\cancion.mp3" que rompe todo.
//
// La solucion es usar siempre UTF-8, que representa cualquier caracter y
// nunca falla. Estas dos funciones son la unica puerta de entrada y salida:
// de fs::path a texto, y de texto a fs::path.

static std::string pstr(const fs::path& p) {
    auto u8 = p.u8string();   // esta version nunca lanza excepcion
    return std::string(reinterpret_cast<const char*>(u8.data()), u8.size());
}

static fs::path from_utf8(const std::string& s) {
#if defined(__cpp_lib_char8_t)   // C++20 en adelante
    return fs::path(std::u8string(reinterpret_cast<const char8_t*>(s.data()), s.size()));
#else                            // C++17
    return fs::u8path(s);
#endif
}

// ===========================================================================
// 1. CONSOLA
// ===========================================================================

void enable_ansi_on_windows() {
    // Los colores en la terminal se piden con "codigos de escape ANSI":
    // textos raros como \x1b[48;2;255;0;0m que significan "de aca en adelante
    // pinta el fondo de rojo". Linux y Mac los entienden desde siempre.
    // Windows los entiende, pero hay que activarlos a mano. Eso hace esto.
#ifdef _WIN32
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);   // agarra "la pantalla"
    DWORD mode = 0;
    if (h != INVALID_HANDLE_VALUE && GetConsoleMode(h, &mode)) {
        SetConsoleMode(h, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }
    // Ademas le decimos a la consola que el texto viene en UTF-8. Sin esto,
    // los nombres de archivo con acentos, enies o simbolos raros salen rotos
    // (y peor: convertirlos revienta el programa). Nuestro texto es ASCII,
    // que es parte de UTF-8, asi que no cambia nada de lo nuestro.
    SetConsoleOutputCP(CP_UTF8);
#endif
}

std::string ask(const char* prompt) {
    std::printf("%s", prompt);
    std::fflush(stdout);            // forza a que el mensaje aparezca YA
    std::string line;
    std::getline(std::cin, line);   // espera el Enter del usuario
    return line;
}

// Preguntarle a la consola cuanto mide evita el problema mas molesto de este
// tipo de programas: escupir 5000 lineas y que el principio se pierda arriba.
int console_rows() {
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi))
        return csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
#else
    struct winsize w{};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0 && w.ws_row > 0) return w.ws_row;
#endif
    return 24;   // valor clasico de consola, por si no se pudo averiguar
}

int console_cols() {
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi))
        return csbi.srWindow.Right - csbi.srWindow.Left + 1;
#else
    struct winsize w{};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0 && w.ws_col > 0) return w.ws_col;
#endif
    return 80;
}

void clear_screen() {
    // \x1b[2J borra todo, \x1b[H manda el cursor arriba a la izquierda.
    std::printf("\x1b[2J\x1b[H");
}

// ===========================================================================
// 2. COLORES
// ===========================================================================
//
// COMO FUNCIONA UN COLOR EN LA COMPUTADORA
// ----------------------------------------
// Todo color en pantalla se arma mezclando tres luces: Roja, Verde y Azul
// (RGB). Cada una va de 0 (apagada) a 255 (a full). Ejemplos:
//     (255,   0,   0) = rojo puro
//     (  0, 255,   0) = verde puro
//     (255, 255,   0) = rojo + verde = amarillo
//     (  0,   0,   0) = todo apagado = negro
//
// Nuestro trabajo es: dado un byte (numero de 0 a 255), inventar una regla
// que lo convierta en una de esas mezclas. Esa regla es una "paleta".

const char* palette_name(int palette) {
    return palette == PAL_HEAT
        ? "calor (azul = valor bajo, verde = medio, rojo = alto)"
        : "texto (verde = letras, gris = vacio, rojo = datos binarios)";
}

bool printable(unsigned char c) {
    // En la tabla ASCII, del 32 al 126 estan los caracteres que se ven:
    // el 32 es el espacio, el 65 es 'A', el 97 es 'a', el 126 es '~'.
    // Abajo del 32 hay codigos invisibles (saltos de linea, tabs) y arriba
    // del 126 hay bytes que directamente no son texto ASCII.
    return c >= 32 && c <= 126;
}

void byte_color(unsigned char value, int palette, int& r, int& g, int& b) {
    if (palette == PAL_TEXT) {
        // PALETA "TEXTO": no le importa el valor numerico, le importa QUE TIPO
        // de byte es. Sirve para ver donde hay texto legible escondido dentro
        // de un archivo binario (por ejemplo, los mensajes de un .exe).
        if (value == 0) {
            r = 40;  g = 40;  b = 40;    // gris oscuro: relleno / espacio vacio
        } else if (printable(value)) {
            r = 0;   g = 220; b = 60;    // verde: texto que se puede leer
        } else if (value == '\n' || value == '\r' || value == '\t') {
            r = 0;   g = 120; b = 200;   // azul: saltos de linea y tabs
        } else {
            r = 200; g = 60;  b = 0;     // rojo: datos binarios
        }
        return;
    }

    // PALETA "CALOR": el color depende del valor numerico del byte, formando
    // un degrade continuo de azul -> verde -> rojo:
    //
    //   byte 0    -> azul  (0, 0, 255)
    //   byte 128  -> verde (0, 254, 0)
    //   byte 255  -> rojo  (254, 0, 0)
    //
    // Tres formulas lineales, una por cada luz: el rojo aparece recien en la
    // segunda mitad, el azul se apaga en la primera, y el verde sube hasta la
    // mitad y despues baja.
    // ponytail: rampa lineal simple. Si algun dia hace falta un degrade
    // perceptualmente correcto (viridis, magma), se cambia solo aca.
    r = value < 128 ? 0 : (value - 128) * 2;
    g = value < 128 ? value * 2 : (255 - value) * 2;
    b = value < 128 ? 255 - value * 2 : 0;
}

// Imprime un "cuadrito" de color. El truco: no dibujamos un cuadrado, sino
// que pintamos el FONDO de uno o dos espacios en blanco.
//   \x1b[48;2;R;G;Bm -> "pinta el fondo de este color"
//   \x1b[0m          -> "volve al color normal" (si falta, se mancha todo)
static void print_swatch(unsigned char value, int palette, const char* cell) {
    int r, g, b;
    byte_color(value, palette, r, g, b);
    std::printf("\x1b[48;2;%d;%d;%dm%s\x1b[0m", r, g, b, cell);
}

// ===========================================================================
// 3. ARCHIVOS
// ===========================================================================

bool read_file(const std::string& path, std::vector<unsigned char>& out) {
    // Uso ifstream con un fs::path en vez del clasico fopen porque fopen no
    // sabe abrir rutas con caracteres raros en Windows. ifstream si.
    // std::ios::binary es clave: le pide al sistema que NO toque nada, que nos
    // de los bytes exactamente como estan en el disco.
    std::ifstream f(from_utf8(path), std::ios::binary);
    if (!f) {
        std::printf(C_WARN "\n  No se pudo abrir ese archivo.\n" C_OFF);
        return false;
    }

    // Truco clasico para saber cuanto pesa un archivo:
    f.seekg(0, std::ios::end);            // 1) saltar hasta el final
    std::streamoff size = f.tellg();      // 2) preguntar "en que posicion estoy?"
    f.seekg(0, std::ios::beg);            // 3) volver al principio para leerlo
    if (size < 0) {
        std::printf(C_WARN "\n  No se pudo medir el archivo.\n" C_OFF);
        return false;
    }

    out.assign((size_t)size, 0);
    if (size > 0) f.read(reinterpret_cast<char*>(out.data()), size);
    if (!f && size > 0) {
        std::printf(C_WARN "\n  Lectura incompleta del archivo.\n" C_OFF);
        return false;
    }
    return true;
    // ponytail: carga el archivo entero en memoria. Para archivos de varios GB
    // habria que leer de a pedazos, pero para esta herramienta sobra.
}

std::string human_size(size_t bytes) {
    char buf[64];
    if (bytes < 1024)               std::snprintf(buf, sizeof buf, "%zu bytes", bytes);
    else if (bytes < 1024 * 1024)   std::snprintf(buf, sizeof buf, "%.1f KB", bytes / 1024.0);
    else                            std::snprintf(buf, sizeof buf, "%.1f MB", bytes / (1024.0 * 1024.0));
    return buf;
}

// ===========================================================================
// 4. DIBUJAR
// ===========================================================================

void print_legend(int palette, bool show_text) {
    std::printf("\n" C_TITLE "PALETA: " C_OFF "%s\n\n  ", palette_name(palette));

    // Barra de referencia: recorro los 256 valores de a 4 y pinto un cuadrito
    // de cada uno. Como usa la misma funcion byte_color() que el mapa, si
    // cambias de paleta la leyenda se actualiza sola.
    for (int v = 0; v <= 255; v += 4) print_swatch((unsigned char)v, palette, " ");

    std::printf("\n  " C_DIM "byte 0%*s" C_OFF "\n\n", 56, "byte 255");
    std::printf(C_DIM "columna izquierda: posicion dentro del archivo (offset, en hexadecimal)\n" C_OFF);
    if (show_text)
        std::printf(C_DIM "columna derecha:   esos bytes leidos como texto "
                    "('.' = no es una letra)\n" C_OFF);
    std::printf("\n");
}

// Dibuja una sola fila del mapa detallado.
static void render_row(const std::vector<unsigned char>& data, size_t start,
                       int bytes_per_row, bool show_text, int palette) {
    size_t end = std::min(start + (size_t)bytes_per_row, data.size());

    // %08zx = el numero en hexadecimal, rellenado con ceros hasta 8 lugares.
    // Es la direccion donde empieza esta fila dentro del archivo.
    std::printf(C_DIM "%08zx" C_OFF "  ", start);

    for (size_t j = start; j < end; ++j) print_swatch(data[j], palette, "  ");

    if (show_text) {
        std::printf("  ");
        for (size_t j = start; j < end; ++j)
            std::putchar(printable(data[j]) ? data[j] : '.');
    }
    std::putchar('\n');
}

void render_paged(const std::vector<unsigned char>& data, int bytes_per_row,
                  bool show_text, int palette) {
    if (data.empty()) { std::printf("\n  El archivo esta vacio.\n"); return; }

    size_t total_rows = (data.size() + bytes_per_row - 1) / bytes_per_row;

    // Cuantas filas entran por pantalla. Le resto lugar para la leyenda de
    // arriba y la barra de abajo, y nunca bajo de 5 (por si la ventana es minima).
    int rows_per_page = std::max(5, console_rows() - 12);

    size_t row = 0;
    while (row < total_rows) {
        clear_screen();
        print_legend(palette, show_text);

        size_t last = std::min(row + (size_t)rows_per_page, total_rows);
        for (; row < last; row++)
            render_row(data, row * bytes_per_row, bytes_per_row, show_text, palette);

        // Barra de progreso de abajo: te dice donde estas parado.
        size_t shown = std::min(row * bytes_per_row, data.size());
        int pct = (int)(100.0 * shown / data.size());
        std::printf("\n" C_TITLE "[ %s de %s  (%d%%) ]" C_OFF "  ",
                    human_size(shown).c_str(), human_size(data.size()).c_str(), pct);

        if (row >= total_rows) {
            std::printf(C_DIM "fin del archivo." C_OFF "\n");
            ask(C_KEY "Enter" C_OFF " para volver al menu... ");
            return;
        }
        std::string in = ask(C_KEY "Enter" C_OFF "=seguir  "
                             C_KEY "q" C_OFF "=volver al menu > ");
        if (in == "q" || in == "Q") return;
    }
}

void render_overview(const std::vector<unsigned char>& data, int palette) {
    if (data.empty()) { std::printf("\n  El archivo esta vacio.\n"); return; }

    // La idea: en vez de una celda por byte, una celda por BLOQUE de bytes.
    // Asi entra cualquier archivo, por grande que sea, en una sola pantalla.
    int cols = std::max(16, std::min(console_cols() - 14, 100)) / 2;
    int rows = std::max(5, console_rows() - 14);

    size_t cells = (size_t)cols * rows;
    size_t block = (data.size() + cells - 1) / cells;   // bytes que resume cada celda
    if (block == 0) block = 1;

    clear_screen();
    std::printf("\n" C_TITLE "VISTA PANORAMICA" C_OFF " - el archivo entero en una pantalla\n");
    std::printf(C_DIM "  cada cuadradito resume %s  |  archivo: %s\n" C_OFF,
                human_size(block).c_str(), human_size(data.size()).c_str());
    std::printf(C_DIM "  paleta: %s\n\n" C_OFF, palette_name(palette));

    for (int rIdx = 0; rIdx < rows; rIdx++) {
        size_t row_start = (size_t)rIdx * cols * block;
        if (row_start >= data.size()) break;
        std::printf(C_DIM "%08zx" C_OFF "  ", row_start);

        for (int c = 0; c < cols; c++) {
            size_t start = row_start + (size_t)c * block;
            if (start >= data.size()) break;
            size_t end = std::min(start + block, data.size());

            // ponytail: uso el promedio del bloque como valor representativo.
            // No es exacto (un bloque mezcla bytes muy distintos), pero es lo
            // que hace que se vea la ESTRUCTURA del archivo de un vistazo.
            size_t sum = 0;
            for (size_t j = start; j < end; j++) sum += data[j];
            print_swatch((unsigned char)(sum / (end - start)), palette, "  ");
        }
        std::putchar('\n');
    }
    std::printf("\n");
    ask(C_KEY "Enter" C_OFF " para volver al menu... ");
}

// ===========================================================================
// 5. ESTADISTICAS
// ===========================================================================

void print_stats(const std::vector<unsigned char>& data) {
    if (data.empty()) { std::printf("\n  El archivo esta vacio.\n"); return; }

    // Un contador por cada valor posible de byte. freq[65] va a terminar
    // valiendo "cuantas veces aparecio la letra A" (65 es la 'A' en ASCII).
    std::array<size_t, 256> freq{};
    size_t printables = 0, zeros = 0;
    for (unsigned char c : data) {
        freq[c]++;
        if (printable(c)) printables++;
        if (c == 0) zeros++;
    }

    // max_element devuelve un puntero al contador mas alto. Restandole el
    // inicio del array obtengo la POSICION, que es justo el valor del byte.
    size_t top = (size_t)(std::max_element(freq.begin(), freq.end()) - freq.begin());
    double pct_text = 100.0 * printables / data.size();

    std::printf("\n" C_TITLE "--- ESTADISTICAS ---" C_OFF "\n");
    std::printf("  tamanio:        %s\n", human_size(data.size()).c_str());
    std::printf("  texto legible:  %.1f%%\n", pct_text);
    std::printf("  bytes en cero:  %zu (%.1f%%)\n", zeros, 100.0 * zeros / data.size());
    std::printf("  byte mas comun: 0x%02zx (%zu en decimal", top, top);
    if (printable((unsigned char)top)) std::printf(", el caracter '%c'", (char)top);
    std::printf("), aparece %zu veces\n", freq[top]);
    // ponytail: el corte en 85%% es una heuristica, no ciencia. Alcanza para
    // distinguir un .txt de un .exe de un vistazo.
    std::printf("  veredicto:      " C_OK "%s" C_OFF "\n\n",
                pct_text > 85.0 ? "parece un archivo de TEXTO"
                                : "parece un archivo BINARIO");
}

void print_explainer() {
    clear_screen();
    std::printf("\n" C_TITLE
        "=====================================================================\n"
        " QUE ES TODO ESTO (explicacion desde cero)\n"
        "=====================================================================" C_OFF "\n\n");
    std::printf(C_KEY "1)" C_OFF " TODO archivo de tu computadora (una foto, un Word, un programa)\n");
    std::printf("   es, por dentro, una lista larguisima de numeros del 0 al 255.\n");
    std::printf("   Cada uno de esos numeros se llama " C_OK "BYTE" C_OFF ".\n\n");
    std::printf(C_KEY "2)" C_OFF " Lo que cambia entre una foto y un texto no son los numeros en si,\n");
    std::printf("   sino el ORDEN y el PATRON que forman.\n\n");
    std::printf(C_KEY "3)" C_OFF " Este programa le asigna un color a cada numero y los dibuja en\n");
    std::printf("   fila. Asi, patrones invisibles en el Bloc de notas aca se ven\n");
    std::printf("   como manchas, rayas y bloques.\n\n");
    std::printf(C_KEY "4)" C_OFF " Los colores salen de mezclar tres luces: Roja, Verde y Azul.\n");
    std::printf("   Cada luz va de 0 (apagada) a 255 (al maximo):\n");
    std::printf("      rojo puro  = R 255, G 0,   B 0\n");
    std::printf("      verde puro = R 0,   G 255, B 0\n");
    std::printf("      amarillo   = R 255, G 255, B 0   (rojo + verde)\n\n");
    std::printf(C_KEY "5)" C_OFF " QUE MIRAR:\n");
    std::printf("      - Zonas de un solo color parejo: relleno o datos repetidos.\n");
    std::printf("      - Zonas con ruido de muchos colores: datos comprimidos o\n");
    std::printf("        codigo de programa.\n");
    std::printf("      - Con la paleta 'texto', todo lo " C_OK "VERDE" C_OFF " es texto legible.\n");
    std::printf("        Abri un .exe con esa paleta: vas a ver los mensajes\n");
    std::printf("        escondidos adentro del programa.\n\n");
    std::printf(C_KEY "6)" C_OFF " Las dos vistas:\n");
    std::printf("      - " C_OK "DETALLE" C_OFF ": byte por byte, de a una pagina por vez.\n");
    std::printf("      - " C_OK "PANORAMICA" C_OFF ": el archivo entero comprimido en una\n");
    std::printf("        pantalla. Para archivos grandes, empeza siempre por aca.\n\n");
    std::printf(C_DIM "Proba abrir cosas distintas y comparar: un .txt, una foto .jpg y\n");
    std::printf("un .exe se ven completamente distintos.\n" C_OFF "\n");
}

// ===========================================================================
// 6. NAVEGADOR DE CARPETAS
// ===========================================================================

std::string current_dir() {
    std::error_code ec;
    fs::path p = fs::current_path(ec);
    return ec ? std::string(".") : pstr(p);
}

std::string parent_dir(const std::string& path) {
    fs::path p = from_utf8(path).parent_path();
    return p.empty() ? current_dir() : pstr(p);
}

// Version "no lanza nunca": si preguntar por un archivo falla (permisos, un
// acceso directo roto, una unidad desconectada), devuelve false en vez de
// tirar una excepcion que cierre el programa.
static bool is_dir_safe(const fs::path& p) {
    std::error_code ec;
    return fs::is_directory(p, ec);   // ec absorbe el error
}

static bool is_file_safe(const fs::path& p) {
    std::error_code ec;
    return fs::is_regular_file(p, ec);
}

std::string browse(const std::string& start_dir) {
    fs::path dir = from_utf8(start_dir);

    while (true) {
        // 1) Junto todo lo que hay adentro de la carpeta.
        std::vector<fs::path> items;
        std::error_code ec;
        auto opts = fs::directory_options::skip_permission_denied;
        for (const auto& e : fs::directory_iterator(dir, opts, ec)) items.push_back(e.path());
        if (ec) {
            std::printf(C_WARN "\n  No se pudo leer esa carpeta (puede ser del sistema).\n" C_OFF);
            ask("  Enter para volver... ");
            return "";
        }

        // 2) Ordeno: primero las carpetas, despues los archivos, alfabetico.
        std::sort(items.begin(), items.end(), [](const fs::path& a, const fs::path& b) {
            bool da = is_dir_safe(a), db = is_dir_safe(b);
            if (da != db) return da;
            return pstr(a.filename()) < pstr(b.filename());
        });

        // 3) Muestro la lista numerada.
        clear_screen();
        std::printf("\n" C_TITLE "== %s ==" C_OFF "\n\n", pstr(dir).c_str());
        std::printf("  " C_KEY "0" C_OFF ") " C_DIM ".. (subir una carpeta)" C_OFF "\n");
        for (size_t i = 0; i < items.size(); i++) {
            bool d = is_dir_safe(items[i]);
            std::printf("  " C_KEY "%2zu" C_OFF ") %s%s%s%s\n", i + 1,
                        d ? C_TITLE : "", pstr(items[i].filename()).c_str(),
                        d ? "/" : "", d ? C_OFF : "");
        }
        std::printf("\n  " C_KEY "r" C_OFF ") escribir/pegar una ruta a mano    "
                    C_KEY "q" C_OFF ") volver al menu\n");

        // 4) Interpreto lo que escribio el usuario.
        std::string in = ask("\n  elegi> ");

        if (in == "q" || in == "Q") return "";

        if (in == "r" || in == "R") {
            std::string p = ask("  ruta completa (ej: C:\\Users\\Nico\\Desktop\\foto.jpg): ");
            if (p.empty()) continue;
            // Windows deja pegar rutas entre comillas; se las saco.
            if (p.size() >= 2 && p.front() == '"' && p.back() == '"')
                p = p.substr(1, p.size() - 2);
            fs::path path = from_utf8(p);
            if (is_dir_safe(path)) { dir = path; continue; }    // era carpeta: entro
            if (is_file_safe(path)) return pstr(path);          // era archivo: listo
            std::printf(C_WARN "  Esa ruta no existe.\n" C_OFF);
            ask("  Enter para seguir... ");
            continue;
        }

        if (in == "0") {
            fs::path up = dir.parent_path();
            if (!up.empty() && up != dir) dir = up;   // si ya estoy en la raiz, me quedo aca
            continue;
        }

        // strtol convierte texto a numero. Si endp quedo igual al inicio,
        // el usuario escribio algo que no era un numero.
        char* endp = nullptr;
        long n = std::strtol(in.c_str(), &endp, 10);
        if (endp == in.c_str() || n < 1 || (size_t)n > items.size()) {
            std::printf(C_WARN "  Opcion invalida.\n" C_OFF);
            ask("  Enter para seguir... ");
            continue;
        }

        const fs::path& chosen = items[(size_t)n - 1];
        if (is_dir_safe(chosen)) dir = chosen;   // entro a la carpeta
        else return pstr(chosen);                // devuelvo el archivo elegido
    }
}

// ===========================================================================
// 7. CHEQUEO INTERNO
// ===========================================================================
// Un test minimo: si alguien toca las formulas de color y las rompe, esto lo
// detecta al instante en vez de que aparezcan colores raros por sorpresa.

static bool fail(const char* msg) {
    std::fprintf(stderr, "selftest FAIL: %s\n", msg);
    return false;
}

bool selftest() {
    int r, g, b;

    byte_color(0, PAL_HEAT, r, g, b);
    if (!(r == 0 && g == 0 && b == 255)) return fail("el byte 0 deberia ser azul puro");

    byte_color(255, PAL_HEAT, r, g, b);
    if (!(r == 254 && g == 0 && b == 0)) return fail("el byte 255 deberia ser rojo");

    byte_color('A', PAL_TEXT, r, g, b);
    if (!(g > r && g > b)) return fail("una letra en la paleta texto deberia salir verde");

    // Ninguna paleta puede devolver un valor fuera de 0-255: eso romperia el
    // codigo de color y ensuciaria toda la pantalla.
    for (int p = 0; p < PAL_COUNT; p++)
        for (int v = 0; v <= 255; v++) {
            byte_color((unsigned char)v, p, r, g, b);
            if (r < 0 || r > 255 || g < 0 || g > 255 || b < 0 || b > 255)
                return fail("un canal de color se fue de rango");
        }

    if (printable(31) || !printable(32) || !printable(126) || printable(127))
        return fail("los limites de printable() estan corridos");

    if (human_size(512) != "512 bytes" || human_size(2048) != "2.0 KB")
        return fail("human_size() no formatea bien");

    std::printf("selftest OK\n");
    return true;
}
