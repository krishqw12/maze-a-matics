// G++ Compilation:
// g++ -o maze-a-matics main.cpp -I$HOME/raylib-install/include -L$HOME/raylib-install/lib -lraylib -lGL -lm -lpthread -ldl
// -lrt -lX11 -Wl,-rpath,$HOME/raylib-install/lib

#include <cmath>
#include <queue>
#include <raylib.h>
#include <raymath.h>
#include <stdlib.h>
#include <time.h>
#include <cstdio>
#include <vector>

const int GRID_SIZE = 51;
const float CELL_SIZE = 1.0f;
const float WALL_HEIGHT = 4.0f;
const float CAMERA_HEIGHT = 1.0f;
const float MOVE_DURATION = 0.17f;
const float TURN_DURATION = 0.17f;
float enemySwitchTimer = 0.0f;
float weakModeTimer = 0.0f;
bool enemyVisualWeak = false;

Color COLOR_FOG = {10, 15, 10, 255};

static const int WALL = 0;
static const int FLOOR = 1;
static int grid[GRID_SIZE][GRID_SIZE];
static std::vector<std::vector<bool>> visited;

struct Player {
  Vector2 gridPos;
  Vector2 moveFrom;
  Vector2 moveTo;
  float animTimer;
  int currentRotation;
  int state;
  bool wHeld;
  bool aHeld;
  bool sHeld;
  bool dHeld;
  float pendingTurn;
  float turnFrom;
  float turnTo;
};
static Player player;

struct Enemy {
  Vector2 gridPos;
  Vector2 moveFrom;
  Vector2 moveTo;
  float animTimer;
  bool isMoving;
  Texture2D texture;
};
static std::vector<Enemy> enemies;

struct Star {
  Vector2 gridPos;
  float bobTimer;
  bool active;
};
static std::vector<Star> stars;
static const int STAR_COUNT = 5;

enum PowerUpType { POWERUP_FREEZE, POWERUP_MUSCLE, POWERUP_ACCELERATE };

struct PowerUp {
  Vector2 gridPos;
  PowerUpType type;
  bool active;
  float bobTimer;
};
static std::vector<PowerUp> powerups;
static const int POWERUP_COUNT = 3;
static float freezeTimer = 0.0f;
static float muscleTimer = 0.0f;
static float accelerateTimer = 0.0f;
static int currentStreak = 0;
static int maxStreak = 0;

enum Difficulty { EASY, MEDIUM, HARD };
static Difficulty currentDifficulty = MEDIUM;
static int enemyCount = 5;
static float enemyMoveInterval = 0.15f;
static float quizTime = 7.0f;

static const int IDLE = 0;
static const int MOVING = 1;
static const int TURNING = 2;
static const int PLAYING = 3;
static const int GAMEOVER = 4;
static const int VICTORY = 5;
static const int HOMESCREEN = 6;
static const int STAR_QUIZ = 7;

static int quizNum1, quizNum2, quizCorrectAnswer;
static char quizInput[16] = {0};
static int quizInputLen = 0;
static float quizFreezeTimer = 0.0f;
static bool quizActive = false;

static AudioStream eerieStream;
static Texture2D wallTex, floorTex, enemyTex, starTex, roofTex, enemyWeakTex, freezeTex, muscleTex, accelerateTex;
static Mesh wallMesh, floorMesh, roofMesh;
static Material wallMaterial, floorMaterial, roofMaterial;
static Sound bgMusic, deathSound, beginSound, reqradSound;
static bool deathSoundPlayed = false;
static bool eeriePlaying = false;
static float eeriePhase = 0.0f;
static float eerieLFO = 0.0f;

float Smoothstep(float t) { return t * t * (3.0f - 2.0f * t); }

struct BSPRect {
  int x, y, w, h;
};

void BSPPartition(BSPRect rect, int depth) {
  if (depth >= 8 || rect.w < 4 || rect.h < 4) {
    if ((rand() % 100) < 80 && rect.w >= 3 && rect.h >= 3) {
      int rw = 2 + rand() % (rect.w - 2);
      int rh = 2 + rand() % (rect.h - 2);
      int rx = rect.x + 1 + rand() % (rect.w - rw - 2);
      int ry = rect.y + 1 + rand() % (rect.h - rh - 2);
      for (int py = ry; py < ry + rh && py < GRID_SIZE - 1; py++) {
        for (int px = rx; px < rx + rw && px < GRID_SIZE - 1; px++) {
          if (py > 0 && py < GRID_SIZE - 1 && px > 0 && px < GRID_SIZE - 1) {
            grid[py][px] = FLOOR;
          }
        }
      }
    }
    return;
  }

  bool splitH = (rand() % 2 == 0);
  if (rect.w <= 4)
    splitH = true;
  if (rect.h <= 4)
    splitH = false;

  if (splitH) {
    int splitY = rect.y + 2 + rand() % (rect.h - 4);
    BSPRect top = {rect.x, rect.y, rect.w, splitY - rect.y};
    BSPRect bottom = {rect.x, splitY, rect.w, rect.y + rect.h - splitY};

    for (int cx = rect.x; cx < rect.x + rect.w && cx < GRID_SIZE - 1; cx++) {
      if (cx > 0 && splitY > 0 && splitY < GRID_SIZE - 1) {
        grid[splitY][cx] = FLOOR;
      }
    }

    BSPPartition(top, depth + 1);
    BSPPartition(bottom, depth + 1);
  } else {
    int splitX = rect.x + 2 + rand() % (rect.w - 4);
    BSPRect left = {rect.x, rect.y, splitX - rect.x, rect.h};
    BSPRect right = {splitX, rect.y, rect.x + rect.w - splitX, rect.h};

    for (int cy = rect.y; cy < rect.y + rect.h && cy < GRID_SIZE - 1; cy++) {
      if (cy > 0 && splitX > 0 && splitX < GRID_SIZE - 1) {
        grid[cy][splitX] = FLOOR;
      }
    }

    BSPPartition(left, depth + 1);
    BSPPartition(right, depth + 1);
  }
}

