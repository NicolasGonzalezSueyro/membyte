// ===========================================================================
// main.cpp - el menu: la unica parte que habla con el usuario
// ===========================================================================
// Este archivo no sabe nada de colores ni de bytes. Solo pregunta que quiere
// hacer el usuario y llama a las funciones declaradas en membyte.h. Toda la
// logica esta del otro lado; aca solo esta la conversacion.
// ===========================================================================

#include "membyte.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

// Recorta una ruta larga para que no rompa el ancho del menu.
static std::string short_path(const std::string& p, size_t max_len = 44) {
    if (p.size() <= max_len) return p;
    return "..." + p.substr(p.size() - (max_len - 3));
}

static void print_menu(const std::string& path, size_t file_size,
                       int bytes_per_row, bool show_text, int palette) {
    clear_screen();
    std::printf("\n" C_TITLE
        "  +-------------------------------------------------------+\n"
        "  |            m e m b y t e                              |\n"
        "  |        explorador visual de bytes                     |\n"
        "  +-------------------------------------------------------+" C_OFF "\n\n");

    if (path.empty()) {
        std::printf("   archivo:  " C_WARN "(ninguno elegido todavia - empeza por la opcion 1)" C_OFF "\n");
    } else {
        std::printf("   archivo:  " C_OK "%s" C_OFF "\n", short_path(path).c_str());
        std::printf("   tamanio:  %s\n", human_size(file_size).c_str());
    }
    std::printf("   paleta:   %s\n", palette_name(palette));
    std::printf("   detalle:  %d bytes por fila, columna de texto %s\n",
                bytes_per_row, show_text ? C_OK "ON" C_OFF : C_DIM "OFF" C_OFF);

    std::printf("\n" C_DIM "  ------------------------ VER ------------------------" C_OFF "\n");
    std::printf("   " C_KEY "1" C_OFF ")  elegir archivo   " C_DIM "(explorador de carpetas)" C_OFF "\n");
    std::printf("   " C_KEY "2" C_OFF ")  vista PANORAMICA " C_DIM "(todo el archivo en 1 pantalla)" C_OFF "\n");
    std::printf("   " C_KEY "3" C_OFF ")  vista DETALLE    " C_DIM "(byte por byte, de a paginas)" C_OFF "\n");
    std::printf("   " C_KEY "4" C_OFF ")  estadisticas     " C_DIM "(que tipo de archivo es)" C_OFF "\n");

    std::printf("\n" C_DIM "  --------------------- AJUSTES -----------------------" C_OFF "\n");
    std::printf("   " C_KEY "5" C_OFF ")  cambiar paleta de colores\n");
    std::printf("   " C_KEY "6" C_OFF ")  prender/apagar la columna de texto\n");
    std::printf("   " C_KEY "7" C_OFF ")  cambiar cuantos bytes por fila\n");

    std::printf("\n" C_DIM "  ----------------------- AYUDA -----------------------" C_OFF "\n");
    std::printf("   " C_KEY "8" C_OFF ")  que es todo esto?  " C_DIM "(explicacion desde cero)" C_OFF "\n");
    std::printf("   " C_KEY "0" C_OFF ")  salir\n");
}

