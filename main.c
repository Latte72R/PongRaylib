#include "raylib.h"
#include <math.h>
#include <stdbool.h>
#include <stdio.h>

#define SCREEN_W 1000
#define SCREEN_H 800

#define PLAY_X 70
#define PLAY_Y 90
#define PLAY_W 860
#define PLAY_H 640

#define BRICK_ROWS 8
#define BRICK_COLS 12
#define BRICK_GAP 6

#define MAX_BRICKS (BRICK_ROWS * BRICK_COLS)
#define MAX_BALLS 4
#define MAX_POWERUPS 6
#define MAX_PARTICLES 220
#define STAR_COUNT 80

#define BASE_PADDLE_W 120.0f
#define PADDLE_H 16.0f
#define PADDLE_SPEED 520.0f
#define BALL_RADIUS 8.0f
#define BALL_BASE_SPEED 430.0f

typedef enum {
  STATE_MENU = 0,
  STATE_PLAY,
  STATE_PAUSE,
  STATE_CLEAR,
  STATE_OVER
} GameState;

typedef enum {
  POWER_EXTEND = 0,
  POWER_MULTIBALL,
  POWER_SLOW,
  POWER_LIFE,
  POWER_FAST,
  POWER_DEATH
} PowerType;

typedef struct {
  Vector2 pos;
  Vector2 vel;
  float radius;
  bool active;
  bool stuck;
} Ball;

typedef struct {
  Rectangle rect;
  int hp;
  int max_hp;
  bool alive;
  bool solid;
  bool power_brick;
  PowerType power_type;
} Brick;

typedef struct {
  Vector2 pos;
  Vector2 vel;
  float radius;
  PowerType type;
  bool active;
} Powerup;

typedef struct {
  Vector2 pos;
  Vector2 vel;
  float life;
  Color color;
  bool active;
} Particle;

typedef struct {
  Vector2 pos;
  float radius;
  float twinkle;
} Star;

static float ClampFloat(float v, float min, float max) {
  if (v < min)
    return min;
  if (v > max)
    return max;
  return v;
}

static Vector2 NormalizeSafe(Vector2 v) {
  float len = sqrtf(v.x * v.x + v.y * v.y);
  if (len <= 0.0001f) {
    return (Vector2){0.0f, -1.0f};
  }
  return (Vector2){v.x / len, v.y / len};
}

static float LevelSpeedMult(int level) {
  if (level <= 1)
    return 0.85f;
  if (level == 2)
    return 0.95f;
  return 1.05f;
}

static float SpeedItemMult(int speed_state) {
  if (speed_state < 0)
    return 0.7f;
  if (speed_state > 0)
    return 1.35f;
  return 1.0f;
}

static void DrawTextFont(Font font, const char *text, int x, int y, int size,
                         Color color) {
  DrawTextEx(font, text, (Vector2){(float)x, (float)y}, (float)size, 1.0f,
             color);
}

static void DrawCenteredText(Font font, const char *text, int center_x, int y,
                             int size, Color color) {
  Vector2 dim = MeasureTextEx(font, text, (float)size, 1.0f);
  DrawTextEx(font, text, (Vector2){(float)center_x - dim.x * 0.5f, (float)y},
             (float)size, 1.0f, color);
}

static void ResetBalls(Ball balls[MAX_BALLS], Rectangle paddle) {
  for (int i = 0; i < MAX_BALLS; i++) {
    balls[i].active = false;
    balls[i].stuck = false;
    balls[i].radius = BALL_RADIUS;
  }
  balls[0].active = true;
  balls[0].stuck = true;
  balls[0].pos =
      (Vector2){paddle.x + paddle.width * 0.5f, paddle.y - BALL_RADIUS - 2.0f};
  balls[0].vel = (Vector2){0.0f, -1.0f};
}

static void LaunchBall(Ball *ball) {
  float angle = GetRandomValue(40, 140) * DEG2RAD;
  ball->vel = (Vector2){cosf(angle), -sinf(angle)};
  ball->stuck = false;
}

