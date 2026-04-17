// Python `stick` module — narrow binding surface exposed to .stickapp code.
//
// Compiled twice:
//   1. During the MicroPython embed regeneration (build_mpy_vm.sh) purely
//      so its QSTR references land in the generated qstr table.
//   2. As part of the Arduino library build, which is where it actually
//      executes. The implementations of stick_bind_* live in
//      stick_bindings.cpp alongside the library.
//
// Only add symbols that map to a concrete binding function — every name
// here is a forward-compatibility promise (api_version 1).

#include "py/obj.h"
#include "py/runtime.h"
#include "py/objtuple.h"

#include "stick_bindings.h"

// ============================================================
//   Helpers
// ============================================================

static uint16_t color_from_obj(mp_obj_t o) {
    return (uint16_t)mp_obj_get_int(o);
}

// ============================================================
//   Top-level: timing / exit
// ============================================================

static mp_obj_t stick_millis(void) {
    return mp_obj_new_int_from_uint(stick_bind_millis());
}
static MP_DEFINE_CONST_FUN_OBJ_0(stick_millis_obj, stick_millis);

static mp_obj_t stick_delay(mp_obj_t ms_in) {
    stick_bind_delay((uint32_t)mp_obj_get_int(ms_in));
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(stick_delay_obj, stick_delay);

static mp_obj_t stick_exit(void) {
    return mp_obj_new_bool(stick_bind_should_exit());
}
static MP_DEFINE_CONST_FUN_OBJ_0(stick_exit_obj, stick_exit);

// ============================================================
//   stick.display submodule
// ============================================================

static mp_obj_t sd_width(void)  { return mp_obj_new_int(stick_bind_display_width()); }
static mp_obj_t sd_height(void) { return mp_obj_new_int(stick_bind_display_height()); }
static MP_DEFINE_CONST_FUN_OBJ_0(sd_width_obj,  sd_width);
static MP_DEFINE_CONST_FUN_OBJ_0(sd_height_obj, sd_height);

static mp_obj_t sd_fill(mp_obj_t color) {
    stick_bind_display_fill(color_from_obj(color));
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(sd_fill_obj, sd_fill);

static mp_obj_t sd_rect(size_t n_args, const mp_obj_t *args) {
    stick_bind_display_rect(
        mp_obj_get_int(args[0]), mp_obj_get_int(args[1]),
        mp_obj_get_int(args[2]), mp_obj_get_int(args[3]),
        color_from_obj(args[4]));
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(sd_rect_obj, 5, 5, sd_rect);

static mp_obj_t sd_line(size_t n_args, const mp_obj_t *args) {
    stick_bind_display_line(
        mp_obj_get_int(args[0]), mp_obj_get_int(args[1]),
        mp_obj_get_int(args[2]), mp_obj_get_int(args[3]),
        color_from_obj(args[4]));
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(sd_line_obj, 5, 5, sd_line);

static mp_obj_t sd_pixel(mp_obj_t x, mp_obj_t y, mp_obj_t color) {
    stick_bind_display_pixel(mp_obj_get_int(x), mp_obj_get_int(y),
                             color_from_obj(color));
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_3(sd_pixel_obj, sd_pixel);

static mp_obj_t sd_text(size_t n_args, const mp_obj_t *args) {
    const char *s = mp_obj_str_get_str(args[0]);
    stick_bind_display_text(s,
        mp_obj_get_int(args[1]), mp_obj_get_int(args[2]),
        color_from_obj(args[3]));
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(sd_text_obj, 4, 4, sd_text);

static mp_obj_t sd_text2(size_t n_args, const mp_obj_t *args) {
    const char *s = mp_obj_str_get_str(args[0]);
    stick_bind_display_text2(s,
        mp_obj_get_int(args[1]), mp_obj_get_int(args[2]),
        color_from_obj(args[3]));
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(sd_text2_obj, 4, 4, sd_text2);

static const mp_rom_map_elem_t sd_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_display) },
    { MP_ROM_QSTR(MP_QSTR_width),    MP_ROM_PTR(&sd_width_obj) },
    { MP_ROM_QSTR(MP_QSTR_height),   MP_ROM_PTR(&sd_height_obj) },
    { MP_ROM_QSTR(MP_QSTR_fill),     MP_ROM_PTR(&sd_fill_obj) },
    { MP_ROM_QSTR(MP_QSTR_rect),     MP_ROM_PTR(&sd_rect_obj) },
    { MP_ROM_QSTR(MP_QSTR_line),     MP_ROM_PTR(&sd_line_obj) },
    { MP_ROM_QSTR(MP_QSTR_pixel),    MP_ROM_PTR(&sd_pixel_obj) },
    { MP_ROM_QSTR(MP_QSTR_text),     MP_ROM_PTR(&sd_text_obj) },
    { MP_ROM_QSTR(MP_QSTR_text2),    MP_ROM_PTR(&sd_text2_obj) },
};
static MP_DEFINE_CONST_DICT(sd_globals, sd_globals_table);
static const mp_obj_module_t sd_module = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&sd_globals,
};

// ============================================================
//   stick.buttons submodule
// ============================================================

static mp_obj_t sb_update(void)    { stick_bind_buttons_update(); return mp_const_none; }
static mp_obj_t sb_a_pressed(void) { return mp_obj_new_bool(stick_bind_buttons_a_pressed()); }
static mp_obj_t sb_b_pressed(void) { return mp_obj_new_bool(stick_bind_buttons_b_pressed()); }
static MP_DEFINE_CONST_FUN_OBJ_0(sb_update_obj,    sb_update);
static MP_DEFINE_CONST_FUN_OBJ_0(sb_a_pressed_obj, sb_a_pressed);
static MP_DEFINE_CONST_FUN_OBJ_0(sb_b_pressed_obj, sb_b_pressed);

static const mp_rom_map_elem_t sb_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__),  MP_ROM_QSTR(MP_QSTR_buttons) },
    { MP_ROM_QSTR(MP_QSTR_update),    MP_ROM_PTR(&sb_update_obj) },
    { MP_ROM_QSTR(MP_QSTR_a_pressed), MP_ROM_PTR(&sb_a_pressed_obj) },
    { MP_ROM_QSTR(MP_QSTR_b_pressed), MP_ROM_PTR(&sb_b_pressed_obj) },
};
static MP_DEFINE_CONST_DICT(sb_globals, sb_globals_table);
static const mp_obj_module_t sb_module = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&sb_globals,
};

