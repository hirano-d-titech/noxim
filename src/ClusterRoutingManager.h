#ifndef __CLUSTERROUTINGMANAGER_H__
#define __CLUSTERROUTINGMANAGER_H__

#include <vector>
#include "GlobalParams.h"
#include "Utils.h"

// Pure, stateless helpers for local adaptive cluster-level routing.
//
// Unlike the previous design (a per-PE, all-destinations DFS route cache),
// no path is ever precomputed or stored: every boundary-crossing router
// decides only the *next* cluster, in O(1), using nothing but the current /
// source / destination cluster coordinates and the sticky "have I ever moved
// east" bit carried in RouteMetadata (see DataStructs.h). The heavier,
// impure lookups (this PE's learned cluster_evaluations, a neighbor
// router's DegradationMonitor delay) are fetched by the caller
// (Routing_CLUSTER.cpp) and passed in here as plain numbers.
namespace ClusterRouting {

  enum ClusterDir { CLUSTER_DIR_NORTH, CLUSTER_DIR_EAST, CLUSTER_DIR_SOUTH, CLUSTER_DIR_WEST };

  struct Candidate {
    ClusterDir dir;
    int cluster_id;
  };

  inline int clusterMeshWidth() {
    return (GlobalParams::mesh_dim_x + 1) / 2;
  }

  inline void clusterCoord(int cluster_id, int &cx, int &cy) {
    int mesh_cx = clusterMeshWidth();
    cx = cluster_id % mesh_cx;
    cy = cluster_id / mesh_cx;
  }

  inline int clusterIdFromCoord(int cx, int cy) {
    return cy * clusterMeshWidth() + cx;
  }

  // Legal next-cluster candidates from `current_cluster` towards
  // `dest_cluster`, applying the Odd-Even Turn Model (parameterized by
  // `src_cluster`, exactly like the classical per-node algorithm) and the
  // convex-detour-to-east restriction (parameterized by `has_moved_east`).
  // Returns at most 2 entries: one horizontal (E/W), one vertical (N/S) --
  // whichever axes actually differ between current and destination cluster.
  inline std::vector<Candidate> getLegalCandidates(int current_cluster, int dest_cluster,
                                                    int src_cluster, bool has_moved_east) {
    std::vector<Candidate> candidates;

    int ccx, ccy, dcx, dcy, scx, scy;
    clusterCoord(current_cluster, ccx, ccy);
    clusterCoord(dest_cluster, dcx, dcy);
    clusterCoord(src_cluster, scx, scy);

    int e0 = dcx - ccx;  // > 0: destination cluster is east
    int e1 = ccy - dcy;  // > 0: destination cluster is north (smaller y == north)

    bool want_horizontal = (e0 != 0);
    bool want_vertical = (e1 != 0);
    bool horizontal_legal = want_horizontal;
    bool vertical_legal = want_vertical;

    if (e0 > 0) {
      if (want_vertical) {
        // Odd-Even: the N/S detour is only legal from an odd column, or
        // while still in the source column (mirrors routeOddEven()).
        vertical_legal = (ccx % 2 == 1) || (ccx == scx);
        horizontal_legal = (dcx % 2 == 1) || (e0 != 1);
      }
      // else: pure east, no turn constraint (horizontal_legal stays true).
    } else if (e0 < 0) {
      // WEST is always Odd-Even legal by itself...
      if (want_vertical) {
        vertical_legal = (ccx % 2 == 0);
      }
      // ...but the convex-detour-to-east restriction forbids it once the
      // packet has ever moved east.
      if (has_moved_east) horizontal_legal = false;
    }
    // e0 == 0: pure vertical move, no turn constraint.

    if (want_horizontal && horizontal_legal) {
      ClusterDir dir = (e0 > 0) ? CLUSTER_DIR_EAST : CLUSTER_DIR_WEST;
      int next_cx = ccx + (e0 > 0 ? 1 : -1);
      candidates.push_back(Candidate{dir, clusterIdFromCoord(next_cx, ccy)});
    }
    if (want_vertical && vertical_legal) {
      ClusterDir dir = (e1 > 0) ? CLUSTER_DIR_NORTH : CLUSTER_DIR_SOUTH;
      int next_cy = ccy + (e1 > 0 ? -1 : 1);
      candidates.push_back(Candidate{dir, clusterIdFromCoord(ccx, next_cy)});
    }

    return candidates;
  }

  // The unique node, within `current_cluster`'s 2x2 group, that has direct
  // (single-hop) links to both external clusters needed to compare a
  // diagonal destination -- i.e. the corner matching the destination's
  // quadrant (NE quadrant -> NE corner, and so on). Only meaningful when a
  // diagonal comparison is actually needed (2 legal candidates); callers
  // with a single candidate should keep using getBestExitNode() instead.
  inline int getDecisionCornerNode(int current_cluster, int dest_cluster) {
    int ccx, ccy, dcx, dcy;
    clusterCoord(current_cluster, ccx, ccy);
    clusterCoord(dest_cluster, dcx, dcy);

    int e0 = dcx - ccx;
    int e1 = ccy - dcy;

    Coord corner;
    corner.x = 2 * ccx + (e0 > 0 ? 1 : 0);
    corner.y = 2 * ccy + (e1 > 0 ? 0 : 1);
    return coord2Id(corner);
  }

  // step_cost(c) = 1.0 + (eval_success - evaluation(c)) + beta * delay(c)
  // Always strictly positive as long as delay(c) >= 0 (see CLAUDE.md's
  // "Step Cost Positivity" caution): the worst-case evaluation is bounded
  // below zero, never above eval_success, so the eval term alone is >= 0.
  inline double computeStepCost(double evaluation, double delay) {
    return 1.0 + (GlobalParams::eval_success - evaluation) + GlobalParams::beta * delay;
  }

  // Picks which legal candidate to actually take:
  //  - the destination cluster itself is always force-selected (3.3), to
  //    guarantee progress rather than orbiting a degraded destination;
  //  - a single candidate is taken as-is (no comparison needed);
  //  - between two candidates, an East candidate is only preferred when it
  //    is *clearly* cheaper (by more than epsilon) than the alternative --
  //    otherwise the non-East candidate wins, including on ties (3.2).
  inline size_t selectNextCluster(const std::vector<Candidate> &candidates, const std::vector<double> &costs,
                                   int dest_cluster) {
    for (size_t i = 0; i < candidates.size(); i++) {
      if (candidates[i].cluster_id == dest_cluster) return i;
    }

    if (candidates.size() <= 1) return 0;

    // candidates.size() == 2: at most one of them is CLUSTER_DIR_EAST.
    size_t east_idx = candidates.size();
    for (size_t i = 0; i < candidates.size(); i++) {
      if (candidates[i].dir == CLUSTER_DIR_EAST) east_idx = i;
    }

    if (east_idx == candidates.size()) {
      // Neither candidate is East (e.g. West vs North/South): plain min-cost.
      return (costs[0] <= costs[1]) ? 0 : 1;
    }

    size_t other_idx = 1 - east_idx;
    if (costs[east_idx] < costs[other_idx] - GlobalParams::epsilon) {
      return east_idx;  // East is clearly better.
    }
    return other_idx;  // Tie or non-East is (at least not clearly worse).
  }

}  // namespace ClusterRouting

#endif