static Color BrickColor(const Brick *brick) {
  if (brick->solid)
    return (Color){90, 90, 110, 255};
  if (brick->power_brick) {
    if (brick->power_type == POWER_MULTIBALL)
      return (Color){100, 181, 246, 255};
    if (brick->power_type == POWER_EXTEND)
      return (Color){129, 199, 132, 255};
    if (brick->power_type == POWER_SLOW)
      return (Color){255, 213, 79, 255};
    if (brick->power_type == POWER_LIFE)
      return (Color){244, 143, 177, 255};
    if (brick->power_type == POWER_FAST)
      return (Color){255, 167, 38, 255};
    return (Color){239, 83, 80, 255};
  }
  return (Color){245, 245, 245, 255};
}

static void SpawnParticles(Particle particles[MAX_PARTICLES], Vector2 pos,
                           Color color) {
  int spawned = 0;
  for (int i = 0; i < MAX_PARTICLES; i++) {
    if (!particles[i].active) {
      particles[i].active = true;
      particles[i].pos = pos;
      particles[i].life = 0.7f + (float)GetRandomValue(0, 30) / 100.0f;
      float speed = 80.0f + (float)GetRandomValue(0, 140);
      float ang = (float)GetRandomValue(0, 360) * DEG2RAD;
      particles[i].vel = (Vector2){cosf(ang) * speed, sinf(ang) * speed};
      particles[i].color = color;
      spawned++;
      if (spawned >= 14)
        break;
    }
  }
}

static void SpawnPowerup(Powerup powerups[MAX_POWERUPS], Vector2 pos,
                         PowerType type) {
  for (int i = 0; i < MAX_POWERUPS; i++) {
    if (!powerups[i].active) {
      powerups[i].active = true;
      powerups[i].pos = pos;
      powerups[i].vel = (Vector2){0.0f, 160.0f};
      powerups[i].radius = 12.0f;
      powerups[i].type = type;
      return;
    }
  }
}

static void InitLevel(int level, Brick bricks[MAX_BRICKS],
                      int *breakable_left) {
  int layout[BRICK_ROWS][BRICK_COLS] = {0};
  if (level == 1) {
    int rows[BRICK_ROWS][BRICK_COLS] = {
        {0, 0, 1, 2, 1, 1, 3, 1, 2, 1, 0, 0},
        {0, 1, 1, 1, 5, 1, 1, 1, 5, 1, 1, 0},
        {1, 2, 1, 1, 1, 6, 1, 1, 1, 1, 2, 1},
        {1, 1, 1, 3, 1, 1, 1, 1, 3, 1, 1, 1},
        {1, 1, 2, 1, 5, 1, 1, 1, 5, 2, 1, 1},
        {0, 1, 6, 1, 1, 1, 1, 1, 1, 1, 1, 0},
        {0, 0, 1, 1, 2, 1, 1, 2, 1, 1, 0, 0},
        {0, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0},
    };
    for (int r = 0; r < BRICK_ROWS; r++) {
      for (int c = 0; c < BRICK_COLS; c++) {
        layout[r][c] = rows[r][c];
      }
    }
  } else if (level == 2) {
    int rows[BRICK_ROWS][BRICK_COLS] = {
        {0, 0, 2, 1, 1, 3, 1, 1, 2, 1, 0, 0},
        {0, 1, 1, 5, 4, 1, 1, 4, 5, 7, 1, 0},
        {1, 1, 1, 2, 1, 6, 1, 1, 2, 1, 7, 1},
        {1, 2, 1, 1, 1, 1, 1, 1, 1, 1, 2, 1},
        {1, 1, 1, 3, 5, 1, 1, 5, 3, 1, 1, 1},
        {0, 1, 7, 1, 2, 1, 1, 2, 1, 1, 1, 0},
        {0, 0, 1, 4, 1, 1, 1, 1, 4, 1, 0, 0},
        {0, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0},
    };
    for (int r = 0; r < BRICK_ROWS; r++) {
      for (int c = 0; c < BRICK_COLS; c++) {
        layout[r][c] = rows[r][c];
      }
    }
  } else {
    int rows[BRICK_ROWS][BRICK_COLS] = {
        {2, 1, 1, 1, 3, 1, 1, 3, 1, 1, 1, 2},
        {1, 1, 1, 4, 5, 1, 1, 5, 4, 7, 1, 1},
        {1, 7, 1, 1, 8, 8, 8, 8, 1, 1, 2, 1},
        {1, 1, 1, 1, 2, 1, 1, 2, 1, 1, 1, 1},
        {1, 1, 2, 1, 6, 4, 4, 6, 1, 2, 7, 1},
        {1, 8, 1, 3, 1, 5, 5, 1, 3, 1, 8, 1},
        {4, 8, 1, 1, 7, 1, 1, 2, 1, 1, 8, 1},
        {1, 1, 2, 1, 1, 1, 1, 1, 1, 2, 4, 1},
    };
    for (int r = 0; r < BRICK_ROWS; r++) {
      for (int c = 0; c < BRICK_COLS; c++) {
        layout[r][c] = rows[r][c];
      }
    }
  }

  *breakable_left = 0;
  float brick_w = (float)(PLAY_W - (BRICK_COLS - 1) * BRICK_GAP) / BRICK_COLS;
  float brick_h = 24.0f;
  for (int r = 0; r < BRICK_ROWS; r++) {
    for (int c = 0; c < BRICK_COLS; c++) {
      int idx = r * BRICK_COLS + c;
      int val = layout[r][c];
      bricks[idx].alive = (val > 0);
      bricks[idx].solid = false;
      bricks[idx].power_brick = false;
      bricks[idx].power_type = POWER_MULTIBALL;
      if (val == 2) {
        bricks[idx].power_brick = true;
        bricks[idx].power_type = POWER_MULTIBALL;
      } else if (val == 3) {
        bricks[idx].power_brick = true;
        bricks[idx].power_type = POWER_EXTEND;
      } else if (val == 4) {
        bricks[idx].power_brick = true;
        bricks[idx].power_type = POWER_DEATH;
      } else if (val == 5) {
        bricks[idx].power_brick = true;
        bricks[idx].power_type = POWER_SLOW;
      } else if (val == 6) {
        bricks[idx].power_brick = true;
        bricks[idx].power_type = POWER_LIFE;
      } else if (val == 7) {
        bricks[idx].power_brick = true;
        bricks[idx].power_type = POWER_FAST;
      } else if (val == 8) {
        bricks[idx].solid = true;
      }
      bricks[idx].max_hp = 1;
      bricks[idx].hp = bricks[idx].max_hp;
      bricks[idx].rect = (Rectangle){PLAY_X + c * (brick_w + BRICK_GAP),
                                     PLAY_Y + 40.0f + r * (brick_h + BRICK_GAP),
                                     brick_w, brick_h};
      if (bricks[idx].alive && !bricks[idx].solid) {
        (*breakable_left)++;
      }
    }
  }
}