void EnsureConnectivity() {
  bool visited[101][101] = {false};
  std::queue<std::pair<int, int>> q;
  int cx = GRID_SIZE / 2;
  int cy = GRID_SIZE / 2;

  q.push({cx, cy});
  visited[cy][cx] = true;

  int dx[4] = {-1, 1, 0, 0};
  int dy[4] = {0, 0, -1, 1};

  while (!q.empty()) {
    auto [x, y] = q.front();
    q.pop();

    for (int i = 0; i < 4; i++) {
      int nx = x + dx[i];
      int ny = y + dy[i];
      if (nx > 0 && nx < GRID_SIZE - 1 && ny > 0 && ny < GRID_SIZE - 1) {
        if (!visited[ny][nx] && grid[ny][nx] == FLOOR) {
          visited[ny][nx] = true;
          q.push({nx, ny});
        }
      }
    }
  }

  for (int y = 1; y < GRID_SIZE - 1; y++) {
    for (int x = 1; x < GRID_SIZE - 1; x++) {
      if (grid[y][x] == FLOOR && !visited[y][x]) {
        bool connected = false;
        for (int dir = 0; dir < 4 && !connected; dir++) {
          int nx = x;
          int ny = y;
          while (nx > 0 && nx < GRID_SIZE - 1 && ny > 0 && ny < GRID_SIZE - 1) {
            nx += dx[dir];
            ny += dy[dir];
            if (grid[ny][nx] == FLOOR && visited[ny][nx]) {
              for (int fill = 0; fill < abs(nx - x) + abs(ny - y); fill++) {
                if (dir < 2) {
                  if (x + fill * dx[dir] > 0 && x + fill * dx[dir] < GRID_SIZE - 1 && y > 0 && y < GRID_SIZE - 1) {
                    grid[y][x + fill * dx[dir]] = FLOOR;
                  }
                } else {
                  if (x > 0 && x < GRID_SIZE - 1 && y + fill * dy[dir] > 0 && y + fill * dy[dir] < GRID_SIZE - 1) {
                    grid[y + fill * dy[dir]][x] = FLOOR;
                  }
                }
              }
              connected = true;
              break;
            }
          }
        }
      }
    }
  }
}

void UpdateEerieAudio(AudioStream stream, float dt) {
  if (!eeriePlaying || !IsAudioStreamProcessed(stream))
    return;

  const int SAMPLES = 512;
  static float buffer[512];

  eerieLFO += dt * 0.15f;
  float lfo = sinf(eerieLFO) * 0.3f + 0.7f;

  for (int i = 0; i < SAMPLES; i++) {
    float sample = 0.0f;

    float note1 = sinf(eeriePhase);
    eeriePhase += 2.0f * PI * 55.0f / 22050.0f;
    if (eeriePhase > 2.0f * PI)
      eeriePhase -= 2.0f * PI;

    float note2 = sinf(eeriePhase * 1.5f);

    sample = note1 * 0.5f * lfo + note2 * 0.3f;
    sample *= 0.12f;

    buffer[i] = sample;
  }

  UpdateAudioStream(stream, buffer, SAMPLES);
}

void GenerateMaze() {
  for (int y = 0; y < GRID_SIZE; y++) {
    for (int x = 0; x < GRID_SIZE; x++) {
      grid[y][x] = WALL;
    }
  }

  int sectors = 6;
  int sectorSize = (GRID_SIZE - 2) / sectors;

  for (int sy = 0; sy < sectors; sy++) {
    for (int sx = 0; sx < sectors; sx++) {
      int baseX = 1 + sx * sectorSize;
      int baseY = 1 + sy * sectorSize;

      int roomW = 2 + rand() % (sectorSize - 3);
      int roomH = 2 + rand() % (sectorSize - 3);
      int roomX = baseX + rand() % (sectorSize - roomW - 1);
      int roomY = baseY + rand() % (sectorSize - roomH - 1);

      for (int dy = 0; dy < roomH && roomY + dy < GRID_SIZE - 2; dy++) {
        for (int dx = 0; dx < roomW && roomX + dx < GRID_SIZE - 2; dx++) {
          grid[roomY + dy][roomX + dx] = FLOOR;
        }
      }
    }
  }

  for (int sy = 0; sy < sectors; sy++) {
    for (int sx = 0; sx < sectors; sx++) {
      int baseX = 1 + sx * sectorSize;
      int baseY = 1 + sy * sectorSize;
      int midX = baseX + sectorSize / 2;
      int midY = baseY + sectorSize / 2;

      if (sx < sectors - 1) {
        for (int cx = baseX; cx < baseX + sectorSize && cx < GRID_SIZE - 2; cx++) {
          grid[midY][cx] = FLOOR;
        }
      }
      if (sy < sectors - 1) {
        for (int cy = baseY; cy < baseY + sectorSize && cy < GRID_SIZE - 2; cy++) {
          grid[cy][midX] = FLOOR;
        }
      }
    }
  }

  for (int y = 0; y < GRID_SIZE; y++) {
    grid[y][0] = WALL;
    grid[y][GRID_SIZE - 1] = WALL;
  }
  for (int x = 0; x < GRID_SIZE; x++) {
    grid[0][x] = WALL;
    grid[GRID_SIZE - 1][x] = WALL;
  }

  int cx = GRID_SIZE / 2;
  int cy = GRID_SIZE / 2;
  grid[cy][cx] = FLOOR;
  grid[cy][cx + 1] = FLOOR;
  grid[cy + 1][cx] = FLOOR;
  grid[cy + 1][cx + 1] = FLOOR;
  grid[cy][cx - 1] = FLOOR;
  grid[cy - 1][cx] = FLOOR;
}

