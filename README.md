# Metro X Theoretical Max Score Optimizer

A high-performance C++20 solver that computes the theoretical maximum score for any **Metro X** subway map given a fixed sequence of turn cards and other players' route completion timings.

---

## Features

- **Optimal Move Discovery:** Combines **Beam Search** (warm-start) with **Depth-First Branch and Bound (DFBnB)** and bitboard states to find the proven mathematical optimum in seconds.
- **Opponent Timing Competition:** Takes opponent finish turns per route into account. If you complete a route on or before the opponent's completion turn, you receive the **Gold Diamond** bonus; otherwise you receive the **Silver/White Square** bonus.
- **Full Rules Fidelity:** Accurately models Number cards, Skip cards, Transfer cards ($2 \times \text{degree}$), Free Space cards, route stop rules, train car window limits, and the official empty station penalty table.
- **Human-Readable Playbook:** Outputs a turn-by-turn log showing which route to play, which stations to cross off or transfer, and when route completion bonuses are triggered.

---

## File Format (`scenarios/*.txt`)

Each scenario file contains three sections:

```text
[MAP]
# RouteName | Windows | GoldBonus | SilverBonus | StationIDs (ordered Start -> End)
LineA | 2 | 6 | 3 | S1 S2 S3 S4 S10 S5
LineB | 3 | 8 | 4 | S6 S3 S7 S8 S9
...

[SETTINGS]
# Optional. Default is true (underfilling allowed).
# Set to false to force using all available squares unless blocked.
allow_short_use: true

[OPPONENTS]
# RouteName : turn_completed (1-indexed)
# If omitted or unlisted, defaults to None (Gold is always available)
LineA : 8
LineC : 11
LineE : 14

[TURNS]
# Space-separated transit cards in drawn order:
# Numbers: 2, 3, 4, 5, 6
# Skips: S2, S3
# Transfers: T or TRANSFER
# Free Space: FREE
3 4 S2 T 5 FREE 6 2 S3 T 4 3 5 2 4 3 2 4 3
```

---

## How to Build & Run

### Compilation
Using `g++` (C++20):
```bash
g++ -O3 -std=c++20 -I src src/main.cpp -o metrox_solver.exe
```

### Running a Scenario (CLI)
```bash
# Default mode (allows voluntary underfilling if beneficial)
.\metrox_solver.exe scenarios/sample_game.txt

# Strict mode: forces maximal fill (disallows voluntary short uses)
.\metrox_solver.exe scenarios/sample_game.txt --strict
```

---

## Interactive Web UI Studio

A web-based interface is included for entering game states, clicking transit cards into a queue, adjusting opponent deadlines, and viewing the optimal playbook.

To start the studio:
```bash
python server.py
```
Then open your browser to **`http://localhost:8000`**.
