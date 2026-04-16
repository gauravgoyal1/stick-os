# Snake — classic grid snake for Stick OS.
# Controls: A turns right, B turns left. PWR exits.
# Target: stick api_version 1.

import stick

# 9-col x 14-row grid at 15px cells fits 135x222 content area with 12px footer.
CELL = 15
COLS = 9
ROWS = 14
GRID_Y = 0  # OS content rect already excludes the status strip.

# Directions: (dx, dy) in grid cells. Order matters for turn math.
DIRS = [(1, 0), (0, 1), (-1, 0), (0, -1)]  # right, down, left, up


def draw_cell(col, row, color):
    x = col * CELL
    y = GRID_Y + row * CELL
    stick.display.rect(x + 1, y + 1, CELL - 2, CELL - 2, color)


def spawn_food(snake):
    # Pick a random empty cell. With a 9x14 grid and small snake this
    # almost always finds one on the first try.
    import urandom
    while True:
        c = urandom.getrandbits(4) % COLS
        r = urandom.getrandbits(4) % ROWS
        if (c, r) not in snake:
            return (c, r)


def draw_score(score, best):
    # Footer strip below the grid.
    fy = GRID_Y + ROWS * CELL + 2
    stick.display.rect(0, fy, stick.display.width(), 12, stick.BLACK)
    stick.display.text("S:" + str(score) + "  B:" + str(best), 4, fy + 2, stick.WHITE)


def game_over_screen(score, best):
    w = stick.display.width()
    h = stick.display.height()
    stick.display.fill(stick.BLACK)
    stick.display.text2("GAME", w // 2 - 24, 60, stick.RED)
    stick.display.text2("OVER", w // 2 - 24, 84, stick.RED)
    stick.display.text("Score: " + str(score), 30, 120, stick.WHITE)
    stick.display.text("Best:  " + str(best), 30, 136, stick.YELLOW)
    stick.display.text("A: restart", 30, 170, stick.CYAN)
    stick.display.text("PWR: exit",  30, 184, stick.CYAN)


def play_round(best):
    stick.display.fill(stick.BLACK)

    snake = [(4, 7), (3, 7), (2, 7)]
    dir_idx = 0  # moving right
    food = spawn_food(snake)
    score = 0
    step_ms = 180  # speeds up as snake grows

    # Initial draw.
    for seg in snake:
        draw_cell(seg[0], seg[1], stick.GREEN)
    draw_cell(food[0], food[1], stick.RED)
    draw_score(score, best)

    next_tick = stick.millis() + step_ms
    while not stick.exit():
        stick.buttons.update()
        if stick.buttons.a_pressed():
            dir_idx = (dir_idx + 1) % 4   # turn right
        elif stick.buttons.b_pressed():
            dir_idx = (dir_idx + 3) % 4   # turn left

        now = stick.millis()
        if now < next_tick:
            stick.delay(10)
            continue
        next_tick = now + step_ms

        dx, dy = DIRS[dir_idx]
        head = (snake[0][0] + dx, snake[0][1] + dy)

        # Wall collision.
        if head[0] < 0 or head[0] >= COLS or head[1] < 0 or head[1] >= ROWS:
            return score
        # Self collision.
        if head in snake:
            return score

        snake.insert(0, head)
        draw_cell(head[0], head[1], stick.GREEN)

        if head == food:
            score += 1
            if step_ms > 70:
                step_ms -= 6
            food = spawn_food(snake)
            draw_cell(food[0], food[1], stick.RED)
            draw_score(score, max(score, best))
        else:
            tail = snake.pop()
            draw_cell(tail[0], tail[1], stick.BLACK)

    return score


def main():
    best = int(stick.store.get("best", "0"))
    while not stick.exit():
        score = play_round(best)
        if stick.exit():
            return
        if score > best:
            best = score
            stick.store.put("best", str(best))

        game_over_screen(score, best)
        # Wait for A (restart) or PWR (exit).
        while not stick.exit():
            stick.buttons.update()
            if stick.buttons.a_pressed():
                break
            stick.delay(30)


main()