bool IsValidMove(int x, int y) {
  if (x < 0 || x >= GRID_SIZE || y < 0 || y >= GRID_SIZE)
    return false;
  if (muscleTimer > 0.0f && grid[y][x] == WALL) {
    if (x > 0 && x < GRID_SIZE - 1 && y > 0 && y < GRID_SIZE - 1) {
      grid[y][x] = FLOOR;
    }
    return true;
  }
  return grid[y][x] == FLOOR;
}

void InitPlayer() {
  for (int y = 1; y < GRID_SIZE - 1; y++) {
    for (int x = 1; x < GRID_SIZE - 1; x++) {
      if (grid[y][x] == FLOOR) {
        bool hasEnemy = false;
        for (auto &e : enemies) {
          if ((int)e.gridPos.x == x && (int)e.gridPos.y == y) {
            hasEnemy = true;
            break;
          }
        }
        if (!hasEnemy) {
          player.gridPos = {(float)x, (float)y};
          player.currentRotation = 0.0f;
          player.state = IDLE;
          player.animTimer = 0.0f;
          player.pendingTurn = 0.0f;
          return;
        }
      }
    }
  }

  player.gridPos = {(float)(GRID_SIZE / 2), (float)(GRID_SIZE / 2)};
  player.currentRotation = 0.0f;
  player.state = IDLE;
  player.animTimer = 0.0f;
  player.pendingTurn = 0.0f;
}

void SpawnEnemies() {
    enemies.clear();
    int spawnCount = 0;
    int attempts = 0;

    while (spawnCount < enemyCount && attempts < 10000) {
    attempts++;
    int x = 1 + rand() % (GRID_SIZE - 2);
    int y = 1 + rand() % (GRID_SIZE - 2);

    if (grid[y][x] == FLOOR) {
      float dx = x - player.gridPos.x;
      float dy = y - player.gridPos.y;
      if (sqrtf(dx * dx + dy * dy) > 15.0f) {
        bool occupied = false;
        for (auto &e : enemies) {
          if ((int)e.gridPos.x == x && (int)e.gridPos.y == y) {
            occupied = true;
            break;
          }
        }
        if (!occupied) {
          Enemy e;
          e.gridPos = {(float)x, (float)y};
          e.moveFrom = e.gridPos;
          e.moveTo = e.gridPos;
          e.animTimer = 0.0f;
          e.isMoving = false;
          e.texture = enemyTex;
          enemies.push_back(e);
          spawnCount++;
        }
      }
    }
  }
}

void SpawnStars() {
  stars.clear();
  int spawnCount = 0;
  int attempts = 0;

  while (spawnCount < STAR_COUNT && attempts < 10000) {
    attempts++;
    int x = 1 + rand() % (GRID_SIZE - 2);
    int y = 1 + rand() % (GRID_SIZE - 2);

    if (grid[y][x] == FLOOR) {
      float dx = x - player.gridPos.x;
      float dy = y - player.gridPos.y;
      if (sqrtf(dx * dx + dy * dy) > 10.0f) {
        bool occupied = false;
        for (auto &s : stars) {
          if ((int)s.gridPos.x == x && (int)s.gridPos.y == y) {
            occupied = true;
            break;
          }
        }
        for (auto &e : enemies) {
          if ((int)e.gridPos.x == x && (int)e.gridPos.y == y) {
            occupied = true;
            break;
          }
        }
        if (!occupied) {
          Star s;
          s.gridPos = {(float)x, (float)y};
          s.active = true;
          s.bobTimer = (float)(rand() % 100) * 0.1f;
          stars.push_back(s);
          spawnCount++;
        }
      }
    }
  }
}

void SpawnPowerUps() {
    powerups.clear();
    std::vector<PowerUpType> types = { POWERUP_FREEZE, POWERUP_MUSCLE, POWERUP_ACCELERATE };
    int spawned = 0;
    
    for (int i = 0; i < POWERUP_COUNT; i++) {
        int attempts = 0;
        bool found = false;
        
        while (attempts < 10000 && !found) {
            attempts++;
            int x = 1 + rand() % (GRID_SIZE - 2);
            int y = 1 + rand() % (GRID_SIZE - 2);

            if (grid[y][x] == FLOOR) {
                float dx = x - player.gridPos.x;
                float dy = y - player.gridPos.y;
                if (sqrtf(dx*dx + dy*dy) > 8.0f) {
                    bool occupied = false;
                    for (auto& p : powerups) {
                        if ((int)p.gridPos.x == x && (int)p.gridPos.y == y) {
                            occupied = true;
                            break;
                        }
                    }
                    for (auto& s : stars) {
                        if ((int)s.gridPos.x == x && (int)s.gridPos.y == y) {
                            occupied = true;
                            break;
                        }
                    }
                    for (auto& e : enemies) {
                        if ((int)e.gridPos.x == x && (int)e.gridPos.y == y) {
                            occupied = true;
                            break;
                        }
                    }
                    if (!occupied) {
                        PowerUp p;
                        p.gridPos = { (float)x, (float)y };
                        p.type = types[spawned];
                        p.active = true;
                        p.bobTimer = (float)(rand() % 100) * 0.1f;
                        powerups.push_back(p);
                        spawned++;
                        found = true;
                    }
                }
            }
        }
        
        if (!found && spawned > 0) {
            PowerUp p;
            p.gridPos = { (float)(GRID_SIZE/2 + spawned), (float)(GRID_SIZE/2) };
            p.type = types[spawned];
            p.active = true;
            p.bobTimer = 0.0f;
            powerups.push_back(p);
            spawned++;
        }
    }
}

