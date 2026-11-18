#include "games/carrom/Carrom.h"

#include "core/DartCollections.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace nx {
namespace carrom {

static const double k3 = 0.8660254037844386;

std::vector<Body> initialBodies() {
    const double s = cfg::coinR * 2 + 0.5;
    const double inner[6][2] = {{s, 0}, {s / 2, s * k3}, {-s / 2, s * k3}, {-s, 0}, {-s / 2, -s * k3}, {s / 2, -s * k3}};
    const double outer[12][2] = {{2 * s, 0},         {1.5 * s, s * k3},   {s, 2 * s * k3},     {0, 2 * s * k3},
                                 {-s, 2 * s * k3},   {-1.5 * s, s * k3},  {-2 * s, 0},         {-1.5 * s, -s * k3},
                                 {-s, -2 * s * k3},  {0, -2 * s * k3},    {s, -2 * s * k3},    {1.5 * s, -s * k3}};
    std::vector<std::array<double, 2>> whites, blacks;
    for (int i = 0; i < 6; ++i) (i % 2 == 0 ? whites : blacks).push_back({{inner[i][0], inner[i][1]}});
    for (int j = 0; j < 12; ++j) {
        bool white;
        if (j % 2 == 0) white = (j / 2) % 2 == 1;
        else white = j == 1 || j == 5 || j == 9;
        (white ? whites : blacks).push_back({{outer[j][0], outer[j][1]}});
    }
    const double cx = cfg::centerX, cy = cfg::centerY;
    std::vector<Body> bs;
    Body st;
    st.id = 0; st.kind = Kind::Striker; st.r = cfg::strikerR; st.m = cfg::strikerMass; st.x = cx; st.y = cfg::baseline0; st.onBoard = false;
    bs.push_back(st);
    for (int i = 0; i < 9; ++i) {
        Body b; b.id = 1 + i; b.kind = Kind::White; b.r = cfg::coinR; b.m = cfg::coinMass; b.x = cx + whites[i][0]; b.y = cy + whites[i][1];
        bs.push_back(b);
    }
    for (int i = 0; i < 9; ++i) {
        Body b; b.id = 10 + i; b.kind = Kind::Black; b.r = cfg::coinR; b.m = cfg::coinMass; b.x = cx + blacks[i][0]; b.y = cy + blacks[i][1];
        bs.push_back(b);
    }
    Body q; q.id = 19; q.kind = Kind::Queen; q.r = cfg::coinR; q.m = cfg::coinMass; q.x = cx; q.y = cy;
    bs.push_back(q);
    return bs;
}

static void collidePair(Body& a, Body& b, std::vector<Impact>* sink) {
    double dx = b.x - a.x, dy = b.y - a.y;
    double minD = a.r + b.r;
    double d2 = dx * dx + dy * dy;
    if (d2 >= minD * minD) return;
    double nx = 1, ny = 0, d = 0;
    if (d2 > 0) {
        d = std::sqrt(d2);
        nx = dx / d;
        ny = dy / d;
    }
    double overlap = minD - d;
    double ia = 1 / a.m, ib = 1 / b.m, isum = ia + ib;
    a.x -= nx * overlap * (ia / isum);
    a.y -= ny * overlap * (ia / isum);
    b.x += nx * overlap * (ib / isum);
    b.y += ny * overlap * (ib / isum);
    double rv = (b.vx - a.vx) * nx + (b.vy - a.vy) * ny;
    if (rv < 0) {
        double j = -(1 + cfg::ePiece) * rv / isum;
        a.vx -= j * ia * nx;
        a.vy -= j * ia * ny;
        b.vx += j * ib * nx;
        b.vy += j * ib * ny;
        if (sink) sink->push_back({a.x + nx * a.r, a.y + ny * a.r, -rv, false});
    }
}

static void cushions(Body& b, std::vector<Impact>* sink) {
    const double e = cfg::eWall;
    if (b.x - b.r < cfg::left) {
        b.x = cfg::left + b.r;
        if (b.vx < 0) {
            if (sink) sink->push_back({cfg::left, b.y, -b.vx, true});
            b.vx = -b.vx * e;
        }
    }
    if (b.x + b.r > cfg::right) {
        b.x = cfg::right - b.r;
        if (b.vx > 0) {
            if (sink) sink->push_back({cfg::right, b.y, b.vx, true});
            b.vx = -b.vx * e;
        }
    }
    if (b.y - b.r < cfg::top) {
        b.y = cfg::top + b.r;
        if (b.vy < 0) {
            if (sink) sink->push_back({b.x, cfg::top, -b.vy, true});
            b.vy = -b.vy * e;
        }
    }
    if (b.y + b.r > cfg::bottom) {
        b.y = cfg::bottom - b.r;
        if (b.vy > 0) {
            if (sink) sink->push_back({b.x, cfg::bottom, b.vy, true});
            b.vy = -b.vy * e;
        }
    }
}

int pocketAt(double x, double y) {
    const double c2 = cfg::captureR * cfg::captureR;
    for (int p = 0; p < 4; ++p) {
        double dx = x - cfg::pockets[p][0], dy = y - cfg::pockets[p][1];
        if (dx * dx + dy * dy < c2) return p;
    }
    return -1;
}

void step(std::vector<Body>& bs, std::vector<int>& pocketed, std::vector<Impact>* sink) {
    const double dt = cfg::dt;
    const double k = std::max(0.0, 1 - cfg::drag * dt);
    const double s2 = cfg::stopSpeed * cfg::stopSpeed;
    const size_t n = bs.size();
    for (size_t i = 0; i < n; ++i) {
        Body& b = bs[i];
        if (!b.onBoard) continue;
        b.x += b.vx * dt;
        b.y += b.vy * dt;
        b.vx *= k;
        b.vy *= k;
        if (b.vx * b.vx + b.vy * b.vy < s2) {
            b.vx = 0;
            b.vy = 0;
        }
    }
    for (size_t i = 0; i < n; ++i) {
        Body& a = bs[i];
        if (!a.onBoard) continue;
        for (size_t j = i + 1; j < n; ++j) {
            Body& b = bs[j];
            if (!b.onBoard) continue;
            if (a.vx == 0 && a.vy == 0 && b.vx == 0 && b.vy == 0) continue;
            collidePair(a, b, sink);
        }
    }
    for (size_t i = 0; i < n; ++i) if (bs[i].onBoard) cushions(bs[i], sink);
    for (size_t i = 0; i < n; ++i) {
        Body& b = bs[i];
        if (!b.onBoard) continue;
        if (pocketAt(b.x, b.y) >= 0) {
            b.onBoard = false;
            b.vx = 0;
            b.vy = 0;
            pocketed.push_back(b.id);
        }
    }
}

bool isSettled(const std::vector<Body>& bs) {
    for (auto& b : bs) if (b.onBoard && (b.vx != 0 || b.vy != 0)) return false;
    return true;
}

bool Simulator::tick() {
    if (done) return true;
    step(bodies, pocketed, sink_);
    ++steps;
    if (isSettled(bodies) || steps >= cfg::maxSteps) done = true;
    return done;
}

static double snapV(double v) { return std::floor(v * 1000 + 0.5) / 1000; }

void snap(std::vector<Body>& bs) {
    for (auto& b : bs) {
        b.x = snapV(b.x);
        b.y = snapV(b.y);
        b.vx = 0;
        b.vy = 0;
    }
}

uint32_t stateHash(const std::vector<Body>& bs) {
    uint32_t h = 2166136261u;
    auto mix = [&h](int64_t v) {
        h = h ^ static_cast<uint32_t>(v & 0xFFFFFFFF);
        h = h * 16777619u;
    };
    for (auto& b : bs) {
        mix(static_cast<int64_t>(std::floor(b.x * 1000 + 0.5)));
        mix(static_cast<int64_t>(std::floor(b.y * 1000 + 0.5)));
        mix(b.onBoard ? 1 : 0);
    }
    return h;
}

bool isFree(const std::vector<Body>& bs, double x, double y, double r, int ignoreId) {
    for (auto& o : bs) {
        if (!o.onBoard || o.id == ignoreId) continue;
        double dx = o.x - x, dy = o.y - y;
        double md = o.r + r + 1;
        if (dx * dx + dy * dy < md * md) return false;
    }
    return true;
}

void respot(std::vector<Body>& bs, int id) {
    Body& b = bs[id];
    b.vx = 0;
    b.vy = 0;
    const double stepLen = cfg::coinR;
    for (int k = 0; k <= 18; ++k) {
        for (int gy = -k; gy <= k; ++gy) {
            for (int gx = -k; gx <= k; ++gx) {
                if (std::abs(gx) != k && std::abs(gy) != k) continue;
                double x = cfg::centerX + gx * stepLen;
                double y = cfg::centerY + gy * stepLen;
                if (x - b.r < cfg::left + 40 || x + b.r > cfg::right - 40) continue;
                if (y - b.r < cfg::top + 40 || y + b.r > cfg::bottom - 40) continue;
                if (isFree(bs, x, y, b.r, id)) {
                    b.x = x;
                    b.y = y;
                    b.onBoard = true;
                    return;
                }
            }
        }
    }
    b.x = cfg::centerX;
    b.y = cfg::centerY;
    b.onBoard = true;
}

// ---------------- Shot / Match ----------------

Json Shot::toJson() const {
    return Json::obj({{"placeX", placeX}, {"dirX", dirX}, {"dirY", dirY}, {"power", power}});
}
Shot Shot::fromJson(const Json& j) {
    Shot s;
    s.placeX = j["placeX"].asDouble();
    s.dirX = j["dirX"].asDouble();
    s.dirY = j["dirY"].asDouble();
    s.power = j["power"].asDouble();
    return s;
}

Match Match::newBoard(int firstTurn) {
    Match m;
    m.bodies = initialBodies();
    m.turn = firstTurn;
    return m;
}

int Match::remaining(int seat) const {
    Kind k = kindFor(seat);
    int n = 0;
    for (auto& b : bodies) if (b.kind == k && b.onBoard) ++n;
    return n;
}

bool Match::placementOk(double x) const {
    if (!std::isfinite(x)) return false;
    if (x < cfg::baseMinX || x > cfg::baseMaxX) return false;
    return isFree(bodies, x, baselineY(turn), cfg::strikerR, cfg::strikerId);
}

bool Match::nearestPlacement(double x, double* out) const {
    double x0 = std::max(cfg::baseMinX, std::min(cfg::baseMaxX, x));
    for (int d = 0; d <= 540; ++d) {
        double a = x0 + d;
        if (a <= cfg::baseMaxX && placementOk(a)) { *out = a; return true; }
        double b = x0 - d;
        if (b >= cfg::baseMinX && placementOk(b)) { *out = b; return true; }
    }
    return false;
}

std::string Match::validate(const Shot& s) const {
    if (phase != Phase::Aiming) return "not_aiming";
    if (!std::isfinite(s.placeX) || !std::isfinite(s.dirX) || !std::isfinite(s.dirY) || !std::isfinite(s.power)) return "not_finite";
    if (s.placeX < cfg::baseMinX || s.placeX > cfg::baseMaxX) return "bad_place";
    if (!placementOk(s.placeX)) return "blocked_place";
    if (s.dirX * s.dirX + s.dirY * s.dirY < 1e-12) return "zero_dir";
    if (s.power <= 0 || s.power > 1) return "bad_power";
    return "";
}

void Match::beginShot(const Shot& s) {
    double len = std::sqrt(s.dirX * s.dirX + s.dirY * s.dirY);
    Body& st = striker();
    st.x = s.placeX;
    st.y = baselineY(turn);
    st.onBoard = true;
    st.vx = s.dirX / len * s.power * cfg::maxShotSpeed;
    st.vy = s.dirY / len * s.power * cfg::maxShotSpeed;
    phase = Phase::Simulating;
}

TurnOutcome Match::finishShot(const std::vector<int>& pocketed, int steps) {
    const int p = turn, opp = 1 - p;
    const Kind ownK = kindFor(p), oppK = kindFor(opp);
    std::vector<int> ownIds, oppIds;
    bool strikerIn = false, queenIn = false;
    for (int id : pocketed) {
        Kind k = bodies[id].kind;
        if (k == Kind::Striker) strikerIn = true;
        else if (k == Kind::Queen) queenIn = true;
        else if (k == ownK) ownIds.push_back(id);
        else if (k == oppK) oppIds.push_back(id);
    }
    striker().onBoard = false;
    striker().vx = 0;
    striker().vy = 0;
    std::vector<int> returned;
    const bool foul = strikerIn;
    bool coveredNow = false, queenBack = false, cont = false;
    if (foul) {
        for (int id : ownIds) {
            respot(bodies, id);
            returned.push_back(id);
        }
        if (queenIn || queenPendingFor == p) {
            respot(bodies, cfg::queenId);
            queenBack = true;
            queenPendingFor = -1;
        }
        for (auto& b : bodies) {
            if (b.kind == ownK && !b.onBoard) {
                int bid = b.id;
                respot(bodies, bid);
                returned.push_back(bid);
                break;
            }
        }
    } else {
        if (queenPendingFor == p) {
            if (!ownIds.empty()) {
                queenCoveredBy = p;
                coveredNow = true;
            } else {
                respot(bodies, cfg::queenId);
                queenBack = true;
            }
            queenPendingFor = -1;
        }
        if (queenIn) {
            if (!ownIds.empty()) {
                queenCoveredBy = p;
                coveredNow = true;
            } else {
                queenPendingFor = p;
            }
        }
        cont = !ownIds.empty() || queenIn;
    }
    if (queenCoveredBy < 0) {
        for (int q = 0; q < 2; ++q) {
            int seat = q == 0 ? p : opp;
            if (remaining(seat) != 0) continue;
            Kind k = kindFor(seat);
            int pick = -1;
            for (int i = static_cast<int>(pocketed.size()) - 1; i >= 0; --i) {
                if (bodies[pocketed[i]].kind == k) { pick = pocketed[i]; break; }
            }
            if (pick < 0) {
                for (int i = static_cast<int>(bodies.size()) - 1; i >= 0; --i) {
                    if (bodies[i].kind == k && !bodies[i].onBoard) { pick = i; break; }
                }
            }
            if (pick >= 0) {
                respot(bodies, pick);
                returned.push_back(pick);
            }
            if (seat == p) cont = false;
        }
    }
    if (!cont && queenPendingFor == p) {
        respot(bodies, cfg::queenId);
        queenPendingFor = -1;
        queenBack = true;
    }
    bool over = false;
    if (queenCoveredBy >= 0) {
        if (remaining(p) == 0) { hasWinner = true; winner = p; }
        else if (remaining(opp) == 0) { hasWinner = true; winner = opp; }
        if (hasWinner && winner >= 0) {
            over = true;
            points[winner] = remaining(1 - winner) + (queenCoveredBy == winner ? cfg::queenBonus : 0);
        }
    }
    turnNumber += 1;
    if (!over && turnNumber >= cfg::maxTurns) {
        over = true;
        decideByTally();
    }
    if (!over && !cont) turn = opp;
    snap(bodies);
    phase = over ? Phase::Finished : Phase::Aiming;
    TurnOutcome o;
    o.shooter = p;
    o.steps = steps;
    o.ownPocketed = static_cast<int>(ownIds.size());
    o.oppPocketed = static_cast<int>(oppIds.size());
    o.queenPocketed = queenIn;
    o.foul = foul;
    o.queenCovered = coveredNow;
    o.queenPending = queenPendingFor == p;
    o.queenReturned = queenBack;
    o.returnedIds = returned;
    o.continueTurn = !over && cont;
    o.matchOver = over;
    return o;
}

void Match::decideByTally() {
    int t0 = tally(0), t1 = tally(1);
    hasWinner = true;
    winner = t0 == t1 ? -1 : (t0 > t1 ? 0 : 1);
    if (winner >= 0) points[winner] = std::abs(t0 - t1);
}

TurnOutcome Match::playShot(const Shot& s) {
    std::string err = validate(s);
    if (!err.empty()) throw std::runtime_error(err);
    beginShot(s);
    Simulator sim(bodies);
    sim.runToEnd();
    return finishShot(sim.pocketed, sim.steps);
}

void Match::passTurn() {
    if (phase != Phase::Aiming) return;
    if (queenPendingFor == turn) {
        respot(bodies, cfg::queenId);
        queenPendingFor = -1;
        snap(bodies);
    }
    turn = 1 - turn;
    turnNumber += 1;
    if (turnNumber >= cfg::maxTurns) {
        phase = Phase::Finished;
        decideByTally();
    }
}

Json Match::toJson() const {
    Json bs = Json::array();
    for (auto& b : bodies) {
        bs.push(Json::obj({{"id", b.id}, {"kind", static_cast<int>(b.kind)}, {"x", b.x}, {"y", b.y}, {"on", b.onBoard}}));
    }
    return Json::obj({{"bodies", bs},
                      {"turn", turn},
                      {"turnNumber", turnNumber},
                      {"phase", static_cast<int>(phase)},
                      {"winner", hasWinner ? Json(winner) : Json()},
                      {"queenPendingFor", queenPendingFor},
                      {"queenCoveredBy", queenCoveredBy},
                      {"points", Json::arr(std::vector<int>{points[0], points[1]})}});
}

Match Match::fromJson(const Json& j) {
    Match m;
    m.bodies = initialBodies();
    for (auto& e : j["bodies"].items()) {
        int id = static_cast<int>(e["id"].asInt());
        if (id < 0 || id >= cfg::bodyCount) continue;
        Body& b = m.bodies[id];
        b.x = e["x"].asDouble();
        b.y = e["y"].asDouble();
        b.onBoard = e["on"].asBool(false);
    }
    m.turn = static_cast<int>(j["turn"].asInt());
    m.turnNumber = static_cast<int>(j["turnNumber"].asInt());
    m.phase = static_cast<Phase>(j["phase"].asInt());
    m.hasWinner = !j["winner"].isNull();
    m.winner = static_cast<int>(j["winner"].asInt());
    m.queenPendingFor = static_cast<int>(j["queenPendingFor"].asInt(-1));
    m.queenCoveredBy = static_cast<int>(j["queenCoveredBy"].asInt(-1));
    m.points = {{static_cast<int>(j["points"][0].asInt()), static_cast<int>(j["points"][1].asInt())}};
    return m;
}

// ---------------- Bot ----------------

static double clampD(double v, double lo, double hi) { return v < lo ? lo : (v > hi ? hi : v); }

Shot Bot::rotated(double x, double dx, double dy, double power, double ang) const {
    double c = std::cos(ang), s = std::sin(ang);
    Shot sh;
    sh.placeX = x;
    sh.dirX = dx * c - dy * s;
    sh.dirY = dx * s + dy * c;
    sh.power = power;
    return sh;
}

std::vector<Shot> Bot::candidates(const Match& m) {
    const double by = Match::baselineY(seat);
    const Kind ownK = Match::kindFor(seat);
    std::vector<const Body*> targets;
    for (auto& b : m.bodies) {
        if (!b.onBoard) continue;
        if (b.kind == ownK) targets.push_back(&b);
        if (b.kind == Kind::Queen && m.queenCoveredBy < 0 && m.queenPendingFor != seat) targets.push_back(&b);
    }
    const double reach = cfg::coinR + cfg::strikerR;
    struct Cand { double x, dx, dy, power, quality; int order; };
    std::vector<Cand> geo;
    int order = 0;
    for (auto* t : targets) {
        for (auto& p : cfg::pockets) {
            double ux = p[0] - t->x, uy = p[1] - t->y;
            double ul = std::sqrt(ux * ux + uy * uy);
            if (ul < 1e-6) continue;
            ux /= ul;
            uy /= ul;
            double gx = t->x - ux * reach, gy = t->y - uy * reach;
            std::vector<double> xs;
            if (std::fabs(uy) > 0.05) {
                double s = (by - gy) / (-uy);
                if (s > 0) xs.push_back(gx - ux * s);
            }
            xs.push_back(gx);
            xs.push_back(t->x);
            for (double x0 : xs) {
                double x;
                if (!m.nearestPlacement(clampD(x0, cfg::baseMinX, cfg::baseMaxX), &x)) continue;
                double dx = gx - x, dy = gy - by;
                double dl = std::sqrt(dx * dx + dy * dy);
                if (dl < 1e-6) continue;
                double cosCut = (dx * ux + dy * uy) / dl;
                if (cosCut < 0.25) continue;
                double dist = dl + ul;
                double power = clampD(0.26 + dist / 1900 + (1 - cosCut) * 0.35, 0.2, 1.0);
                geo.push_back({x, dx / dl, dy / dl, power, cosCut * 2 - dist / 1500, order++});
            }
        }
    }
    if (level == BotLevel::Easy) {
        dartShuffle(geo, rng_);
    } else {
        dartSort(geo, [](const Cand& a, const Cand& b) {
            int c = dartCompare(b.quality, a.quality);
            return c != 0 ? c : dartCompare(a.order, b.order);
        });
    }
    std::vector<Shot> out;
    const int count = candidateCount();
    const int takeGeo = static_cast<int>(std::round(count * (level == BotLevel::Easy ? 0.5 : 0.8)));
    for (auto& c : geo) {
        if (static_cast<int>(out.size()) >= takeGeo) break;
        out.push_back(rotated(c.x, c.dx, c.dy, c.power, (rng_.nextDouble() * 2 - 1) * aimJitter()));
        if (level == BotLevel::Hard && static_cast<int>(out.size()) < takeGeo) {
            out.push_back(rotated(c.x, c.dx, c.dy, clampD(c.power * 1.2, 0.2, 1.0), (rng_.nextDouble() * 2 - 1) * aimJitter()));
        }
    }
    int guard = 0;
    while (static_cast<int>(out.size()) < count && guard < count * 6) {
        ++guard;
        double x;
        if (!m.nearestPlacement(cfg::baseMinX + rng_.nextDouble() * (cfg::baseMaxX - cfg::baseMinX), &x)) break;
        double tx = cfg::centerX + (rng_.nextDouble() * 2 - 1) * 170;
        double ty = cfg::centerY + (rng_.nextDouble() * 2 - 1) * 170;
        double dx = tx - x, dy = ty - by;
        double dl = std::sqrt(dx * dx + dy * dy);
        if (dl < 1e-6) continue;
        Shot s;
        s.placeX = x;
        s.dirX = dx / dl;
        s.dirY = dy / dl;
        s.power = 0.55 + rng_.nextDouble() * 0.45;
        out.push_back(s);
    }
    return out;
}

bool Bot::evaluate(const Match& m, const Shot& shot, double* sc) {
    if (m.turn != seat) return false;
    Match sim = m;
    if (!sim.validate(shot).empty()) return false;
    TurnOutcome out = sim.playShot(shot);
    *sc = score(m, sim, out);
    return true;
}

double Bot::score(const Match& before, const Match& after, const TurnOutcome& o) const {
    if (after.hasWinner && after.winner == seat) return 1e6;
    if (after.hasWinner && after.winner == 1 - seat) return -1e6;
    double s = 0.0;
    s += 120.0 * (before.remaining(seat) - after.remaining(seat));
    s -= 80.0 * (before.remaining(1 - seat) - after.remaining(1 - seat));
    if (o.foul) s -= 260;
    if (o.queenCovered) s += 220;
    if (after.queenPendingFor == seat) s += 60;
    if (o.continueTurn) s += 40;
    const Kind ownK = Match::kindFor(seat);
    for (auto& b : after.bodies) {
        if (!b.onBoard || b.isStriker()) continue;
        double best = 1e9;
        for (auto& p : cfg::pockets) {
            double dx = b.x - p[0], dy = b.y - p[1];
            double d = std::sqrt(dx * dx + dy * dy);
            if (d < best) best = d;
        }
        double close = std::max(0.0, 1 - best / 450);
        if (b.kind == ownK) s += 18 * close;
        else if (!o.continueTurn && b.kind != Kind::Queen) s -= 14 * close;
    }
    return s;
}

Shot Bot::withExecutionNoise(const Shot& s) {
    return rotated(s.placeX, s.dirX, s.dirY, s.power, (rng_.nextDouble() * 2 - 1) * execNoise());
}

Shot Bot::fallback(const Match& m) const {
    double x;
    if (!m.nearestPlacement(cfg::centerX, &x)) x = cfg::centerX;
    Shot s;
    s.placeX = x;
    s.dirX = 0;
    s.dirY = seat == 0 ? -1 : 1;
    s.power = 0.8;
    return s;
}

Shot Bot::choose(const Match& m) {
    BotSearch search(*this, m);
    while (!search.step(1000)) {}
    return search.best();
}

bool BotSearch::step(int budget) {
    int n = 0;
    while (i_ < cands_.size() && n < budget) {
        const Shot shot = cands_[i_];
        ++i_;
        ++n;
        double s;
        if (bot_.evaluate(base_, shot, &s) && s > bestScore_) {
            bestScore_ = s;
            best_ = shot;
            hasBest_ = true;
        }
    }
    return done();
}

}  // namespace carrom
}  // namespace nx
