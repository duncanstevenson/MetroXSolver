/**
 * Metro X Optimizer - JavaScript Automated Test Suite
 * Compatible with Node.js and Browser environments.
 */

(function (root) {
  let engine = null;
  if (typeof require !== 'undefined') {
    engine = require('../ui/metrox_engine.js');
  } else if (typeof root.MetroXEngine !== 'undefined') {
    engine = root.MetroXEngine;
  } else {
    throw new Error("MetroXEngine not found in environment.");
  }

  let testsRun = 0;
  let testsPassed = 0;
  const results = [];

  function assertEqual(actual, expected, msg) {
    if (actual !== expected) {
      throw new Error((msg ? msg + ": " : "") + `Expected ${expected}, but got ${actual}`);
    }
  }

  function assertTrue(cond, msg) {
    if (!cond) {
      throw new Error(msg || "Assertion failed: expected true, got false");
    }
  }

  function test(name, fn) {
    testsRun++;
    try {
      fn();
      testsPassed++;
      results.push({ name, status: 'PASSED' });
    } catch (err) {
      results.push({ name, status: 'FAILED', error: err.message });
    }
  }

  // Test 1: Penalty table
  test("test_penalty_table", () => {
    for (let i = 0; i <= 4; ++i) {
      assertEqual(engine.getEmptyStationsPenalty(i), 0);
    }
    assertEqual(engine.getEmptyStationsPenalty(5), -1);
    assertEqual(engine.getEmptyStationsPenalty(6), -2);
    assertEqual(engine.getEmptyStationsPenalty(7), -3);
    assertEqual(engine.getEmptyStationsPenalty(8), -4);
    assertEqual(engine.getEmptyStationsPenalty(9), -4);
    assertEqual(engine.getEmptyStationsPenalty(10), -5);
    assertEqual(engine.getEmptyStationsPenalty(11), -5);
    assertEqual(engine.getEmptyStationsPenalty(12), -6);
    assertEqual(engine.getEmptyStationsPenalty(13), -6);
    assertEqual(engine.getEmptyStationsPenalty(14), -7);
    assertEqual(engine.getEmptyStationsPenalty(15), -7);
    assertEqual(engine.getEmptyStationsPenalty(16), -8);
    assertEqual(engine.getEmptyStationsPenalty(17), -8);
    assertEqual(engine.getEmptyStationsPenalty(18), -9);
    assertEqual(engine.getEmptyStationsPenalty(19), -9);
    assertEqual(engine.getEmptyStationsPenalty(20), -10);
    assertEqual(engine.getEmptyStationsPenalty(50), -10);
  });

  // Test 2: Card parsing
  test("test_card_parsing", () => {
    const c1 = engine.parseCardToken("3");
    assertEqual(c1.type, "NUMBER");
    assertEqual(c1.value, 3);

    const c2 = engine.parseCardToken("S2");
    assertEqual(c2.type, "SKIP");
    assertEqual(c2.value, 2);

    const c3 = engine.parseCardToken("SKIP4");
    assertEqual(c3.type, "SKIP");
    assertEqual(c3.value, 4);

    const c4 = engine.parseCardToken("TRANSFER");
    assertEqual(c4.type, "TRANSFER");

    const c5 = engine.parseCardToken("T");
    assertEqual(c5.type, "TRANSFER");

    const c6 = engine.parseCardToken("FREE");
    assertEqual(c6.type, "FREE");

    const c7 = engine.parseCardToken("F");
    assertEqual(c7.type, "FREE");
  });

  // Test 3: Scenario parser
  test("test_scenario_parser", () => {
    const text = `
[MAP]
LineA | 2 | 6 | 3 | S1 S2 S3
LineB | 1 | 4 | 2 | S2 S4

[SETTINGS]
allow_short_use: false

[OPPONENTS]
LineA : 3

[TURNS]
3 S2 T FREE
`;
    const sc = engine.parseScenario(text);
    assertEqual(sc.map.routes.length, 2);
    assertEqual(sc.map.total_stations, 4);
    assertEqual(sc.settings.allow_short_use, false);
    assertEqual(sc.turns.length, 4);
    assertEqual(sc.opponent_finish_turns[0], 3);
    assertEqual(sc.opponent_finish_turns[1], 999999);

    const s2Id = sc.map.station_names.indexOf("S2");
    assertTrue(s2Id >= 0);
    assertEqual(sc.map.station_degrees[s2Id], 2);
    assertEqual(sc.map.station_transfer_pts[s2Id], 4);
  });

  // Test 4: Move generation for Number card (stops when blocked by marked station)
  test("test_number_card_move_generation", () => {
    const text = `
[MAP]
LineA | 2 | 5 | 2 | S1 S2 S3 S4

[TURNS]
3
`;
    const sc = engine.parseScenario(text);
    const solver = new engine.MetroXSolver(sc);

    const state = {
      stations_marked: 0n,
      completed_routes_mask: 0,
      windows_used: new Array(sc.map.routes.length).fill(0),
      transfer_score: 0,
      route_score: 0
    };

    const moves = solver.generateMoves(state, 0);
    assertTrue(moves.length > 0);
    assertEqual(moves[0].marked_stations.length, 3); // Marks S1, S2, S3

    // When S2 (index 1) is already marked:
    state.stations_marked = 1n << 1n; // S2 marked
    const blockedMoves = solver.generateMoves(state, 0);
    assertTrue(blockedMoves.length > 0);
    assertEqual(blockedMoves[0].marked_stations.length, 1); // Only marks S1 then stops
    assertEqual(blockedMoves[0].marked_stations[0], 0);
  });

  // Test 5: Move generation for Skip card (skips over marked stations)
  test("test_skip_card_move_generation", () => {
    const text = `
[MAP]
LineA | 2 | 5 | 2 | S1 S2 S3 S4

[TURNS]
S2
`;
    const sc = engine.parseScenario(text);
    const solver = new engine.MetroXSolver(sc);

    const state = {
      stations_marked: 1n << 1n, // S2 marked
      completed_routes_mask: 0,
      windows_used: new Array(sc.map.routes.length).fill(0),
      transfer_score: 0,
      route_score: 0
    };

    const moves = solver.generateMoves(state, 0);
    assertTrue(moves.length > 0);
    assertEqual(moves[0].marked_stations.length, 2);
    assertEqual(moves[0].marked_stations[0], 0); // S1
    assertEqual(moves[0].marked_stations[1], 2); // S3 (jumped over S2!)
  });

  // Test 6: Transfer card mechanics
  test("test_transfer_card_mechanics", () => {
    const text = `
[MAP]
LineA | 2 | 5 | 2 | S1 S2 S3
LineB | 1 | 5 | 2 | S2 S4

[TURNS]
TRANSFER
`;
    const sc = engine.parseScenario(text);
    const solver = new engine.MetroXSolver(sc);

    const state = {
      stations_marked: 1n << 0n, // S1 marked
      completed_routes_mask: 0,
      windows_used: new Array(sc.map.routes.length).fill(0),
      transfer_score: 0,
      route_score: 0
    };

    const moves = solver.generateMoves(state, 0);
    const s2Transfer = moves.find(m => m.route_idx === 0 && m.is_transfer && m.transfer_points === 4);
    assertTrue(!!s2Transfer, "Must generate transfer move for S2 with 4 points");
  });

  // Test 7: Window exhaustion prevents route usage
  test("test_window_exhaustion", () => {
    const text = `
[MAP]
LineA | 1 | 5 | 2 | S1 S2
LineB | 2 | 5 | 2 | S3 S4

[TURNS]
2
`;
    const sc = engine.parseScenario(text);
    const solver = new engine.MetroXSolver(sc);

    const state = {
      stations_marked: 0n,
      completed_routes_mask: 0,
      windows_used: [1, 0], // LineA exhausted
      transfer_score: 0,
      route_score: 0
    };

    const moves = solver.generateMoves(state, 0);
    for (const m of moves) {
      assertTrue(m.route_idx !== 0, "LineA should not be played when windows exhausted");
    }
  });

  // Test 8: Free Space card marks any empty station
  test("test_free_space_card", () => {
    const text = `
[MAP]
LineA | 1 | 5 | 2 | S1 S2 S3

[TURNS]
FREE
`;
    const sc = engine.parseScenario(text);
    const solver = new engine.MetroXSolver(sc);

    const state = {
      stations_marked: 0n,
      completed_routes_mask: 0,
      windows_used: [1], // LineA windows already exhausted
      transfer_score: 0,
      route_score: 0
    };

    const moves = solver.generateMoves(state, 0);
    assertTrue(moves.length > 0, "Free card should generate moves even when route windows are exhausted");
    assertEqual(moves[0].route_idx, -1);
  });

  // Test 9: Route completion timing (Gold vs Silver)
  test("test_route_completion_timing", () => {
    const textGold = `
[MAP]
LineA | 2 | 6 | 2 | S1 S2

[OPPONENTS]
LineA : 1

[TURNS]
2
`;
    const solGold = engine.solveMetroX(textGold);
    assertEqual(solGold.route_score, 6, "Should award Gold Diamond (+6) when on/before opponent finish");

    const textSilver = `
[MAP]
LineA | 2 | 6 | 2 | S1 S2

[OPPONENTS]
LineA : 0

[TURNS]
2
`;
    const solSilver = engine.solveMetroX(textSilver);
    assertEqual(solSilver.route_score, 2, "Should award Silver Square (+2) when after opponent finish");
  });

  // Test 10: Dead-route upper-bound pruning accounts for Free cards
  test("test_upper_bound_pruning_with_free_cards", () => {
    const text = `
[MAP]
LineA | 1 | 6 | 2 | S1 S2 S3

[TURNS]
2 FREE
`;
    const sc = engine.parseScenario(text);
    const solver = new engine.MetroXSolver(sc);

    // At turn 1 (FREE card remaining):
    const state = {
      stations_marked: (1n << 0n) | (1n << 1n), // S1, S2 marked
      completed_routes_mask: 0,
      windows_used: [1], // LineA window exhausted
      transfer_score: 0,
      route_score: 0
    };

    const ub = solver.computeUpperBound(state, 1);
    assertTrue(ub >= 6, "UB must include LineA bonus (+6) because FREE card can finish it");
  });

  // Test 11: End-to-end full solve (matches C++ output)
  test("test_end_to_end_solver_parity", () => {
    const text = `
[MAP]
LineA | 2 | 6 | 3 | S1 S2 S3 S4
LineB | 2 | 4 | 2 | S4 S5 S6

[TURNS]
3 2 3
`;
    const sol = engine.solveMetroX(text);
    assertEqual(sol.total_score, 10, "Total score must equal 10 (matching C++ solver)");
    assertEqual(sol.playbook.length, 3);
  });

  function runAll() {
    const outputLines = [
      "========================================",
      "     METRO X JAVASCRIPT TEST SUITE      ",
      "========================================"
    ];

    results.forEach((r, idx) => {
      if (r.status === 'PASSED') {
        outputLines.push(`[TEST ${idx + 1}] ${r.name}... PASSED`);
      } else {
        outputLines.push(`[TEST ${idx + 1}] ${r.name}... FAILED (${r.error})`);
      }
    });

    outputLines.push("========================================");
    outputLines.push(`Tests Passed: ${testsPassed} / ${testsRun}`);
    outputLines.push("========================================");

    return {
      total: testsRun,
      passed: testsPassed,
      failed: testsRun - testsPassed,
      results,
      output: outputLines.join('\n')
    };
  }

  // Node.js execution
  if (typeof module !== 'undefined' && module.exports) {
    const res = runAll();
    console.log(res.output);
    if (res.failed > 0) {
      process.exit(1);
    }
  }

  // Browser execution
  if (typeof window !== 'undefined') {
    window.MetroXTests = {
      runAll
    };
  }

})(typeof globalThis !== 'undefined' ? globalThis : this);