void SetDifficultySettings() {
    switch (currentDifficulty) {
        case EASY: enemyCount = 3; enemyMoveInterval = 0.30f; quizTime = 10.0f; break;
        case MEDIUM: enemyCount = 5; enemyMoveInterval = 0.15f; quizTime = 7.0f; break;
        case HARD: enemyCount = 10; enemyMoveInterval = 0.135f; quizTime = 5.0f; break;
    }
}

void SaveMaxStreak(int maxStreak) {
    FILE* f = fopen("assets/flatlogic_maxscore.dat", "w");
    if (f) {
        fprintf(f, "%d", maxStreak);
        fclose(f);
    }
}

void LoadMaxStreak(int* maxStreak) {
    FILE* f = fopen("assets/flatlogic_maxscore.dat", "r");
    if (f) {
        fscanf(f, "%d", maxStreak);
        fclose(f);
    } else {
        FILE* old = fopen("/home/pranay/.flatlogic_maxscore.dat", "r");
        if (old) {
            fscanf(old, "%d", maxStreak);
            fclose(old);
            SaveMaxStreak(*maxStreak);
        }
    }
}

Vector2 GetForwardDir(float rotation) {
  float angle = rotation * DEG2RAD;
  return {sinf(angle), cosf(angle)};
}

void UpdatePlayer(float dt) {
  if (IsKeyUp(KEY_A))
    player.aHeld = false;
  if (IsKeyUp(KEY_D))
    player.dHeld = false;
  if (IsKeyUp(KEY_W))
    player.wHeld = false;
  if (IsKeyUp(KEY_S))
    player.sHeld = false;

  Vector2 dir = GetForwardDir(player.currentRotation);

  if (player.state == MOVING) {
    player.animTimer += dt;
    float currentMoveDuration = (accelerateTimer > 0.0f) ? MOVE_DURATION * 0.5f : MOVE_DURATION;
    float t = fminf(player.animTimer / currentMoveDuration, 1.0f);
    player.gridPos = {player.moveFrom.x + (player.moveTo.x - player.moveFrom.x) * t,
                      player.moveFrom.y + (player.moveTo.y - player.moveFrom.y) * t};

    if (player.animTimer >= currentMoveDuration) {
      player.gridPos = player.moveTo;
      player.state = IDLE;
      player.animTimer = 0.0f;
      if (player.pendingTurn != 0.0f) {
        player.turnFrom = player.currentRotation;
        player.turnTo = player.currentRotation + player.pendingTurn;
        player.pendingTurn = 0.0f;
        player.state = TURNING;
      }
    }
  } else if (player.state == TURNING) {
    player.animTimer += dt;
    float t = fminf(player.animTimer / TURN_DURATION, 1.0f);
    player.currentRotation = player.turnFrom + (player.turnTo - player.turnFrom) * t;

    if (player.animTimer >= TURN_DURATION) {
      player.currentRotation = player.turnTo;
      player.state = IDLE;
      player.animTimer = 0.0f;
    }
  }

  if (!player.aHeld && IsKeyDown(KEY_A)) {
    player.aHeld = true;
    if (player.state == IDLE) {
      player.turnFrom = player.currentRotation;
      player.turnTo = player.currentRotation + 90.0f;
      player.animTimer = 0.0f;
      player.state = TURNING;
    } else if (player.state == MOVING) {
      player.pendingTurn = 90.0f;
    }
  } else if (!player.dHeld && IsKeyDown(KEY_D)) {
    player.dHeld = true;
    if (player.state == IDLE) {
      player.turnFrom = player.currentRotation;
      player.turnTo = player.currentRotation - 90.0f;
      player.animTimer = 0.0f;
      player.state = TURNING;
    } else if (player.state == MOVING) {
      player.pendingTurn = -90.0f;
    }
  } else if (player.state == IDLE && (IsKeyDown(KEY_W) || IsKeyDown(KEY_S))) {
    if (IsKeyDown(KEY_W))
      player.wHeld = true;
    if (IsKeyDown(KEY_S))
      player.sHeld = true;

    int forward = IsKeyDown(KEY_W) ? 1 : -1;
    dir = GetForwardDir(player.currentRotation);
    int nx = (int)(player.gridPos.x + dir.x * forward);
    int ny = (int)(player.gridPos.y + dir.y * forward);

    if (IsValidMove(nx, ny)) {
      player.moveFrom = player.gridPos;
      player.moveTo = {(float)nx, (float)ny};
      player.animTimer = 0.0f;
      player.state = MOVING;
    }
  }
}

float EnemyAnimTimer = 0.0f;

