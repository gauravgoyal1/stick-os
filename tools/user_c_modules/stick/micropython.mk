# Included by MicroPython's py.mk during the embed-port build.
# Adds mod_stick.c to SRC_QSTR so QSTR extraction picks up the `stick`
# module's names (stick, millis, delay, exit, ...) for the generated
# qstrdefs table.

STICK_MOD_DIR := $(USERMOD_DIR)

SRC_USERMOD_C += $(STICK_MOD_DIR)/mod_stick.c
CFLAGS_USERMOD += -I$(STICK_MOD_DIR)
