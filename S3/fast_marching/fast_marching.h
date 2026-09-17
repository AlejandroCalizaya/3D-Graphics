#ifndef FAST_MARCHING_H
#define FAST_MARCHING_H

#include "../../shared.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <unordered_set>

namespace FMM {
enum class State {
    FAR,
    TRIAL,
    ALIVE
};

struct FMMResult {
    std::vector<float> U;
    std::vector<State> state;
    float minU = 0.0f;
    float maxU = 0.0f;
    double elapsedMs = 0.0;
};

inline float dist2(const Vertex &a, const Vertex &b) {
    float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
    return dx * dx + dy * dy + dz * dz;
}

inline float dist(const Vertex &a, const Vertex &b) {
    return std::sqrt(dist2(a, b));
}

inline std::uint64_t edgeKey(int a, int b) {
    int lo = std::min(a, b), hi = std::max(a, b);
    return (static_cast<std::uint64_t>(lo) << 32) | static_cast<std::uint64_t>(hi);
}

inline int halfEdgeFrom(const std::vector<unsigned int> &faceIndices, size_t i) {
    int t = static_cast<int>(i / 3);
    int localIdx = static_cast<int>(i % 3);
    return faceIndices[3 * t + (localIdx + 2) % 3];
}

inline void buildTopology(const std::vector<CHE> &halfEdges, const std::vector<unsigned int> &faceIndices,
                          std::vector<std::vector<int>> &adj, std::unordered_set<std::uint64_t> &edges) {
    size_t numVertices = faceIndices.empty() ? 0 : *std::max_element(faceIndices.begin(), faceIndices.end()) + 1;
    adj.assign(numVertices, {});

    for (size_t i = 0; i < halfEdges.size(); ++i) {
        int from = halfEdgeFrom(faceIndices, i);
        int to = halfEdges[i].to;
        if (from == to)
            continue;

        adj[from].push_back(to);
        adj[to].push_back(from);
        edges.insert(edgeKey(from, to));
    }

    for (auto &nbrs : adj) {
        std::sort(nbrs.begin(), nbrs.end());
        nbrs.erase(std::unique(nbrs.begin(), nbrs.end()), nbrs.end());
    }
}

inline float solve2D(float uA, float uB, float x, float y, float z, float f, bool &valid) {
    valid = false;
    if (x < 1e-9f)
        return std::numeric_limits<float>::infinity();

    float alpha = (uB - uA) / x;
    float D = f * f - alpha * alpha;

    if (D < 0.0f || z < 0.0f || z > x)
        return std::numeric_limits<float>::infinity();

    valid = true;
    return uA + alpha * z + y * std::sqrt(D);
}

inline void localCoords(const Vertex &A, const Vertex &B, const Vertex &C, float &x, float &y, float &z) {
    float abx = B.x - A.x, aby = B.y - A.y, abz = B.z - A.z;
    x = std::sqrt(abx * abx + aby * aby + abz * abz);
    if (x < 1e-9f) {
        y = 0.0f;
        z = 0.0f;
        return;
    }
    float inv = 1.0f / x;
    float ax = abx * inv, ay = aby * inv, az = abz * inv;

    float cx = C.x - A.x, cy = C.y - A.y, cz = C.z - A.z;
    z = cx * ax + cy * ay + cz * az;

    float px = A.x + z * ax, py = A.y + z * ay, pz = A.z + z * az;
    float dx = C.x - px, dy = C.y - py, dz = C.z - pz;
    y = std::sqrt(dx * dx + dy * dy + dz * dz);
}

inline float solveLocal(int C, const std::vector<std::vector<int>> &adj, const std::unordered_set<std::uint64_t> &edges,
                        const std::vector<Vertex> &verts, const std::vector<float> &U, const std::vector<State> &state,
                        float f) {
    const float INF = std::numeric_limits<float>::infinity();
    const auto &nbrs = adj[C];

    std::vector<int> alive;
    alive.reserve(nbrs.size());
    for (int n : nbrs)
        if (state[n] == State::ALIVE)
            alive.push_back(n);

    if (alive.empty())
        return INF;

    if (alive.size() == 1)
        return U[alive[0]] + f * dist(verts[C], verts[alive[0]]);

    float best = INF;
    bool anyObtuse = false;
    int obtuseA = -1, obtuseB = -1;
    float obtuseZ = 0.0f;
    float bestObtuseCost = INF;

    for (size_t i = 0; i < alive.size(); ++i) {
        for (size_t j = i + 1; j < alive.size(); ++j) {
            int A = alive[i], B = alive[j];
            if (!edges.count(edgeKey(A, B)))
                continue;

            float x, y, z;
            localCoords(verts[A], verts[B], verts[C], x, y, z);

            bool valid;
            float cand = solve2D(U[A], U[B], x, y, z, f, valid);
            if (valid) {
                best = std::min(best, cand);
            } else {
                anyObtuse = true;
                float cost = U[A] + U[B];
                if (cost < bestObtuseCost) {
                    bestObtuseCost = cost;
                    obtuseA = A;
                    obtuseB = B;
                    obtuseZ = z;
                }
            }
        }
    }

    if (best < INF)
        return best;

    if (anyObtuse && obtuseA >= 0) {
        int pivot = (obtuseZ < 0.0f) ? obtuseA : obtuseB;
        int other = (obtuseZ < 0.0f) ? obtuseB : obtuseA;

        for (int D : adj[pivot]) {
            if (D == C || D == other)
                continue;
            if (state[D] != State::ALIVE)
                continue;
            if (!edges.count(edgeKey(pivot, D)))
                continue;

            float x, y, z;
            localCoords(verts[pivot], verts[D], verts[C], x, y, z);
            bool valid;
            float cand = solve2D(U[pivot], U[D], x, y, z, f, valid);
            if (valid)
                best = std::min(best, cand);
        }

        if (best < INF)
            return best;
    }

    for (int A : alive)
        best = std::min(best, U[A] + f * dist(verts[C], verts[A]));

    return best;
}

inline FMMResult fastMarching(const std::vector<Vertex> &verts, const std::vector<std::vector<int>> &adj,
                              const std::unordered_set<std::uint64_t> &edges, const std::vector<int> &sources,
                              float f) {
    const float INF = std::numeric_limits<float>::infinity();
    size_t N = verts.size();

    FMMResult res;
    res.U.assign(N, INF);
    res.state.assign(N, State::FAR);

    std::set<std::pair<float, int>> heap;

    auto clock0 = std::chrono::high_resolution_clock::now();

    for (int s : sources) {
        if (s < 0 || s >= static_cast<int>(N))
            continue;
        res.U[s] = 0.0f;
        res.state[s] = State::ALIVE;
    }

    for (int s : sources) {
        if (s < 0 || s >= static_cast<int>(N))
            continue;
        for (int n : adj[s]) {
            if (res.state[n] == State::FAR) {
                res.U[n] = f * dist(verts[s], verts[n]);
                res.state[n] = State::TRIAL;
                heap.insert({res.U[n], n});
            }
        }
    }

    while (!heap.empty()) {
        auto it = heap.begin();
        int u = it->second;
        heap.erase(it);

        if (res.state[u] == State::ALIVE)
            continue;
        res.state[u] = State::ALIVE;

        for (int v : adj[u]) {
            if (res.state[v] == State::ALIVE)
                continue;

            float Unew = solveLocal(v, adj, edges, verts, res.U, res.state, f);
            if (Unew >= res.U[v])
                continue;

            float oldU = res.U[v];
            res.U[v] = Unew;

            if (res.state[v] == State::FAR) {
                res.state[v] = State::TRIAL;
                heap.insert({Unew, v});
            } else {
                heap.erase({oldU, v});
                heap.insert({Unew, v});
            }
        }
    }

    auto clock1 = std::chrono::high_resolution_clock::now();
    res.elapsedMs = std::chrono::duration_cast<std::chrono::microseconds>(clock1 - clock0).count() / 1000.0;

    float lo = INF, hi = -INF;
    for (float u : res.U) {
        if (u < INF) {
            lo = std::min(lo, u);
            hi = std::max(hi, u);
        }
    }
    if (lo < INF) {
        res.minU = lo;
        res.maxU = hi;
    }

    return res;
}

inline glm::vec3 jet(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    float r = std::clamp(1.5f - std::fabs(4.0f * t - 3.0f), 0.0f, 1.0f);
    float g = std::clamp(1.5f - std::fabs(4.0f * t - 2.0f), 0.0f, 1.0f);
    float b = std::clamp(1.5f - std::fabs(4.0f * t - 1.0f), 0.0f, 1.0f);
    return {r, g, b};
}

} // namespace FMM

#endif // FAST_MARCHING_H