void UpdateEnemies(float dt) {
  EnemyAnimTimer += dt;
  enemySwitchTimer += dt;
  if (enemySwitchTimer >= 1.0f) {
    enemySwitchTimer = 0.0f;
    if (weakModeTimer > 0.0f) {
      enemyVisualWeak = !enemyVisualWeak;
    } else {
      enemyVisualWeak = false;
    }
  }

  if (freezeTimer > 0.0f)
    return;

  for (auto &e : enemies) {
    if (e.isMoving) {
      e.animTimer += dt;
      float t = fminf(e.animTimer / MOVE_DURATION, 1.0f);
      t = Smoothstep(t);

      e.gridPos = {e.moveFrom.x + (e.moveTo.x - e.moveFrom.x) * t, e.moveFrom.y + (e.moveTo.y - e.moveFrom.y) * t};

      if (e.animTimer >= MOVE_DURATION) {
        e.gridPos = e.moveTo;
        e.isMoving = false;
      }
    }
  }

    if (EnemyAnimTimer >= enemyMoveInterval) {
    EnemyAnimTimer = 0.0f;

    for (auto &e : enemies) {
      if (!e.isMoving) {
        Vector2 dirs[] = {{0, -1}, {0, 1}, {-1, 0}, {1, 0}};

        struct Candidate {
          Vector2 pos;
          float dist;
        };
        std::vector<Candidate> candidates;

        for (auto &d : dirs) {
          int nx = (int)(e.gridPos.x + d.x);
          int ny = (int)(e.gridPos.y + d.y);

          if (IsValidMove(nx, ny)) {
            float newDist = sqrtf(powf(player.gridPos.x - nx, 2) + powf(player.gridPos.y - ny, 2));
            candidates.push_back({{(float)nx, (float)ny}, newDist});
          }
        }

        if (!candidates.empty()) {
          Candidate best = candidates[0];
          bool useRandom = (rand() % 4) == 0;

          if (useRandom) {
            int idx = rand() % (int)candidates.size();
            best = candidates[idx];
          } else {
            for (auto &c : candidates) {
              float repulsion = 0.0f;
              for (auto &other : enemies) {
                if (&other != &e) {
                  float od = sqrtf(powf(other.gridPos.x - c.pos.x, 2) + powf(other.gridPos.y - c.pos.y, 2));
                  if (od < 2.0f)
                    repulsion += (2.0f - od) * 1.5f;
                }
              }
              float adjustedDist = c.dist + repulsion;
              if (adjustedDist < best.dist + repulsion) {
                best = c;
              }
            }
          }

          bool occupied = false;
          for (auto &other : enemies) {
            if (&other != &e && (int)other.gridPos.x == (int)best.pos.x && (int)other.gridPos.y == (int)best.pos.y) {
              occupied = true;
              break;
            }
          }

          if (!occupied) {
            e.moveFrom = e.gridPos;
            e.moveTo = best.pos;
            e.animTimer = 0.0f;
            e.isMoving = true;
          }
        }
      }
    }
  }
}

void DrawMaze() {
  for (int y = 0; y < GRID_SIZE; y++) {
    for (int x = 0; x < GRID_SIZE; x++) {
      float px = (float)x * CELL_SIZE;
      float pz = (float)y * CELL_SIZE;

      if (grid[y][x] == WALL) {
        Matrix transform = MatrixTranslate(px, WALL_HEIGHT / 2.0f, pz);
        DrawMesh(wallMesh, wallMaterial, transform);
      } else {
        Matrix transform = MatrixTranslate(px, 0.0f, pz);
        DrawMesh(floorMesh, floorMaterial, transform);
        Matrix roofTransform = MatrixTranslate(px, WALL_HEIGHT + 0.05f, pz);
        DrawMesh(roofMesh, roofMaterial, roofTransform);
      }
    }
  }
}

void DrawEnemies(Camera3D cam) {
  Texture2D tex = enemyVisualWeak ? enemyWeakTex : enemyTex;
  for (auto &e : enemies) {
    float bob = sinf(e.animTimer * 5.0f) * 0.1f;
    Vector3 pos = {e.gridPos.x * CELL_SIZE, 0.5f + bob, e.gridPos.y * CELL_SIZE};

    DrawBillboard(cam, tex, pos, 1.0f, WHITE);
  }
}

void DrawStars(Camera3D cam) {
  for (auto &s : stars) {
    if (!s.active)
      continue;
    s.bobTimer += GetFrameTime();
    float bob = sinf(s.bobTimer * 5.0f) * 0.2f;
    Vector3 pos = {s.gridPos.x * CELL_SIZE, 1.0f + bob, s.gridPos.y * CELL_SIZE};

    DrawBillboard(cam, starTex, pos, 0.8f, WHITE);
  }
}

void DrawPowerUps(Camera3D cam) {
  for (auto &p : powerups) {
    if (!p.active)
      continue;
    p.bobTimer += GetFrameTime();
    float bob = sinf(p.bobTimer * 5.0f) * 0.2f;
    Vector3 pos = {p.gridPos.x * CELL_SIZE, 1.0f + bob, p.gridPos.y * CELL_SIZE};

    Texture2D tex;
    switch (p.type) {
    case POWERUP_FREEZE:
      tex = freezeTex;
      break;
    case POWERUP_MUSCLE:
      tex = muscleTex;
      break;
    case POWERUP_ACCELERATE:
      tex = accelerateTex;
      break;
    }
    DrawBillboard(cam, tex, pos, 0.8f, WHITE);
  }
}

void ResetGame() {
    SetDifficultySettings();
    GenerateMaze();
    EnsureConnectivity();
    
    player.gridPos = { (float)(GRID_SIZE/2), (float)(GRID_SIZE/2) };
    player.currentRotation = 0.0f;
    player.state = IDLE;
    
    while (grid[(int)player.gridPos.y][(int)player.gridPos.x] != FLOOR) {
        player.gridPos.x += 1.0f;
        if (player.gridPos.x >= GRID_SIZE) {
            player.gridPos.x = 1.0f;
            player.gridPos.y += 1.0f;
        }
    }
    
    enemies.clear();
    SpawnEnemies();
    SpawnStars();
    SpawnPowerUps();
    weakModeTimer = 0.0f;
    enemyVisualWeak = false;
    freezeTimer = 0.0f;
    muscleTimer = 0.0f;
    accelerateTimer = 0.0f;
}

void InitGame() {
    SetDifficultySettings();
    GenerateMaze();
    EnsureConnectivity();

    player.gridPos = {(float)(GRID_SIZE / 2), (float)(GRID_SIZE / 2)};
    player.currentRotation = 0.0f;
    player.state = IDLE;

    while (grid[(int)player.gridPos.y][(int)player.gridPos.x] != FLOOR) {
        player.gridPos.x += 1.0f;
        if (player.gridPos.x >= GRID_SIZE) {
            player.gridPos.x = 1.0f;
            player.gridPos.y += 1.0f;
        }
    }

    enemies.clear();
    SpawnEnemies();
    SpawnStars();
    SpawnPowerUps();
    enemyVisualWeak = false;
}

