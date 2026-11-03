/* Sprites straight from app/images, so the simulator always shows the art in
 * the repository. CMake generates the list (icons_list.c). */

#include "sim.h"

#include "png.h"

extern Icon* const sim_icon_list[];

bool sim_icons_load(void) {
    bool ok = true;
    for(size_t i = 0; sim_icon_list[i]; i++) {
        Icon* icon = sim_icon_list[i];
        int w, h;
        uint8_t* gray = png_read_gray(icon->file, &w, &h);
        if(!gray) {
            fprintf(stderr, "can't read %s\n", icon->file);
            ok = false;
            continue;
        }
        icon->width = (uint16_t)w;
        icon->height = (uint16_t)h;
        icon->pixels = malloc((size_t)w * h);
        /* The sprites are 1-bit: black is ink. */
        for(int p = 0; p < w * h; p++) icon->pixels[p] = gray[p] < 128;
        free(gray);
    }
    return ok;
}