// ============================================================
//   stick.imu submodule
// ============================================================

static mp_obj_t si_accel(void) {
    float x, y, z;
    stick_bind_imu_accel(&x, &y, &z);
    mp_obj_t items[3] = {
        mp_obj_new_float(x), mp_obj_new_float(y), mp_obj_new_float(z),
    };
    return mp_obj_new_tuple(3, items);
}
static mp_obj_t si_gyro(void) {
    float x, y, z;
    stick_bind_imu_gyro(&x, &y, &z);
    mp_obj_t items[3] = {
        mp_obj_new_float(x), mp_obj_new_float(y), mp_obj_new_float(z),
    };
    return mp_obj_new_tuple(3, items);
}
static MP_DEFINE_CONST_FUN_OBJ_0(si_accel_obj, si_accel);
static MP_DEFINE_CONST_FUN_OBJ_0(si_gyro_obj,  si_gyro);

static const mp_rom_map_elem_t si_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_imu) },
    { MP_ROM_QSTR(MP_QSTR_accel),    MP_ROM_PTR(&si_accel_obj) },
    { MP_ROM_QSTR(MP_QSTR_gyro),     MP_ROM_PTR(&si_gyro_obj) },
};
static MP_DEFINE_CONST_DICT(si_globals, si_globals_table);
static const mp_obj_module_t si_module = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&si_globals,
};

// ============================================================
//   stick.store submodule
// ============================================================

#include <string.h>

static mp_obj_t ss_get(size_t n_args, const mp_obj_t *args) {
    const char *key = mp_obj_str_get_str(args[0]);
    const char *def = (n_args >= 2) ? mp_obj_str_get_str(args[1]) : "";
    char buf[128];
    stick_bind_store_get(key, buf, sizeof(buf), def);
    return mp_obj_new_str(buf, strlen(buf));
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(ss_get_obj, 1, 2, ss_get);

static mp_obj_t ss_put(mp_obj_t key, mp_obj_t value) {
    const char *k = mp_obj_str_get_str(key);
    const char *v = mp_obj_str_get_str(value);
    return mp_obj_new_bool(stick_bind_store_put(k, v));
}
static MP_DEFINE_CONST_FUN_OBJ_2(ss_put_obj, ss_put);

static const mp_rom_map_elem_t ss_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_store) },
    { MP_ROM_QSTR(MP_QSTR_get),      MP_ROM_PTR(&ss_get_obj) },
    { MP_ROM_QSTR(MP_QSTR_put),      MP_ROM_PTR(&ss_put_obj) },
};
static MP_DEFINE_CONST_DICT(ss_globals, ss_globals_table);
static const mp_obj_module_t ss_module = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&ss_globals,
};

// ============================================================
//   Top-level stick module
// ============================================================

static const mp_rom_map_elem_t stick_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_stick) },

    { MP_ROM_QSTR(MP_QSTR_millis),   MP_ROM_PTR(&stick_millis_obj) },
    { MP_ROM_QSTR(MP_QSTR_delay),    MP_ROM_PTR(&stick_delay_obj) },
    { MP_ROM_QSTR(MP_QSTR_exit),     MP_ROM_PTR(&stick_exit_obj) },

    { MP_ROM_QSTR(MP_QSTR_display),  MP_ROM_PTR(&sd_module) },
    { MP_ROM_QSTR(MP_QSTR_buttons),  MP_ROM_PTR(&sb_module) },
    { MP_ROM_QSTR(MP_QSTR_imu),      MP_ROM_PTR(&si_module) },
    { MP_ROM_QSTR(MP_QSTR_store),    MP_ROM_PTR(&ss_module) },

    // RGB565 color constants — the 7 colors the plan specified.
    { MP_ROM_QSTR(MP_QSTR_BLACK),    MP_ROM_INT(0x0000) },
    { MP_ROM_QSTR(MP_QSTR_WHITE),    MP_ROM_INT(0xFFFF) },
    { MP_ROM_QSTR(MP_QSTR_RED),      MP_ROM_INT(0xF800) },
    { MP_ROM_QSTR(MP_QSTR_GREEN),    MP_ROM_INT(0x07E0) },
    { MP_ROM_QSTR(MP_QSTR_BLUE),     MP_ROM_INT(0x001F) },
    { MP_ROM_QSTR(MP_QSTR_YELLOW),   MP_ROM_INT(0xFFE0) },
    { MP_ROM_QSTR(MP_QSTR_CYAN),     MP_ROM_INT(0x07FF) },
};
static MP_DEFINE_CONST_DICT(stick_module_globals, stick_module_globals_table);

const mp_obj_module_t stick_user_cmodule = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&stick_module_globals,
};

MP_REGISTER_MODULE(MP_QSTR_stick, stick_user_cmodule);
