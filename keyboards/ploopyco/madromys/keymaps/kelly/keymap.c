/* Kelly's Ploopy Adept layout
 *
 * Button order in LAYOUT(): top-left-outer, top-left, top-right, top-right-outer, bottom-left, bottom-right
 *
 * RIGHT-HAND MODE (layer 0)
 *   Top-left-outer:  Back (tap) / Forward (double tap)
 *   Top-left:        App switcher (Ctrl+Alt+Tab)
 *   Top-right:       Middle click (tap) / Esc (double tap) / Zoom (hold + move ball)
 *   Top-right-outer: Right click (tap) / Scroll (hold + move ball)
 *   Bottom-left:     Left click
 *   Bottom-right:    DPI cycle (tap) / Switch hands (hold 1 second)
 *
 * LEFT-HAND MODE (layer 1) is the mirror image.
 * Hand choice is saved on the trackball, so it stays after unplugging.
 */

#include QMK_KEYBOARD_H

// Built into Ploopy's firmware: turns ball scrolling on/off, and cycles speed
extern bool is_drag_scroll;
void cycle_dpi(void);

// Our custom buttons
enum custom_keycodes {
    RCLK_SCRL = SAFE_RANGE, // tap = right click, hold = scroll
    MCLK_ZOOM,              // tap = middle click, double tap = Esc, hold = zoom
    DPI_HAND,               // tap = DPI cycle, hold 1 sec = switch hands
    BACK_FWD,               // tap = Back, double tap = Forward
};

#define APP_SW C(A(KC_TAB))

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    // Right-hand mode
    [0] = LAYOUT( BACK_FWD,  APP_SW,    MCLK_ZOOM, RCLK_SCRL, MS_BTN1,  DPI_HAND ),
    // Left-hand mode (mirror)
    [1] = LAYOUT( RCLK_SCRL, MCLK_ZOOM, APP_SW,    BACK_FWD,  DPI_HAND, MS_BTN1  ),
};

#define HOLD_TIME      200  // ms: shorter than this counts as a tap
#define SCROLL_SLOWDOWN 6.0f  // bigger number = slower scroll
#define ZOOM_SLOWDOWN   18.0f // bigger number = slower zoom
#define HAND_SWAP_TIME 1000 // ms: hold DPI button this long to switch hands

static uint16_t rclk_timer, mclk_timer, dpi_timer;
static bool     mclk_held = false, zoom_active = false;
static uint16_t mclk_tap_timer;
static bool     mclk_pending = false, mclk_second = false;
static bool     dpi_held = false, hand_swapped = false;
static uint16_t back_timer;
static bool     back_pending = false;

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    switch (keycode) {
        case BACK_FWD:
            if (record->event.pressed) {
                if (back_pending && timer_elapsed(back_timer) < HOLD_TIME) {
                    back_pending = false;
                    tap_code16(MS_BTN5); // second tap in time = Forward
                } else {
                    back_pending = true; // wait to see if a second tap comes
                    back_timer   = timer_read();
                }
            }
            return false;

        case RCLK_SCRL:
            if (record->event.pressed) {
                rclk_timer     = timer_read();
                is_drag_scroll = true; // scroll starts right away while held
            } else {
                is_drag_scroll = false;
                if (timer_elapsed(rclk_timer) < HOLD_TIME) {
                    tap_code16(MS_BTN2); // quick tap = right click
                }
            }
            return false;

        case MCLK_ZOOM:
            if (record->event.pressed) {
                mclk_timer = timer_read();
                mclk_held  = true;
                // pressed again soon after a tap = second tap
                mclk_second = mclk_pending && timer_elapsed(mclk_tap_timer) < HOLD_TIME;
            } else {
                mclk_held = false;
                if (zoom_active) {
                    zoom_active    = false;
                    is_drag_scroll = false;
                    unregister_code(KC_LCTL);
                } else if (mclk_second) {
                    mclk_second  = false;
                    mclk_pending = false;
                    tap_code(KC_ESC); // double tap = Esc
                } else {
                    mclk_pending   = true; // wait to see if a second tap comes
                    mclk_tap_timer = timer_read();
                }
            }
            return false;

        case DPI_HAND:
            if (record->event.pressed) {
                dpi_timer    = timer_read();
                dpi_held     = true;
                hand_swapped = false;
            } else {
                dpi_held = false;
                if (!hand_swapped) {
                    cycle_dpi(); // short press = next speed
                }
            }
            return false;
    }
    return true;
}

// Runs constantly in the background to detect long holds
void housekeeping_task_user(void) {
    // Back button: no second tap came in time, so send Back
    if (back_pending && timer_elapsed(back_timer) >= HOLD_TIME) {
        back_pending = false;
        tap_code16(MS_BTN4);
    }
    // Middle button: one tap and no second tap in time = middle click
    if (mclk_pending && !mclk_held && timer_elapsed(mclk_tap_timer) >= HOLD_TIME) {
        mclk_pending = false;
        tap_code16(MS_BTN3);
    }
    // Middle button held long enough: zoom = Ctrl + ball scroll
    if (mclk_held && !zoom_active && timer_elapsed(mclk_timer) >= HOLD_TIME) {
        mclk_pending = false;
        mclk_second  = false;
        zoom_active  = true;
        register_code(KC_LCTL);
        is_drag_scroll = true;
    }
    // DPI button held 1 second: switch hands and save the choice
    if (dpi_held && !hand_swapped && timer_elapsed(dpi_timer) >= HAND_SWAP_TIME) {
        hand_swapped = true;
        uint8_t next = (get_highest_layer(default_layer_state) == 0) ? 1 : 0;
        set_single_persistent_default_layer(next);
    }
}

// Slow down scroll and zoom: shrink ball movement while scrolling or zooming
static float scroll_acc_x = 0, scroll_acc_y = 0;
report_mouse_t pointing_device_task_user(report_mouse_t report) {
    if (is_drag_scroll) {
        float slow = zoom_active ? ZOOM_SLOWDOWN : SCROLL_SLOWDOWN;
        scroll_acc_x += report.x / slow;
        scroll_acc_y -= report.y / slow; // minus = flip up/down direction
        report.x = (int)scroll_acc_x;
        report.y = (int)scroll_acc_y;
        scroll_acc_x -= report.x;
        scroll_acc_y -= report.y;
    } else {
        scroll_acc_x = 0;
        scroll_acc_y = 0;
    }
    return report;
}
