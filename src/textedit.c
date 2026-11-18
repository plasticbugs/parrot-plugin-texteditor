// textedit (TEXTEDIT.CVP): a plain text editor for the ColecoVision. Text is
// typed on a USB keyboard (through a powered hub), shown in 40 columns with
// word wrap, and kept as .txt files on the card.
//
// The editor is the SDK's cvedit (../sdk/cvedit.h): the keys, the wrap, the
// files and the save that survives a power cut are all there. This file is
// the plugin around it: the console's screen and controller, the keyboard,
// the card, and the file the menu opened it with.
//
// RESET goes back to the menu at once, as for every plugin: the kernel gives
// the plugin no moment first, so text that wasn't saved is lost. The menu's
// bottom line says so when it happens.
#include <string.h>

#include "pico/stdlib.h"

#include "cvedit.h"
#include "cvinput.h"
#include "cvkernel.h"
#include "cvscreen.h"
#include "cvsd.h"
#include "cvusb.h"
#include "handoff.h"            // made by the build
#include "rom_image.h"          // made by the build

#define TEXT_MAX (127 * 1024)   // the biggest file: with its spare copy's 1K header, the 256 clusters fsro makes at a time
#define STICK_WAIT_US 400000    // the stick held: it repeats
#define STICK_US      80000

static cvs_t screen;
static uint8_t list[CVS_LIST_MAX];
static cvin_t player;
static cvedit_t ed;
static uint8_t text[TEXT_MAX];

// ---- the card: mounted when the editor wants it, again after it was out ----------------
static fs_t fs;
static bool mounted;

static fs_t *card(void *ctx, bool failed) {
    (void)ctx;
    if (failed || !cvsd_card_in()) mounted = false;
    if (!cvsd_card_in()) return NULL;
    if (!mounted) {
        if (!cvsd_init() || fs_mount(&fs, cvsd_read) != FS_OK) return NULL;
        fs_set_writer(&fs, cvsd_write);
        mounted = true;
    }
    return &fs;
}

static void draw(canvas_t *c, bool all, void *ctx) {
    (void)ctx;
    cvedit_draw(&ed, c, all);
}

int main(void) {
    static const cvk_config_t kernel = {rom_image, addr_line, data_bit, handoff_bin, HANDOFF_BIN_LEN};
    static const cvedit_config_t cfg = {
        .text = text, .cap = sizeof text,
        .x = 8, .y = 0, .cols = 40, .rows = 24,         // x 8 to 247: inside a TV's edges
        .ext = ".txt", .home = "/TEXT", .stem = "NOTE",
        .keys = "1 Save  2 Open  3 New",
        .card = card,
    };
    cvk_start(&kernel);
    cvs_init(&screen);
    cvin_init(&player);
    cvedit_init(&ed, &cfg);
    // the card first (its start is up to 1.5 s), then USB
    const cvl_t *launch = cvk_launch();
    if (launch && launch->path[0]) {                    // the menu opened a file in it
        fs_file_t f;
        cvsd_launched_file(launch, &f);
        cvedit_load(&ed, launch->path, &f);
    } else {
        cvedit_check(&ed, NULL);                        // a save cut short last time?
    }
    cvusb_start();

    uint32_t t_frame = 0, t_stick = 0, t_start = time_us_32();
    uint8_t stick = 0;
    bool stick_repeats = false, hinted = false, unsaved = false;
    for (;;) {
        const uint32_t t = time_us_32();
        cvk_task(t);
        cvusb_task();
        int kind;
        uint8_t bits;
        while (cvk_report(t, &kind, &bits)) {
            if (kind == REPORT_KIND_HELLO || kind == REPORT_KIND_ACK)
                cvs_report(&screen, kind == REPORT_KIND_HELLO, bits & 31, t);
            else
                cvin_report(&player, kind, bits);
        }

        cvkey_t k;
        while (cvusb_key(&k)) {
            hinted = true;
            cvedit_key(&ed, &k);
        }
        cvedit_task(&ed, t);
        if (mounted && !cvsd_card_in()) mounted = false;

        if (t - t_frame >= 16667) {                     // a frame: the console's controller
            t_frame = t;
            cvin_update(&player);
            uint8_t pressed = cvin_pressed(&player);
            const uint8_t dirs = player.held & (CVIN_UP | CVIN_DOWN | CVIN_LEFT | CVIN_RIGHT);
            if (dirs != stick) {
                stick = dirs;
                t_stick = t;
                stick_repeats = false;
            } else if (dirs && t - t_stick >= (stick_repeats ? STICK_US : STICK_WAIT_US)) {
                t_stick = t;
                stick_repeats = true;
                pressed |= dirs;
            }
            cvedit_pad(&ed, pressed, cvin_key(&player));

            // no keyboard after a few seconds: say where it goes
            if (!hinted && t - t_start > 4000000) {
                hinted = true;
                cvusb_status_t st;
                cvusb_status(&st);
                if (!st.keyboards && cvedit_editing(&ed)) cvedit_message(&ed, "No keyboard: plug one into a USB hub", false);
            }
            // what the menu says if RESET is pressed now
            if (cvedit_modified(&ed) != unsaved) {
                unsaved = !unsaved;
                cvk_note(unsaved ? "Text Editor: text wasn't saved" : NULL);
            }
        }

        const size_t len = cvs_update(&screen, draw, NULL, list, sizeof list, t);
        if (len) cvk_list(list, len, screen.out_full, screen.seq);
    }
}