int main(void) {
  InitWindow(SCREEN_W, SCREEN_H, "Block Breaker / pong");
  InitAudioDevice();
  SetTargetFPS(60);

  const char *app_dir = GetApplicationDirectory();
  if (app_dir != NULL && app_dir[0] != '\0') {
    ChangeDirectory(app_dir);
  }

  const char *font_path = "NotoSansMono-Regular.ttf";
  if (!FileExists(font_path)) {
    font_path = "../NotoSansMono-Regular.ttf";
  }
  if (!FileExists(font_path)) {
    CloseAudioDevice();
    CloseWindow();
    printf("Font file 'NotoSansMono-Regular.ttf' not found.\n");
    return 1;
  }
  Font ui_font = LoadFontEx(font_path, 48, NULL, 0);

  Music bgm = {0};
  Sound sfx_hit = {0};
  Sound sfx_break = {0};
  Sound sfx_power = {0};
  Sound sfx_lose = {0};
  Sound sfx_clear = {0};

  bool audio_ok = true;
  if (FileExists("background.wav")) {
    bgm = LoadMusicStream("background.wav");
  } else {
    audio_ok = false;
  }
  if (FileExists("gameclear.wav")) {
    sfx_clear = LoadSound("gameclear.wav");
  } else {
    audio_ok = false;
  }
  if (FileExists("gameover.wav")) {
    sfx_lose = LoadSound("gameover.wav");
  } else {
    audio_ok = false;
  }

  if (audio_ok) {
    SetSoundVolume(sfx_lose, 0.6f);
    SetSoundVolume(sfx_clear, 0.7f);
    SetMusicVolume(bgm, 0.45f);
    PlayMusicStream(bgm);
  }

  Rectangle paddle = {PLAY_X + PLAY_W * 0.5f - BASE_PADDLE_W * 0.5f,
                      PLAY_Y + PLAY_H - 40.0f, BASE_PADDLE_W, PADDLE_H};
  float paddle_target_w = BASE_PADDLE_W;

  Ball balls[MAX_BALLS] = {0};
  Brick bricks[MAX_BRICKS] = {0};
  Powerup powerups[MAX_POWERUPS] = {0};
  Particle particles[MAX_PARTICLES] = {0};
  Star stars[STAR_COUNT] = {0};

  for (int i = 0; i < STAR_COUNT; i++) {
    stars[i].pos = (Vector2){(float)GetRandomValue(0, SCREEN_W),
                             (float)GetRandomValue(0, SCREEN_H)};
    stars[i].radius = 1.0f + (float)GetRandomValue(0, 2);
    stars[i].twinkle = (float)GetRandomValue(0, 100) / 100.0f;
  }

  GameState state = STATE_MENU;
  int level = 1;
  int selected_level = 1;
  int breakable_left = 0;
  int score = 0;
  int lives = 3;
  int combo = 0;
  float shake_time = 0.0f;
  float shake_mag = 0.0f;
  int speed_state = 0;
  float speed_timer = 0.0f;

  InitLevel(level, bricks, &breakable_left);
  ResetBalls(balls, paddle);

  while (!WindowShouldClose()) {
    float dt = GetFrameTime();

    if (audio_ok) {
      UpdateMusicStream(bgm);
    }

    if (state == STATE_MENU) {
      Rectangle buttons[3] = {
          {SCREEN_W / 2.0f - 140.0f, 250.0f, 280.0f, 34.0f},
          {SCREEN_W / 2.0f - 140.0f, 290.0f, 280.0f, 34.0f},
          {SCREEN_W / 2.0f - 140.0f, 330.0f, 280.0f, 34.0f},
      };

      if (IsKeyPressed(KEY_ONE))
        selected_level = 1;
      if (IsKeyPressed(KEY_TWO))
        selected_level = 2;
      if (IsKeyPressed(KEY_THREE))
        selected_level = 3;
      if (IsKeyPressed(KEY_UP)) {
        selected_level--;
        if (selected_level < 1)
          selected_level = 3;
      }
      if (IsKeyPressed(KEY_DOWN)) {
        selected_level++;
        if (selected_level > 3)
          selected_level = 1;
      }
      if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        Vector2 mouse = GetMousePosition();
        for (int i = 0; i < 3; i++) {
          if (CheckCollisionPointRec(mouse, buttons[i])) {
            selected_level = i + 1;
            level = selected_level;
            score = 0;
            lives = 3;
            combo = 0;
            speed_state = 0;
            speed_timer = 0.0f;
            paddle_target_w = BASE_PADDLE_W;
            paddle.width = BASE_PADDLE_W;
            paddle.x = PLAY_X + PLAY_W * 0.5f - paddle.width * 0.5f;
            InitLevel(level, bricks, &breakable_left);
            ResetBalls(balls, paddle);
            for (int p = 0; p < MAX_POWERUPS; p++)
              powerups[p].active = false;
            for (int p = 0; p < MAX_PARTICLES; p++)
              particles[p].active = false;
            state = STATE_PLAY;
            break;
          }
        }
      }
      if (IsKeyPressed(KEY_ENTER)) {
        level = selected_level;
        score = 0;
        lives = 3;
        combo = 0;
        speed_state = 0;
        speed_timer = 0.0f;
        paddle_target_w = BASE_PADDLE_W;
        paddle.width = BASE_PADDLE_W;
        paddle.x = PLAY_X + PLAY_W * 0.5f - paddle.width * 0.5f;
        InitLevel(level, bricks, &breakable_left);
        ResetBalls(balls, paddle);
        for (int i = 0; i < MAX_POWERUPS; i++)
          powerups[i].active = false;
        for (int i = 0; i < MAX_PARTICLES; i++)
          particles[i].active = false;
        state = STATE_PLAY;
      }
    } else if (state == STATE_PAUSE) {
      if (IsKeyPressed(KEY_P)) {
        state = STATE_PLAY;
      }
    } else if (state == STATE_OVER) {
      if (IsKeyPressed(KEY_ENTER)) {
        state = STATE_MENU;
      }
    } else if (state == STATE_CLEAR) {
      if (IsKeyPressed(KEY_ENTER)) {
        state = STATE_MENU;
      }
    } else if (state == STATE_PLAY) {
      if (IsKeyPressed(KEY_P)) {
        state = STATE_PAUSE;
      }

      bool any_stuck = false;
      for (int i = 0; i < MAX_BALLS; i++) {
        if (balls[i].active && balls[i].stuck) {
          any_stuck = true;
          break;
        }
      }

      if (!any_stuck) {
        float move = 0.0f;
        if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A))
          move -= 1.0f;
        if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D))
          move += 1.0f;
        paddle.x += move * PADDLE_SPEED * dt;
        paddle.x = ClampFloat(paddle.x, PLAY_X, PLAY_X + PLAY_W - paddle.width);

        paddle.width += (paddle_target_w - paddle.width) * 8.0f * dt;
        paddle.x = ClampFloat(paddle.x, PLAY_X, PLAY_X + PLAY_W - paddle.width);
      }

      float base_speed = BALL_BASE_SPEED * LevelSpeedMult(level);
      float speed_mult = SpeedItemMult(speed_state);
      float current_speed = base_speed * speed_mult;

      if (IsKeyPressed(KEY_SPACE)) {
        for (int i = 0; i < MAX_BALLS; i++) {
          if (balls[i].active && balls[i].stuck) {
            LaunchBall(&balls[i]);
          }
        }
      }

      for (int i = 0; i < MAX_BALLS; i++) {
        if (!balls[i].active)
          continue;

        if (balls[i].stuck) {
          balls[i].pos.x = paddle.x + paddle.width * 0.5f;
          balls[i].pos.y = paddle.y - balls[i].radius - 2.0f;
          continue;
        }

        balls[i].pos.x += balls[i].vel.x * dt * current_speed;
        balls[i].pos.y += balls[i].vel.y * dt * current_speed;

        if (balls[i].pos.x - balls[i].radius < PLAY_X) {
          balls[i].pos.x = PLAY_X + balls[i].radius;
          balls[i].vel.x *= -1.0f;
          if (audio_ok && sfx_hit.frameCount > 0)
            PlaySound(sfx_hit);
        }
        if (balls[i].pos.x + balls[i].radius > PLAY_X + PLAY_W) {
          balls[i].pos.x = PLAY_X + PLAY_W - balls[i].radius;
          balls[i].vel.x *= -1.0f;
          if (audio_ok && sfx_hit.frameCount > 0)
            PlaySound(sfx_hit);
        }
        if (balls[i].pos.y - balls[i].radius < PLAY_Y) {
          balls[i].pos.y = PLAY_Y + balls[i].radius;
          balls[i].vel.y *= -1.0f;
          if (audio_ok && sfx_hit.frameCount > 0)
            PlaySound(sfx_hit);
        }

        if (balls[i].pos.y - balls[i].radius > PLAY_Y + PLAY_H) {
          balls[i].active = false;
        }

        Rectangle paddle_rect = {paddle.x, paddle.y, paddle.width,
                                 paddle.height};
        if (CheckCollisionCircleRec(balls[i].pos, balls[i].radius,
                                    paddle_rect) &&
            balls[i].vel.y > 0.0f) {
          float hit = (balls[i].pos.x - (paddle.x + paddle.width * 0.5f)) /
                      (paddle.width * 0.5f);
          hit = ClampFloat(hit, -1.0f, 1.0f);
          float angle = hit * 70.0f * DEG2RAD;
          balls[i].vel.x = sinf(angle);
          balls[i].vel.y = -cosf(angle);
          combo = 0;
          if (audio_ok && sfx_hit.frameCount > 0)
            PlaySound(sfx_hit);
        }

        bool bounced = false;
        for (int b = 0; b < MAX_BRICKS; b++) {
          if (!bricks[b].alive)
            continue;
          if (CheckCollisionCircleRec(balls[i].pos, balls[i].radius,
                                      bricks[b].rect)) {
            float nearest_x =
                ClampFloat(balls[i].pos.x, bricks[b].rect.x,
                           bricks[b].rect.x + bricks[b].rect.width);
            float nearest_y =
                ClampFloat(balls[i].pos.y, bricks[b].rect.y,
                           bricks[b].rect.y + bricks[b].rect.height);
            float dx = balls[i].pos.x - nearest_x;
            float dy = balls[i].pos.y - nearest_y;
            if (fabsf(dx) > fabsf(dy)) {
              balls[i].vel.x *= -1.0f;
            } else {
              balls[i].vel.y *= -1.0f;
            }
            balls[i].vel = NormalizeSafe(balls[i].vel);

            if (!bricks[b].solid) {
              bricks[b].hp -= 1;
              if (bricks[b].hp <= 0) {
                bricks[b].alive = false;
                breakable_left--;
                score += 100 + combo * 30;
                combo++;
                SpawnParticles(
                    particles,
                    (Vector2){bricks[b].rect.x + bricks[b].rect.width * 0.5f,
                              bricks[b].rect.y + bricks[b].rect.height * 0.5f},
                    BrickColor(&bricks[b]));
                shake_time = 0.15f;
                shake_mag = 6.0f;
                if (audio_ok && sfx_break.frameCount > 0)
                  PlaySound(sfx_break);
                if (bricks[b].power_brick) {
                  SpawnPowerup(
                      powerups,
                      (Vector2){bricks[b].rect.x + bricks[b].rect.width * 0.5f,
                                bricks[b].rect.y +
                                    bricks[b].rect.height * 0.5f},
                      bricks[b].power_type);
                }
              } else {
                score += 40;
                if (audio_ok && sfx_hit.frameCount > 0)
                  PlaySound(sfx_hit);
              }
            } else {
              score += 10;
              if (audio_ok && sfx_hit.frameCount > 0)
                PlaySound(sfx_hit);
            }
            bounced = true;
            break;
          }
        }

        if (bounced) {
          balls[i].pos.x += balls[i].vel.x * dt * current_speed;
          balls[i].pos.y += balls[i].vel.y * dt * current_speed;
        }
      }

      bool any_ball = false;
      for (int i = 0; i < MAX_BALLS; i++) {
        if (balls[i].active) {
          any_ball = true;
          break;
        }
      }
      if (!any_ball) {
        lives--;
        combo = 0;
        if (lives <= 0) {
          if (audio_ok)
            PlaySound(sfx_lose);
          state = STATE_OVER;
        } else {
          ResetBalls(balls, paddle);
          paddle_target_w = BASE_PADDLE_W;
          paddle.width = BASE_PADDLE_W;
          paddle.x = PLAY_X + PLAY_W * 0.5f - paddle.width * 0.5f;
          speed_state = 0;
          speed_timer = 0.0f;
        }
      }

      for (int i = 0; i < MAX_POWERUPS; i++) {
        if (!powerups[i].active)
          continue;
        if (!any_stuck) {
          powerups[i].pos.y += powerups[i].vel.y * dt;
        }
        if (powerups[i].pos.y - powerups[i].radius > PLAY_Y + PLAY_H) {
          powerups[i].active = false;
          continue;
        }
        if (CheckCollisionCircleRec(
                powerups[i].pos, powerups[i].radius,
                (Rectangle){paddle.x, paddle.y, paddle.width, paddle.height})) {
          powerups[i].active = false;
          if (audio_ok && sfx_power.frameCount > 0)
            PlaySound(sfx_power);
          if (powerups[i].type == POWER_EXTEND) {
            paddle_target_w = BASE_PADDLE_W * 1.6f;
          } else if (powerups[i].type == POWER_MULTIBALL) {
            for (int b = 0; b < MAX_BALLS; b++) {
              if (!balls[b].active) {
                balls[b].active = true;
                balls[b].stuck = false;
                balls[b].pos =
                    (Vector2){paddle.x + paddle.width * 0.5f, paddle.y - 20};
                LaunchBall(&balls[b]);
              }
            }
          } else if (powerups[i].type == POWER_SLOW) {
            speed_state = -1;
            speed_timer = 10.0f;
          } else if (powerups[i].type == POWER_LIFE) {
            lives++;
          } else if (powerups[i].type == POWER_FAST) {
            speed_state = 1;
            speed_timer = 10.0f;
          } else if (powerups[i].type == POWER_DEATH) {
            lives--;
            if (lives <= 0) {
              if (audio_ok)
                PlaySound(sfx_lose);
              state = STATE_OVER;
            }
          }
        }
      }

      if (speed_timer > 0.0f) {
        speed_timer -= dt;
        if (speed_timer <= 0.0f) {
          speed_timer = 0.0f;
          speed_state = 0;
        }
      }

      for (int i = 0; i < MAX_PARTICLES; i++) {
        if (!particles[i].active)
          continue;
        particles[i].life -= dt;
        if (particles[i].life <= 0.0f) {
          particles[i].active = false;
          continue;
        }
        particles[i].pos.x += particles[i].vel.x * dt;
        particles[i].pos.y += particles[i].vel.y * dt;
        particles[i].vel.y += 120.0f * dt;
      }

      if (breakable_left <= 0) {
        state = STATE_CLEAR;
        if (audio_ok)
          PlaySound(sfx_clear);
      }
    }

    Vector2 shake = {0.0f, 0.0f};
    if (shake_time > 0.0f) {
      shake_time -= dt;
      shake.x = (float)GetRandomValue(-(int)shake_mag, (int)shake_mag);
      shake.y = (float)GetRandomValue(-(int)shake_mag, (int)shake_mag);
    }

    BeginDrawing();
    ClearBackground((Color){8, 16, 24, 255});

    DrawRectangleGradientV(0, 0, SCREEN_W, SCREEN_H, (Color){10, 25, 35, 255},
                           (Color){5, 10, 15, 255});

    for (int i = 0; i < STAR_COUNT; i++) {
      float glow = 0.5f + 0.5f * sinf((GetTime() + stars[i].twinkle) * 2.0f);
      DrawCircleV(stars[i].pos, stars[i].radius,
                  Fade(RAYWHITE, 0.3f + glow * 0.5f));
    }

    DrawRectangle(PLAY_X - 10, PLAY_Y - 10, PLAY_W + 20, PLAY_H + 20,
                  (Color){30, 38, 45, 255});
    DrawRectangle(PLAY_X, PLAY_Y, PLAY_W, PLAY_H, (Color){17, 21, 32, 255});

    Camera2D camera = {0};
    camera.target = (Vector2){0.0f, 0.0f};
    camera.offset = shake;
    camera.zoom = 1.0f;
    BeginMode2D(camera);

    for (int i = 0; i < MAX_BRICKS; i++) {
      if (!bricks[i].alive)
        continue;
      Color c = BrickColor(&bricks[i]);
      DrawRectangleRounded(bricks[i].rect, 0.2f, 6, c);
      DrawRectangleLinesEx(bricks[i].rect, 1.5f, Fade(BLACK, 0.2f));
    }

    for (int i = 0; i < MAX_PARTICLES; i++) {
      if (!particles[i].active)
        continue;
      DrawCircleV(particles[i].pos, 2.2f,
                  Fade(particles[i].color, particles[i].life));
    }

    for (int i = 0; i < MAX_POWERUPS; i++) {
      if (!powerups[i].active)
        continue;
      Color pc = (Color){129, 199, 132, 255};
      char label = 'E';
      if (powerups[i].type == POWER_MULTIBALL) {
        pc = (Color){100, 181, 246, 255};
        label = 'M';
      } else if (powerups[i].type == POWER_SLOW) {
        pc = (Color){255, 213, 79, 255};
        label = 'S';
      } else if (powerups[i].type == POWER_LIFE) {
        pc = (Color){244, 143, 177, 255};
        label = 'L';
      } else if (powerups[i].type == POWER_FAST) {
        pc = (Color){255, 167, 38, 255};
        label = 'F';
      } else if (powerups[i].type == POWER_DEATH) {
        pc = (Color){239, 83, 80, 255};
        label = 'X';
      }
      DrawCircleV(powerups[i].pos, powerups[i].radius, pc);
      const char *label_text = TextFormat("%c", label);
      Vector2 label_size = MeasureTextEx(ui_font, label_text, 16.0f, 1.0f);
      DrawTextFont(ui_font, label_text,
                   (int)(powerups[i].pos.x - label_size.x * 0.5f),
                   (int)(powerups[i].pos.y - label_size.y * 0.5f), 16, BLACK);
    }

    DrawRectangleRounded(
        (Rectangle){paddle.x, paddle.y, paddle.width, paddle.height}, 0.4f, 8,
        (Color){130, 190, 255, 255});

    for (int i = 0; i < MAX_BALLS; i++) {
      if (!balls[i].active)
        continue;
      DrawCircleV(balls[i].pos, balls[i].radius, (Color){255, 238, 88, 255});
      DrawCircleLines((int)balls[i].pos.x, (int)balls[i].pos.y, balls[i].radius,
                      Fade(WHITE, 0.5f));
    }

    EndMode2D();

    DrawTextFont(ui_font, "BLOCK BREAKER", 24, 24, 28, RAYWHITE);
    DrawTextFont(ui_font, TextFormat("LEVEL %d", level), 24, 54, 18,
                 Fade(WHITE, 0.75f));

    DrawTextFont(ui_font, TextFormat("SCORE %05d", score), 720, 24, 20,
                 RAYWHITE);
    DrawTextFont(ui_font, TextFormat("LIFE %d", lives), 720, 52, 18,
                 Fade(WHITE, 0.75f));
    if (combo > 1) {
      DrawTextFont(ui_font, TextFormat("COMBO x%d", combo), 430, 54, 18,
                   (Color){255, 214, 102, 255});
    }

    if (state == STATE_MENU) {
      DrawRectangle(210, 190, 580, 360, (Color){20, 28, 40, 220});
      DrawRectangleLines(210, 190, 580, 360, Fade(WHITE, 0.4f));
      DrawCenteredText(ui_font, "SELECT LEVEL", SCREEN_W / 2, 220, 26,
                       RAYWHITE);
      const char *labels[3] = {"[1] EASY", "[2] NORMAL", "[3] HARD"};
      for (int i = 0; i < 3; i++) {
        Rectangle btn = {SCREEN_W / 2.0f - 140.0f, 250.0f + i * 40.0f, 280.0f,
                         34.0f};
        Color fill = (i + 1 == selected_level) ? (Color){80, 120, 160, 255}
                                               : (Color){30, 40, 60, 255};
        DrawRectangleRounded(btn, 0.25f, 6, fill);
        DrawRectangleLinesEx(btn, 1.5f, Fade(WHITE, 0.35f));
        Vector2 dim = MeasureTextEx(ui_font, labels[i], 20.0f, 1.0f);
        float text_x = btn.x + 22.0f;
        float text_y = btn.y + (btn.height - dim.y) * 0.5f;
        DrawTextFont(ui_font, labels[i], (int)text_x, (int)text_y, 20,
                     RAYWHITE);
      }
      DrawCenteredText(ui_font, "ENTER: START", SCREEN_W / 2, 395, 20,
                       Fade(WHITE, 0.85f));
      DrawCenteredText(ui_font, "UP/DOWN or 1-3", SCREEN_W / 2, 420, 18,
                       Fade(WHITE, 0.7f));
      DrawCenteredText(ui_font, "A/D or Left/Right: MOVE", SCREEN_W / 2, 455,
                       18, Fade(WHITE, 0.8f));
      DrawCenteredText(ui_font, "SPACE: LAUNCH BALL", SCREEN_W / 2, 480, 18,
                       Fade(WHITE, 0.8f));
      DrawCenteredText(ui_font, "P: PAUSE", SCREEN_W / 2, 505, 18,
                       Fade(WHITE, 0.8f));
    }

    if (state == STATE_PAUSE) {
      DrawRectangle(270, 290, 460, 120, (Color){10, 15, 25, 220});
      DrawCenteredText(ui_font, "PAUSE", SCREEN_W / 2, 320, 32, RAYWHITE);
      DrawCenteredText(ui_font, "Press P to resume", SCREEN_W / 2, 360, 18,
                       Fade(WHITE, 0.8f));
    }

    if (state == STATE_OVER) {
      DrawRectangle(260, 260, 480, 170, (Color){35, 18, 20, 230});
      DrawCenteredText(ui_font, "GAME OVER", SCREEN_W / 2, 300, 32,
                       (Color){255, 120, 120, 255});
      DrawCenteredText(ui_font, "Press Enter", SCREEN_W / 2, 350, 18,
                       Fade(WHITE, 0.8f));
    }

    if (state == STATE_CLEAR) {
      DrawRectangle(260, 260, 480, 170, (Color){20, 35, 30, 230});
      DrawCenteredText(ui_font, "STAGE CLEAR", SCREEN_W / 2, 300, 30,
                       (Color){130, 220, 180, 255});
      DrawCenteredText(ui_font, "Press Enter", SCREEN_W / 2, 350, 18,
                       Fade(WHITE, 0.8f));
    }

    EndDrawing();
  }

  if (audio_ok) {
    StopMusicStream(bgm);
    UnloadMusicStream(bgm);
    UnloadSound(sfx_hit);
    UnloadSound(sfx_break);
    UnloadSound(sfx_power);
    UnloadSound(sfx_lose);
    UnloadSound(sfx_clear);
  }
  UnloadFont(ui_font);
  CloseAudioDevice();
  CloseWindow();
  return 0;
}
