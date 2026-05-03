# maze-a-matics

A 3D first-person dungeon crawler built with Raylib featuring math quizzes and procedural mazes.

## Features
- 51×51 procedural maze generation using Binary Space Partitioning
- 3 difficulty levels (Easy/Medium/Hard) with different enemy counts and speeds
- Star collection with multiplication quiz mini-game
- Enemy AI with player tracking and enemy repulsion
- Power-ups: Freeze (stop enemies), Muscle (break walls), Accelerate (move faster)
- Score streak tracking with persistent high score
- Procedural eerie ambient audio
- Raylib 3D rendering with billboard sprites

## Building

### Prerequisites
- CMake 3.10+
- Raylib

On macOS with Homebrew:
```bash
brew install cmake raylib
```

### Manual Compilation

Linux with Raylib installed at `$HOME/raylib-install`:
```bash
g++ -o maze-a-matics main.cpp -I$HOME/raylib-install/include -L$HOME/raylib-install/lib \
   -lraylib -lGL -lm -lpthread -ldl -lrt -lX11 -Wl,-rpath,$HOME/raylib-install/lib
```

### CMake
```bash
mkdir build && cd build
cmake ..
make
cd ..
./maze-a-matics
```

## Controls
- **WASD**: Move forward/backward, Turn left/right
- **ESC**: Quit game
- **R**: Restart (on Game Over / Victory)

## Gameplay
- Navigate the maze and collect all 5 stars
- Each star triggers a math quiz (single-digit multiplication)
- Answer correctly within the time limit to continue
- Wrong answer or timeout = Game Over
- Collect power-ups to gain advantages
- When enemies are nearby, eerie music plays
- Build your streak by clearing consecutive levels!

## Difficulty Levels

| Difficulty | Enemies | Enemy Speed | Quiz Time |
|------------|---------|-------------|-----------|
| Easy | 3 | 0.30s | 10s |
| Medium | 5 | 0.15s | 7s |
| Hard | 10 | 0.135s (10% faster) | 5s |

## License
MIT
# maze-a-matics
# maze-a-matics
# hehe