int main() {
  srand(time(NULL));

  const int screenWidth = 1280;
  const int screenHeight = 720;

  InitWindow(screenWidth, screenHeight, "MAZE-A-MATICS");
  SetTargetFPS(60);
  SetWindowState(FLAG_WINDOW_RESIZABLE);

  InitAudioDevice();

  eerieStream = LoadAudioStream(22050, 32, 1);

  wallTex = LoadTexture("assets/wall.png");
  floorTex = LoadTexture("assets/floor.png");
  enemyTex = LoadTexture("assets/enemy.png");
  enemyWeakTex = LoadTexture("assets/enemy_weak.png");
  starTex = LoadTexture("assets/star.png");
  roofTex = LoadTexture("assets/roof.png");
  freezeTex = LoadTexture("assets/freeze.png");
  muscleTex = LoadTexture("assets/muscle.png");
  accelerateTex = LoadTexture("assets/accelerate.png");
  bgMusic = LoadSound("assets/audio/bg.mp3");
  deathSound = LoadSound("assets/audio/death.mp3");
  beginSound = LoadSound("assets/audio/begin.mp3");
  reqradSound = LoadSound("assets/audio/reward.mp3");
  SetSoundVolume(bgMusic, 0.8f);

  wallMesh = GenMeshCube(CELL_SIZE, WALL_HEIGHT, CELL_SIZE);
  floorMesh = GenMeshCube(CELL_SIZE, 0.05f, CELL_SIZE);
  roofMesh = GenMeshCube(CELL_SIZE, 0.05f, CELL_SIZE);

  wallMaterial = LoadMaterialDefault();
  SetMaterialTexture(&wallMaterial, MATERIAL_MAP_DIFFUSE, wallTex);

  floorMaterial = LoadMaterialDefault();
  SetMaterialTexture(&floorMaterial, MATERIAL_MAP_DIFFUSE, floorTex);

  roofMaterial = LoadMaterialDefault();
  SetMaterialTexture(&roofMaterial, MATERIAL_MAP_DIFFUSE, roofTex);

  player.gridPos = {(float)(GRID_SIZE / 2), (float)(GRID_SIZE / 2)};
  player.currentRotation = 0.0f;
  player.state = IDLE;
  player.animTimer = 0.0f;

    int gameState = HOMESCREEN;
    bool quitGame = false;
    bool playerCaught = false;
    LoadMaxStreak(&maxStreak);

  while (!WindowShouldClose() && !quitGame) {
    float dt = GetFrameTime();

    if (gameState == HOMESCREEN) {
      BeginDrawing();
      ClearBackground(COLOR_FOG);

      int cx = GetScreenWidth() / 2;
      int cy = GetScreenHeight() / 2;

      DrawText(TextFormat("MAX SCORE: %d", maxStreak), cx - 70, 20, 24, GOLD);
      int titleWidth = MeasureText("MAZE-A-MATICS", 70);
      DrawText("MAZE-A-MATICS", cx - titleWidth/2, cy - 100, 70, GREEN);
      DrawText("Grid Size: 51x51", cx - 120, cy - 20, 30, GRAY);
      DrawText("[WASD] Move    [ESC] Quit", cx - 150, cy + 20, 24, GRAY);

      Rectangle diffBtns[3] = {
        {static_cast<float>(cx - 180), static_cast<float>(cy + 55), 100.0f, 35.0f},
        {static_cast<float>(cx - 35), static_cast<float>(cy + 55), 100.0f, 35.0f},
        {static_cast<float>(cx + 110), static_cast<float>(cy + 55), 100.0f, 35.0f}
      };
      const char* diffLabels[3] = {"EASY", "MEDIUM", "HARD"};
      Difficulty diffValues[3] = {EASY, MEDIUM, HARD};

      for (int i = 0; i < 3; i++) {
        Color btnColor = (currentDifficulty == diffValues[i]) ? GREEN : DARKGREEN;
        Color txtColor = (currentDifficulty == diffValues[i]) ? BLACK : GREEN;
        DrawRectangleRec(diffBtns[i], btnColor);
        int textWidth = MeasureText(diffLabels[i], 20);
        DrawText(diffLabels[i], diffBtns[i].x + (diffBtns[i].width - textWidth) / 2, diffBtns[i].y + 8, 20, txtColor);
        Vector2 mouse = GetMousePosition();
        if (CheckCollisionPointRec(mouse, diffBtns[i]) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
          currentDifficulty = diffValues[i];
        }
      }

      Rectangle playBtn = {static_cast<float>(cx - 80), static_cast<float>(cy + 105), 160.0f, 50.0f};
      DrawRectangleRec(playBtn, DARKGREEN);
      DrawText("PLAY", cx - 35, cy + 115, 30, GREEN);

      Vector2 mouse = GetMousePosition();
      if (CheckCollisionPointRec(mouse, playBtn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        ResetGame();
        gameState = PLAYING;
        deathSoundPlayed = false;
        PlaySound(beginSound);
        PlaySound(bgMusic);
      }

      if (IsKeyPressed(KEY_ESCAPE)) {
        quitGame = true;
      }

      EndDrawing();
      continue;
        }
          
        if (weakModeTimer > 0.0f) {
      weakModeTimer -= GetFrameTime();
      if (weakModeTimer < 0.0f)
        weakModeTimer = 0.0f;
    }

    if (freezeTimer > 0.0f) {
      freezeTimer -= GetFrameTime();
      if (freezeTimer < 0.0f)
        freezeTimer = 0.0f;
    }

    if (muscleTimer > 0.0f) {
      muscleTimer -= GetFrameTime();
      if (muscleTimer < 0.0f)
        muscleTimer = 0.0f;
    }

    if (accelerateTimer > 0.0f) {
      accelerateTimer -= GetFrameTime();
      if (accelerateTimer < 0.0f)
        accelerateTimer = 0.0f;
    }

    int starsCollected = 0;
    for (auto &s : stars) {
      if (s.active) {
        if ((int)s.gridPos.x == (int)player.gridPos.x && (int)s.gridPos.y == (int)player.gridPos.y) {
                    s.active = false;
                    weakModeTimer = 5.0f;
                    PlaySound(reqradSound);
                    quizNum1 = 1 + rand() % 9;
                    quizNum2 = 1 + rand() % 9;
                    quizCorrectAnswer = quizNum1 * quizNum2;
                    quizInput[0] = 0;
                    quizInputLen = 0;
                    quizActive = true;
                    quizFreezeTimer = quizTime;
        }
      }
      if (!s.active)
        starsCollected++;
    }

    for (auto &p : powerups) {
      if (p.active && p.type >= POWERUP_FREEZE && p.type <= POWERUP_ACCELERATE) {
        if ((int)p.gridPos.x == (int)player.gridPos.x && (int)p.gridPos.y == (int)player.gridPos.y) {
          p.active = false;
          switch (p.type) {
          case POWERUP_FREEZE:
            freezeTimer = 10.0f;
            break;
          case POWERUP_MUSCLE:
            muscleTimer = 10.0f;
            break;
          case POWERUP_ACCELERATE:
            accelerateTimer = 10.0f;
            break;
          }
          PlaySound(reqradSound);
        }
      }
    }

    for (auto &e : enemies) {
      if ((int)e.gridPos.x == (int)player.gridPos.x && (int)e.gridPos.y == (int)player.gridPos.y) {
        if (weakModeTimer > 0.0f) {
          e = enemies.back();
          enemies.pop_back();
        } else {
          playerCaught = true;
        }
        break;
      }
    }

    bool victory = (starsCollected == STAR_COUNT);

    float enemyDist = 1000.0f;
    for (auto &e : enemies) {
      float d = sqrtf(powf(e.gridPos.x - player.gridPos.x, 2) + powf(e.gridPos.y - player.gridPos.y, 2));
      if (d < enemyDist)
        enemyDist = d;
    }
    float dangerTint = (enemyDist < 10.0f) ? (1.0f - enemyDist / 10.0f) * 30.0f : 0.0f;

    if (enemyDist < 10.0f && !IsSoundPlaying(bgMusic)) {
      PlaySound(bgMusic);
    } else if (enemyDist >= 10.0f && IsSoundPlaying(bgMusic)) {
      StopSound(bgMusic);
    }

    if (enemyDist < 10.0f && !eeriePlaying) {
      eeriePlaying = true;
      PlayAudioStream(eerieStream);
    } else if (enemyDist >= 10.0f && eeriePlaying) {
      eeriePlaying = false;
      StopAudioStream(eerieStream);
    }
    UpdateEerieAudio(eerieStream, dt);

    if (quizActive) {
      quizFreezeTimer -= GetFrameTime();
      if (quizFreezeTimer <= 0.0f) {
        quizFreezeTimer = 0.0f;
        quizActive = false;
        playerCaught = true;
      }
    }

    if (!playerCaught && !victory && !quizActive) {
      UpdatePlayer(dt);
      UpdateEnemies(dt);
    }

    Camera3D cam = {0};
    cam.position = {player.gridPos.x * CELL_SIZE, CAMERA_HEIGHT, player.gridPos.y * CELL_SIZE};
    cam.target = {cam.position.x + sinf(player.currentRotation * DEG2RAD), CAMERA_HEIGHT,
                  cam.position.z + cosf(player.currentRotation * DEG2RAD)};
    cam.up = {0.0f, 1.0f, 0.0f};
    cam.fovy = 60.0f;
    cam.projection = CAMERA_PERSPECTIVE;

    BeginDrawing();
    if (dangerTint > 0.0f) {
      ClearBackground(Color{(unsigned char)(COLOR_FOG.r + dangerTint), COLOR_FOG.g,
                            (unsigned char)(COLOR_FOG.b + dangerTint), 255});
    } else {
      ClearBackground(COLOR_FOG);
    }

    BeginMode3D(cam);

    DrawMaze();
    DrawEnemies(cam);
    DrawStars(cam);
    DrawPowerUps(cam);

    EndMode3D();

    DrawText(TextFormat("LEVEL %d", currentStreak + 1), screenWidth / 2 - 50, 20, 30, WHITE);
    DrawText("MAZE-A-MATICS", 20, 20, 30, GREEN);
    DrawText(TextFormat("Stars: %d/%d", starsCollected, STAR_COUNT), 20, 60, 20, YELLOW);
    DrawText(TextFormat("Enemies: %d", (int)enemies.size()), 20, 90, 20, GRAY);

        int powerY = 120;
        if (weakModeTimer > 0.0f) {
            DrawText(TextFormat("WEAKNESS: %.0fs", weakModeTimer), 20, powerY, 18, PINK);
            powerY += 25;
        }
        if (freezeTimer > 0.0f) {
      DrawText(TextFormat("FREEZE: %.0fs", freezeTimer), 20, powerY, 18, SKYBLUE);
      powerY += 25;
    }
    if (muscleTimer > 0.0f) {
      DrawText(TextFormat("MUSCLE: %.0fs", muscleTimer), 20, powerY, 18, ORANGE);
      powerY += 25;
    }
    if (accelerateTimer > 0.0f) {
      DrawText(TextFormat("ACCELERATE: %.0fs", accelerateTimer), 20, powerY, 18, GREEN);
    }

    if (quizActive) {
      DrawRectangle(0, 0, screenWidth, screenHeight, Color{0, 0, 0, 200});

      int boxW = 400, boxH = 250;
      int boxX = screenWidth / 2 - boxW / 2, boxY = screenHeight / 2 - boxH / 2;
      DrawRectangle(boxX, boxY, boxW, boxH, Color{30, 30, 30, 255});
      DrawRectangleLines(boxX, boxY, boxW, boxH, GREEN);

      DrawText("STAR COLLECTED!", boxX + 80, boxY + 30, 24, YELLOW);
      DrawText(TextFormat("Solve: %d x %d = ?", quizNum1, quizNum2), boxX + 110, boxY + 80, 24, WHITE);
      DrawText("Your answer:", boxX + 60, boxY + 130, 20, GRAY);
      DrawRectangle(boxX + 160, boxY + 125, 120, 30, BLACK);
      DrawRectangleLines(boxX + 160, boxY + 125, 120, 30, GREEN);
      DrawText(quizInput[0] ? quizInput : "_", boxX + 170, boxY + 130, 20, WHITE);
      DrawText("Press ENTER to submit", boxX + 100, boxY + 180, 18, GRAY);
      DrawText(TextFormat("Time left: %.1fs", quizFreezeTimer), boxX + 130, boxY + 210, 18, ORANGE);

      int key = GetCharPressed();
      while (key > 0) {
        if (key >= 48 && key <= 57 && quizInputLen < 15) {
          quizInput[quizInputLen++] = (char)key;
          quizInput[quizInputLen] = 0;
        }
        key = GetCharPressed();
      }
      if (IsKeyPressed(KEY_BACKSPACE) && quizInputLen > 0) {
        quizInput[--quizInputLen] = 0;
      }
      if (IsKeyPressed(KEY_ENTER) && quizInputLen > 0) {
        int answer = atoi(quizInput);
        if (answer == quizCorrectAnswer) {
          quizActive = false;
        } else {
          playerCaught = true;
          quizActive = false;
        }
      }
    }
    if (playerCaught) {
      DrawRectangle(0, 0, screenWidth, screenHeight, Color{50, 0, 0, 220});

      DrawText("GAME OVER", screenWidth / 2 - 120, screenHeight / 2 - 60, 50, RED);
      DrawText("[R] Restart  [ESC] Quit", screenWidth / 2 - 130, screenHeight / 2 + 20, 24, GRAY);

      static float deathTurnTimer = 0.0f;
      static float deathTurnFrom = 0.0f;
      static float deathTurnTo = 0.0f;

      if (!deathSoundPlayed) {
        PlaySound(deathSound);
        deathSoundPlayed = true;
        deathTurnTimer = 0.0f;
        deathTurnFrom = player.currentRotation;

        float nearestDist = 1000.0f;
        for (auto &e : enemies) {
          float d = sqrtf(powf(e.gridPos.x - player.gridPos.x, 2) + powf(e.gridPos.y - player.gridPos.y, 2));
          if (d < nearestDist) {
            nearestDist = d;
            deathTurnTo = atan2f(e.gridPos.x - player.gridPos.x, e.gridPos.y - player.gridPos.y) * RAD2DEG;
          }
        }
      }

      deathTurnTimer += GetFrameTime();
      if (deathTurnTimer < 0.1f) {
        float t = deathTurnTimer / 0.1f;
        player.currentRotation = deathTurnFrom + (deathTurnTo - deathTurnFrom) * t;
      }

            if (IsKeyPressed(KEY_R)) {
                currentStreak = 0;
                ResetGame();
                gameState = PLAYING;
                playerCaught = false;
                deathSoundPlayed = false;
                PlaySound(beginSound);
            }

      if (IsKeyPressed(KEY_ESCAPE)) {
        quitGame = true;
      }
    } else if (victory) {
      DrawRectangle(0, 0, screenWidth, screenHeight, Color{0, 50, 0, 220});
      DrawText("VICTORY!", screenWidth / 2 - 120, screenHeight / 2 - 60, 50, GREEN);
      DrawText("[R] Play Again  [ESC] Quit", screenWidth / 2 - 150, screenHeight / 2 + 20, 24, GRAY);

      if (IsKeyPressed(KEY_R)) {
        currentStreak++;
        if (currentStreak > maxStreak) {
            maxStreak = currentStreak;
            SaveMaxStreak(maxStreak);
        }
        ResetGame();
        gameState = PLAYING;
      }

      if (IsKeyPressed(KEY_ESCAPE)) {
        quitGame = true;
      }
    }

    EndDrawing();

    if (playerCaught) {
      if (IsKeyPressed(KEY_ESCAPE)) {
        quitGame = true;
      }
    }
  }

  UnloadTexture(wallTex);
  UnloadTexture(floorTex);
  UnloadTexture(enemyTex);
  UnloadTexture(enemyWeakTex);
  UnloadTexture(starTex);
  UnloadTexture(roofTex);
  UnloadTexture(freezeTex);
  UnloadTexture(muscleTex);
  UnloadTexture(accelerateTex);
  UnloadMesh(wallMesh);
  UnloadMesh(floorMesh);
  UnloadMesh(roofMesh);
  UnloadMaterial(wallMaterial);
  UnloadMaterial(floorMaterial);
  UnloadMaterial(roofMaterial);
  UnloadSound(bgMusic);
  UnloadSound(deathSound);
  UnloadSound(beginSound);
  UnloadSound(reqradSound);
  UnloadAudioStream(eerieStream);
  SaveMaxStreak(maxStreak);
  CloseAudioDevice();
  CloseWindow();

  return 0;
}
