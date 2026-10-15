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

## Interactive Web UI Studio & Client-Side Engine

A web-based interface and standalone JavaScript solver are included in [`ui/`](ui/). It runs **100% in the browser** with zero dependencies, making it ready for **GitHub Pages (`github.io`)** or embedding into any other site.

### Running Locally (Zero Server Needed)
You can directly double-click or open [`ui/index.html`](ui/index.html) in any web browser, or serve it with Python:
```bash
python server.py
```
Then visit **`http://localhost:8000`**.

---

## Importing into Another Repository

You can easily integrate this project into your main website or portfolio repository in two ways:

### Option A: Import as a Git Submodule
If you want to keep this repo as a submodule inside another repository:
```bash
git submodule add https://github.com/<your-username>/metrox.git metrox
```
Then embed the UI via an `<iframe>` or link to `metrox/ui/index.html`:
```html
<iframe src="metrox/ui/index.html" class="w-full h-[800px] border-none"></iframe>
```

### Option B: Use the Pure JavaScript Solver Library (`ui/metrox_engine.js`)
If you have an existing React, Vue, Next.js, or vanilla JS site in another repo, copy [`ui/metrox_engine.js`](ui/metrox_engine.js). It has zero dependencies and works with both ES Modules and standard `<script>` tags:

```javascript
// Via script tag:
// <script src="metrox_engine.js"></script>

const scenario = `
[MAP]
LineA | 2 | 6 | 3 | S1 S2 S3 S4
LineB | 2 | 6 | 3 | S5 S2 S6

[TURNS]
3 4
`;

// Instant client-side solve (< 50ms):
const solution = MetroXEngine.solveMetroX(scenario, { allow_short_use: true });
console.log("Max Score:", solution.total_score);
console.log("Playbook:", solution.playbook);
```
