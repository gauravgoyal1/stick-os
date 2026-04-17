# Tilt — bubble-level / spirit level using the IMU.
# Controls: A zeros the reference. PWR exits.
# Target: stick api_version 1.

import stick

# Bubble rendered as a small filled diamond (stick API has no circle primitive).
# The "frame" is a square outline with crosshairs; the bubble moves in proportion
# to tilt measured in g on the X/Y accel axes.

BUBBLE_R = 6                  # half-size of the bubble diamond
FRAME_R = 48                  # half-size of the frame square
LEVEL_THRESHOLD = 0.03        # within ~1.7 deg considered level (green)
WARN_THRESHOLD  = 0.17        # within ~10 deg = yellow; outside = red


def draw_diamond(cx, cy, r, color):
    # Filled diamond by stacking shrinking horizontal bars.
    for dy in range(-r, r + 1):
        w = (r - abs(dy)) * 2 + 1
        stick.display.rect(cx - w // 2, cy + dy, w, 1, color)


def draw_frame(cx, cy):
    # Frame outline and crosshairs. Redrawn after every bubble move so
    # the bubble's trailing erase doesn't chip the frame.
    stick.display.rect(cx - FRAME_R, cy - FRAME_R, FRAME_R * 2, 1, stick.WHITE)
    stick.display.rect(cx - FRAME_R, cy + FRAME_R, FRAME_R * 2, 1, stick.WHITE)
    stick.display.rect(cx - FRAME_R, cy - FRAME_R, 1, FRAME_R * 2, stick.WHITE)
    stick.display.rect(cx + FRAME_R, cy - FRAME_R, 1, FRAME_R * 2, stick.WHITE)
    stick.display.line(cx - FRAME_R, cy, cx + FRAME_R, cy, stick.RED)
    stick.display.line(cx, cy - FRAME_R, cx, cy + FRAME_R, stick.RED)


def tilt_color(ax, ay):
    mag = (ax * ax + ay * ay) ** 0.5
    if mag < LEVEL_THRESHOLD:
        return stick.GREEN
    if mag < WARN_THRESHOLD:
        return stick.YELLOW
    return stick.RED


def main():
    w = stick.display.width()
    h = stick.display.height()
    cx = w // 2
    cy = h // 2 - 10

    # Zero offsets — A calibrates to current orientation.
    zx = float(stick.store.get("zx", "0"))
    zy = float(stick.store.get("zy", "0"))

    last_bx, last_by = cx, cy
    last_color = stick.GREEN
    last_deg_text = ""

    stick.display.fill(stick.BLACK)
    draw_frame(cx, cy)
    stick.display.text("A: zero   PWR: back", 4, h - 14, stick.CYAN)

    while not stick.exit():
        stick.buttons.update()

        ax, ay, az = stick.imu.accel()
        cal_x = ax - zx
        cal_y = ay - zy

        if stick.buttons.a_pressed():
            zx = ax
            zy = ay
            stick.store.put("zx", str(zx))
            stick.store.put("zy", str(zy))
            cal_x = 0.0
            cal_y = 0.0

        # Bubble position: tilt left -> bubble slides right. Clamp to frame.
        bx = cx + int(cal_x * FRAME_R * 2)
        by = cy - int(cal_y * FRAME_R * 2)
        if bx < cx - FRAME_R + BUBBLE_R: bx = cx - FRAME_R + BUBBLE_R
        if bx > cx + FRAME_R - BUBBLE_R: bx = cx + FRAME_R - BUBBLE_R
        if by < cy - FRAME_R + BUBBLE_R: by = cy - FRAME_R + BUBBLE_R
        if by > cy + FRAME_R - BUBBLE_R: by = cy + FRAME_R - BUBBLE_R

        color = tilt_color(cal_x, cal_y)

        # Erase old bubble, restore any frame pixels it chipped, then
        # draw the new bubble.
        if (bx, by, color) != (last_bx, last_by, last_color):
            draw_diamond(last_bx, last_by, BUBBLE_R, stick.BLACK)
            draw_frame(cx, cy)
            draw_diamond(bx, by, BUBBLE_R, color)
            last_bx, last_by, last_color = bx, by, color

        # Angle readout in degrees (approx: asin(mag) * 180/pi).
        # No str.format in the minimal embed build — format one decimal
        # manually.
        mag = (cal_x * cal_x + cal_y * cal_y) ** 0.5
        if mag > 1.0:
            mag = 1.0
        deg10 = int(mag * 572.958)
        deg_text = str(deg10 // 10) + "." + str(deg10 % 10) + " deg"
        if deg_text != last_deg_text:
            stick.display.rect(cx - 40, cy + FRAME_R + 8, 80, 12, stick.BLACK)
            stick.display.text(deg_text, cx - 28, cy + FRAME_R + 10, stick.WHITE)
            last_deg_text = deg_text

        stick.delay(40)


main()
