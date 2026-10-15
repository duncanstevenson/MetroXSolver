/**
 * Metro X Optimizer Engine (Pure JavaScript / ES Module & Browser)
 * Zero external dependencies. Uses BigInt for high-performance bitboard search.
 */

(function (root, factory) {
  if (typeof define === 'function' && define.amd) {
    define([], factory);
  } else if (typeof module === 'object' && module.exports) {
    module.exports = factory();
  } else {
    root.MetroXEngine = factory();
  }
}(typeof self !== 'undefined' ? self : this, function () {

  // Official Empty Stations Penalty Table
  function getEmptyStationsPenalty(emptyCount) {
    if (emptyCount <= 4) return 0;
    if (emptyCount === 5) return -1;
    if (emptyCount === 6) return -2;
    if (emptyCount === 7) return -3;
    if (emptyCount <= 9) return -4;
    if (emptyCount <= 11) return -5;
    if (emptyCount <= 13) return -6;
    if (emptyCount <= 15) return -7;
    if (emptyCount <= 17) return -8;
    if (emptyCount <= 19) return -9;
    return -10;
  }

  function parseCardToken(token) {
    const upper = token.trim().toUpperCase();
    if (upper === 'T' || upper === 'TRANSFER' || upper === 'X') {
      return { type: 'TRANSFER', value: 0 };
    }
    if (upper === 'FREE' || upper === 'F') {
      return { type: 'FREE', value: 0 };
    }
    if (upper.startsWith('S') || upper.startsWith('SKIP')) {
      const valStr = upper.startsWith('SKIP') ? upper.slice(4) : upper.slice(1);
      return { type: 'SKIP', value: parseInt(valStr, 10) || 0 };
    }
    return { type: 'NUMBER', value: parseInt(upper, 10) || 0 };
  }

  function cardToString(card) {
    if (card.type === 'NUMBER') return String(card.value);
    if (card.type === 'SKIP') return 'S' + card.value;
    if (card.type === 'TRANSFER') return 'TRANSFER';
    if (card.type === 'FREE') return 'FREE';
    return '?';
  }

  function parseScenario(scenarioText) {
    const lines = scenarioText.split('\n');
    let currentSection = '';

    const rawRoutes = [];
    const oppFinishMap = {};
    const turns = [];
    let allowShortUse = true;

    for (let rawLine of lines) {
      const line = rawLine.trim();
      if (!line || line.startsWith('#') || line.startsWith('//')) continue;

      if (line.startsWith('[') && line.endsWith(']')) {
        currentSection = line.slice(1, -1).trim().toUpperCase();
        continue;
      }

      if (currentSection === 'MAP') {
        const parts = line.split('|').map(s => s.trim());
        if (parts.length >= 5) {
          rawRoutes.push({
            name: parts[0],
            windows: parseInt(parts[1], 10),
            gold: parseInt(parts[2], 10),
            silver: parseInt(parts[3], 10),
            stations: parts[4].split(/\s+/)
          });
        }
      } else if (currentSection === 'OPPONENTS') {
        const parts = line.split(':').map(s => s.trim());
        if (parts.length >= 2) {
          oppFinishMap[parts[0]] = parseInt(parts[1], 10);
        }
      } else if (currentSection === 'SETTINGS') {
        let parts = line.split(':');
        if (parts.length < 2) parts = line.split('=');
        if (parts.length >= 2) {
          const k = parts[0].trim().toLowerCase();
          const v = parts[1].trim().toLowerCase();
          if (k === 'allow_short_use' || k === 'allow_underfill') {
            allowShortUse = (v === 'true' || v === '1' || v === 'yes');
          } else if (k === 'disable_short_use' || k === 'strict_fill' || k === 'force_full_fill') {
            allowShortUse = !(v === 'true' || v === '1' || v === 'yes');
          }
        }
      } else if (currentSection === 'TURNS') {
        const tokens = line.split(/\s+/);
        for (const tok of tokens) {
          if (tok) turns.push(parseCardToken(tok));
        }
      }
    }

    // Index unique stations
    const stationNameToId = new Map();
    const stationNames = [];
    for (const rr of rawRoutes) {
      for (const sname of rr.stations) {
        if (!stationNameToId.has(sname)) {
          const id = stationNames.length;
          stationNameToId.set(sname, id);
          stationNames.push(sname);
        }
      }
    }

    const totalStations = stationNames.length;
    const stationDegrees = new Array(totalStations).fill(0);
    const routes = [];
    const routeNameToIdx = new Map();

    for (let i = 0; i < rawRoutes.length; ++i) {
      const rr = rawRoutes[i];
      let mask = 0n;
      const sIds = [];

      for (const sname of rr.stations) {
        const sid = stationNameToId.get(sname);
        sIds.push(sid);
        mask |= (1n << BigInt(sid));
        stationDegrees[sid]++;
      }

      routes.push({
        id: i,
        name: rr.name,
        max_windows: rr.windows,
        gold_bonus: rr.gold,
        silver_bonus: rr.silver,
        stations: sIds,
        mask: mask
      });
      routeNameToIdx.set(rr.name, i);
    }

    const stationTransferPts = stationDegrees.map(deg => 2 * deg);
    const opponentFinishTurns = new Array(routes.length).fill(999999);
    for (const [rname, turnVal] of Object.entries(oppFinishMap)) {
      if (routeNameToIdx.has(rname)) {
        opponentFinishTurns[routeNameToIdx.get(rname)] = turnVal;
      }
    }

    return {
      map: {
        routes,
        total_stations: totalStations,
        station_names: stationNames,
        station_degrees: stationDegrees,
        station_transfer_pts: stationTransferPts
      },
      turns,
      opponent_finish_turns: opponentFinishTurns,
      settings: {
        allow_short_use: allowShortUse
      }
    };
  }

  class MetroXSolver {
    constructor(scenario) {
      this.scenario = scenario;
      this.map = scenario.map;
      this.settings = scenario.settings;

      const T = this.scenario.turns.length;
      this.remainingTransfers = new Array(T + 1).fill(0);
      for (let i = T - 1; i >= 0; --i) {
        this.remainingTransfers[i] = this.remainingTransfers[i + 1] + 
          (this.scenario.turns[i].type === 'TRANSFER' ? 1 : 0);
      }

      this.bestScore = -9999;
      this.bestPlaybook = [];
      this.bestBreakdown = {
        route_score: 0,
        transfer_score: 0,
        empty_stations: 0,
        penalty: 0
      };
      this.statesEvaluated = 0;
      this.transpositionTable = new Map();
    }

    countBits(n) {
      let count = 0;
      let temp = n;
      while (temp > 0n) {
        temp &= (temp - 1n);
        count++;
      }
      return count;
    }

    computeUpperBound(state, turn) {
      let ub = state.route_score + state.transfer_score;
      const turn1Based = turn + 1;

      // 1. Incomplete route bonuses (pruning impossible routes)
      for (let i = 0; i < this.map.routes.length; ++i) {
        if (!(state.completed_routes_mask & (1 << i))) {
          const r = this.map.routes[i];
          if (state.windows_used[i] >= r.max_windows) {
            let canComplete = true;
            for (const sid of r.stations) {
              if (!((state.stations_marked >> BigInt(sid)) & 1n)) {
                let hasOpen = false;
                for (let o = 0; o < this.map.routes.length; ++o) {
                  if (o !== i && ((this.map.routes[o].mask >> BigInt(sid)) & 1n) &&
                      state.windows_used[o] < this.map.routes[o].max_windows) {
                    hasOpen = true;
                    break;
                  }
                }
                if (!hasOpen) {
                  canComplete = false;
                  break;
                }
              }
            }
            if (!canComplete) continue;
          }

          const oppTurn = this.scenario.opponent_finish_turns[i];
          if (turn1Based <= oppTurn) {
            ub += this.map.routes[i].gold_bonus;
          } else {
            ub += this.map.routes[i].silver_bonus;
          }
        }
      }

      // 2. Remaining transfers on top available hubs
      const transfersLeft = this.remainingTransfers[turn];
      if (transfersLeft > 0) {
        const emptyVals = [];
        for (let s = 0; s < this.map.total_stations; ++s) {
          if (!((state.stations_marked >> BigInt(s)) & 1n)) {
            emptyVals.push(this.map.station_transfer_pts[s]);
          }
        }
        emptyVals.sort((a, b) => b - a);
        const count = Math.min(emptyVals.length, transfersLeft);
        for (let i = 0; i < count; ++i) {
          ub += emptyVals[i];
        }
      }

      return ub;
    }

    generateMoves(state, turn) {
      const moves = [];
      const card = this.scenario.turns[turn];
      const futureTransfersExist = (this.remainingTransfers[turn + 1] > 0);
      const allowShortUse = this.settings.allow_short_use;

      if (card.type === 'FREE') {
        let foundIncomplete = false;
        for (let s = 0; s < this.map.total_stations; ++s) {
          if (!((state.stations_marked >> BigInt(s)) & 1n)) {
            let inIncomplete = false;
            for (let r = 0; r < this.map.routes.length; ++r) {
              if (!(state.completed_routes_mask & (1 << r)) && ((this.map.routes[r].mask >> BigInt(s)) & 1n)) {
                inIncomplete = true;
                break;
              }
            }
            if (inIncomplete) {
              foundIncomplete = true;
              moves.push({
                route_idx: -1,
                marked_stations: [s],
                is_transfer: false,
                transfer_station: -1,
                transfer_points: 0
              });
            }
          }
        }
        if (!foundIncomplete) {
          for (let s = 0; s < this.map.total_stations; ++s) {
            if (!((state.stations_marked >> BigInt(s)) & 1n)) {
              moves.push({
                route_idx: -1,
                marked_stations: [s],
                is_transfer: false,
                transfer_station: -1,
                transfer_points: 0
              });
            }
          }
        }
        return moves;
      }

      for (const r of this.map.routes) {
        if (state.windows_used[r.id] >= r.max_windows) continue;

        if (card.type === 'TRANSFER') {
          let firstEmpty = -1;
          for (const sid of r.stations) {
            if (!((state.stations_marked >> BigInt(sid)) & 1n)) {
              firstEmpty = sid;
              break;
            }
          }
          const m = {
            route_idx: r.id,
            marked_stations: firstEmpty !== -1 ? [firstEmpty] : [],
            is_transfer: firstEmpty !== -1,
            transfer_station: firstEmpty,
            transfer_points: firstEmpty !== -1 ? this.map.station_transfer_pts[firstEmpty] : 0
          };
          moves.push(m);
        }
        else if (card.type === 'NUMBER') {
          let firstEmptyIdx = r.stations.length;
          for (let i = 0; i < r.stations.length; ++i) {
            if (!((state.stations_marked >> BigInt(r.stations[i])) & 1n)) {
              firstEmptyIdx = i;
              break;
            }
          }

          if (firstEmptyIdx === r.stations.length) {
            moves.push({
              route_idx: r.id,
              marked_stations: [],
              is_transfer: false,
              transfer_station: -1,
              transfer_points: 0
            });
          } else {
            const candidates = [];
            for (let i = firstEmptyIdx; i < r.stations.length && candidates.length < card.value; ++i) {
              if ((state.stations_marked >> BigInt(r.stations[i])) & 1n) {
                break; // Stopped by already marked station
              }
              candidates.push(r.stations[i]);
            }

            // 1. Full fill
            moves.push({
              route_idx: r.id,
              marked_stations: candidates,
              is_transfer: false,
              transfer_station: -1,
              transfer_points: 0
            });

            // 2. Optional underfill if allowed
            if (allowShortUse && futureTransfersExist && candidates.length > 1) {
              for (let len = 1; len < candidates.length; ++len) {
                const nextStation = candidates[len];
                if (this.map.station_degrees[nextStation] >= 2) {
                  moves.push({
                    route_idx: r.id,
                    marked_stations: candidates.slice(0, len),
                    is_transfer: false,
                    transfer_station: -1,
                    transfer_points: 0
                  });
                }
              }
            }
          }
        }
        else if (card.type === 'SKIP') {
          const candidates = [];
          for (let i = 0; i < r.stations.length && candidates.length < card.value; ++i) {
            if (!((state.stations_marked >> BigInt(r.stations[i])) & 1n)) {
              candidates.push(r.stations[i]);
            }
          }

          if (candidates.length === 0) {
            moves.push({
              route_idx: r.id,
              marked_stations: [],
              is_transfer: false,
              transfer_station: -1,
              transfer_points: 0
            });
          } else {
            moves.push({
              route_idx: r.id,
              marked_stations: candidates,
              is_transfer: false,
              transfer_station: -1,
              transfer_points: 0
            });

            if (allowShortUse && futureTransfersExist && candidates.length > 1) {
              for (let len = 1; len < candidates.length; ++len) {
                const nextStation = candidates[len];
                if (this.map.station_degrees[nextStation] >= 2) {
                  moves.push({
                    route_idx: r.id,
                    marked_stations: candidates.slice(0, len),
                    is_transfer: false,
                    transfer_station: -1,
                    transfer_points: 0
                  });
                }
              }
            }
          }
        }
      }

      return moves;
    }

    applyMove(state, turn, move) {
      const next = {
        stations_marked: state.stations_marked,
        completed_routes_mask: state.completed_routes_mask,
        windows_used: new Uint8Array(state.windows_used),
        transfer_score: state.transfer_score,
        route_score: state.route_score
      };

      const turn1Based = turn + 1;

      if (move.route_idx !== -1) {
        next.windows_used[move.route_idx]++;
      }

      for (const sid of move.marked_stations) {
        next.stations_marked |= (1n << BigInt(sid));
      }

      if (move.is_transfer) {
        next.transfer_score += move.transfer_points;
      }

      move.completed_routes = [];
      move.completion_bonuses = [];
      move.is_gold_bonus = [];

      for (let i = 0; i < this.map.routes.length; ++i) {
        if (!(next.completed_routes_mask & (1 << i))) {
          const r = this.map.routes[i];
          if ((next.stations_marked & r.mask) === r.mask) {
            next.completed_routes_mask |= (1 << i);
            const isGold = (turn1Based <= this.scenario.opponent_finish_turns[i]);
            const bonus = isGold ? r.gold_bonus : r.silver_bonus;
            next.route_score += bonus;

            move.completed_routes.push(i);
            move.completion_bonuses.push(bonus);
            move.is_gold_bonus.push(isGold);
          }
        }
      }

      return next;
    }

    runBeamSearch(beamWidth = 2000) {
      let beam = [{
        state: {
          stations_marked: 0n,
          completed_routes_mask: 0,
          windows_used: new Uint8Array(this.map.routes.length),
          transfer_score: 0,
          route_score: 0
        },
        path: [],
        h: 0
      }];

      const totalTurns = this.scenario.turns.length;

      for (let t = 0; t < totalTurns; ++t) {
        const nextBeam = [];

        for (const node of beam) {
          const moves = this.generateMoves(node.state, t);
          for (const m of moves) {
            const nextState = this.applyMove(node.state, t, m);
            const markedCount = this.countBits(nextState.stations_marked);
            const h = (nextState.route_score * 3) + (nextState.transfer_score * 2) + markedCount;

            nextBeam.push({
              state: nextState,
              path: node.path.concat([m]),
              h: h
            });
          }
        }

        if (nextBeam.length === 0) break;

        nextBeam.sort((a, b) => b.h - a.h);
        beam = nextBeam.slice(0, beamWidth);
      }

      for (const node of beam) {
        const emptyCount = this.map.total_stations - this.countBits(node.state.stations_marked);
        const penalty = getEmptyStationsPenalty(emptyCount);
        const total = node.state.route_score + node.state.transfer_score + penalty;

        if (total > this.bestScore) {
          this.bestScore = total;
          this.bestPlaybook = node.path;
          this.bestBreakdown = {
            route_score: node.state.route_score,
            transfer_score: node.state.transfer_score,
            empty_stations: emptyCount,
            penalty: penalty
          };
        }
      }
    }

    dfbnb(state, turn, currentPath) {
      if (this.statesEvaluated >= 200000) return;
      this.statesEvaluated++;

      if (turn === this.scenario.turns.length) {
        const emptyCount = this.map.total_stations - this.countBits(state.stations_marked);
        const penalty = getEmptyStationsPenalty(emptyCount);
        const total = state.route_score + state.transfer_score + penalty;

        if (total > this.bestScore) {
          this.bestScore = total;
          this.bestPlaybook = currentPath.slice();
          this.bestBreakdown = {
            route_score: state.route_score,
            transfer_score: state.transfer_score,
            empty_stations: emptyCount,
            penalty: penalty
          };
        }
        return;
      }

      // Upper bound pruning
      const ub = this.computeUpperBound(state, turn);
      if (ub <= this.bestScore) return;

      // Transposition key
      const key = `${turn}_${state.stations_marked.toString(36)}_${state.windows_used.join(',')}`;
      const curScore = state.route_score + state.transfer_score;
      if (this.transpositionTable.has(key) && this.transpositionTable.get(key) >= curScore) {
        return;
      }
      this.transpositionTable.set(key, curScore);

      const moves = this.generateMoves(state, turn);
      const scoredMoves = [];

      for (const m of moves) {
        const nextState = this.applyMove(state, turn, m);
        let priority = 0;
        if (m.completed_routes.length > 0) {
          priority += 100 * m.completed_routes.length;
          for (const b of m.completion_bonuses) priority += b * 10;
        }
        priority += m.transfer_points * 5;
        priority += m.marked_stations.length;

        scoredMoves.push({ move: m, nextState, priority });
      }

      scoredMoves.sort((a, b) => b.priority - a.priority);

      for (const sm of scoredMoves) {
        currentPath.push(sm.move);
        this.dfbnb(sm.nextState, turn + 1, currentPath);
        currentPath.pop();
      }
    }

    solve() {
      const startTime = performance.now();

      // Phase 1: Fast Beam Search warm-start
      this.runBeamSearch(2500);

      // Phase 2: DFBnB
      const initialState = {
        stations_marked: 0n,
        completed_routes_mask: 0,
        windows_used: new Uint8Array(this.map.routes.length),
        transfer_score: 0,
        route_score: 0
      };

      this.transpositionTable.clear();
      this.statesEvaluated = 0;
      this.dfbnb(initialState, 0, []);

      const elapsed = performance.now() - startTime;

      // Format output format matching C++ solver JSON
      const formattedPlaybook = [];
      for (let t = 0; t < this.bestPlaybook.length; ++t) {
        const move = this.bestPlaybook[t];
        const card = this.scenario.turns[t];

        const compRoutes = [];
        if (move.completed_routes) {
          for (let i = 0; i < move.completed_routes.length; ++i) {
            compRoutes.push({
              route: this.map.routes[move.completed_routes[i]].name,
              bonus: move.completion_bonuses[i],
              is_gold: move.is_gold_bonus[i]
            });
          }
        }

        formattedPlaybook.push({
          turn: t + 1,
          card: cardToString(card),
          route: move.route_idx >= 0 ? this.map.routes[move.route_idx].name : '',
          is_transfer: move.is_transfer,
          transfer_station: move.transfer_station >= 0 ? this.map.station_names[move.transfer_station] : '',
          transfer_points: move.transfer_points,
          marked_stations: move.marked_stations.map(sid => this.map.station_names[sid]),
          completed_routes: compRoutes
        });
      }

      return {
        total_score: this.bestScore,
        route_score: this.bestBreakdown.route_score,
        transfer_score: this.bestBreakdown.transfer_score,
        empty_stations: this.bestBreakdown.empty_stations,
        penalty: this.bestBreakdown.penalty,
        states_evaluated: this.statesEvaluated,
        search_time_ms: elapsed,
        playbook: formattedPlaybook
      };
    }
  }

  // Convenient helper function to solve from raw text
  function solveMetroX(scenarioText, options = {}) {
    const scenario = parseScenario(scenarioText);
    if (options.allow_short_use !== undefined) {
      scenario.settings.allow_short_use = options.allow_short_use;
    }
    const solver = new MetroXSolver(scenario);
    return solver.solve();
  }

  return {
    parseScenario,
    parseCardToken,
    cardToString,
    MetroXSolver,
    solveMetroX,
    getEmptyStationsPenalty
  };

}));