// El programa de verdad. main() lo llama envuelto en un try/catch (abajo).
static int run(int argc, char** argv) {
    // Pedirle a Windows que muestre colores. Va primero que todo.
    enable_ansi_on_windows();

    // Modo test: "membyte.exe --selftest" chequea que los colores esten bien.
    if (argc >= 2 && std::strcmp(argv[1], "--selftest") == 0)
        return selftest() ? 0 : 1;

    // --- Estado de la sesion: lo que el usuario va cambiando desde el menu ---
    std::string path = argc >= 2 ? argv[1] : "";   // archivo abierto ahora
    int bytes_per_row = 16;    // 16 deja la columna de texto bien legible
    bool show_text = true;
    int palette = PAL_HEAT;
    std::vector<unsigned char> data;

    // Si arrancaste el programa pasandole un archivo, lo abre solo.
    if (!path.empty() && !read_file(path, data)) path.clear();

    // Lo primero que ve alguien que abre el programa por primera vez es la
    // explicacion, no un menu con palabras que no entiende.
    print_explainer();
    ask(C_KEY "Enter" C_OFF " para entrar al programa... ");

    while (true) {
        print_menu(path, data.size(), bytes_per_row, show_text, palette);
        std::string op = ask("\n   opcion> ");

        if (op == "0" || op == "q") break;

        // Las vistas 2, 3 y 4 necesitan un archivo abierto.
        bool needs_file = (op == "2" || op == "3" || op == "4");
        if (needs_file && path.empty()) {
            std::printf(C_WARN "\n   Primero elegi un archivo con la opcion 1.\n" C_OFF);
            ask("   Enter para seguir... ");
            continue;
        }

        if (op == "1") {
            // Arranca el explorador en la carpeta del archivo actual, o en la
            // carpeta donde esta el programa si todavia no elegiste nada.
            // El try/catch es una red de seguridad: navegar el disco toca
            // carpetas del sistema, unidades desconectadas y accesos directos
            // rotos. Sin esto, cualquiera de esas cosas cerraria el programa
            // de golpe y el usuario no llegaria ni a leer el error.
            try {
                std::string start = path.empty() ? current_dir() : parent_dir(path);
                std::string chosen = browse(start);
                if (!chosen.empty() && read_file(chosen, data)) path = chosen;
            } catch (const std::exception& e) {
                std::printf(C_WARN "\n   Hubo un problema navegando: %s\n" C_OFF, e.what());
                ask("   Enter para volver al menu... ");
            }

        } else if (op == "2") {
            render_overview(data, palette);

        } else if (op == "3") {
            // Aviso antes de entrar: en detalle, un archivo grande son miles
            // de paginas. La panoramica suele ser lo que la persona quiere.
            size_t rows = (data.size() + bytes_per_row - 1) / bytes_per_row;
            if (rows > 2000) {
                std::printf(C_WARN "\n   Ojo: este archivo son %zu filas en modo detalle.\n" C_OFF, rows);
                std::printf("   Para ver la forma general conviene la vista panoramica (opcion 2).\n");
                std::string go = ask("   Seguir igual? (s/n) > ");
                if (go != "s" && go != "S") continue;
            }
            render_paged(data, bytes_per_row, show_text, palette);

        } else if (op == "4") {
            print_stats(data);
            ask("   Enter para volver al menu... ");

        } else if (op == "5") {
            palette = (palette + 1) % PAL_COUNT;   // rota entre las paletas

        } else if (op == "6") {
            show_text = !show_text;

        } else if (op == "7") {
            std::string v = ask("\n   bytes por fila (8 a 128; 16 con texto, 64 sin texto): ");
            int n = std::atoi(v.c_str());
            if (n >= 8 && n <= 128) bytes_per_row = n;
            else {
                std::printf(C_WARN "   Valor invalido, queda en %d.\n" C_OFF, bytes_per_row);
                ask("   Enter para seguir... ");
            }

        } else if (op == "8") {
            print_explainer();
            ask(C_KEY "Enter" C_OFF " para volver al menu... ");

        } else {
            std::printf(C_WARN "\n   Opcion invalida.\n" C_OFF);
            ask("   Enter para seguir... ");
        }
    }

    // Si abriste el .exe con doble click, sin esto la ventana se cerraria de
    // golpe y no llegarias a leer nada.
    std::printf("\n   Chau!\n");
    ask("   Enter para cerrar... ");
    return 0;
}

// Envoltorio de ultima instancia. Si algo revienta en cualquier parte del
// programa, el usuario ve QUE paso en vez de que la ventana desaparezca.
int main(int argc, char** argv) {
    try {
        return run(argc, argv);
    } catch (const std::exception& e) {
        std::printf(C_WARN "\n   Error inesperado: %s\n" C_OFF, e.what());
    } catch (...) {
        std::printf(C_WARN "\n   Error inesperado (desconocido).\n" C_OFF);
    }
    ask("   Enter para cerrar... ");
    return 1;
}
