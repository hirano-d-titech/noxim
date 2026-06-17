#ifndef __CLUSTERROUTINGMANAGER_H__
#define __CLUSTERROUTINGMANAGER_H__

#include <vector>
#include <map>
#include <algorithm>
#include <iostream>
#include "GlobalParams.h"
#include "Utils.h"

class ClusterRoutingManager {
private:
  int N; // Mesh size N
  int num_clusters; // N^2 / 4 (for routing cache, padded for actual size)
  int max_route_len; // 3N/2 - 3
  
  // Route cache table: [dest_cluster_id][route step sequence of clusters]
  std::vector<std::vector<int>> route_cache;
  
  bool is_routes_generated;

  // Helper: Generate WEST_LAST route (West-bound last turn model)
  std::vector<int> generateWestLastRoute(int src_cluster, int dst_cluster) {
    std::vector<int> path;
    int mesh_cx = (GlobalParams::mesh_dim_x + 1) / 2;

    int scx = src_cluster % mesh_cx;
    int scy = src_cluster / mesh_cx;
    int dcx = dst_cluster % mesh_cx;
    int dcy = dst_cluster / mesh_cx;

    int cx = scx;
    int cy = scy;

    path.push_back(cy * mesh_cx + cx);

    if (dcx < scx) {
      // West-bound last: route North/South first, then West
      while (cy != dcy) {
        cy += (dcy > cy) ? 1 : -1;
        path.push_back(cy * mesh_cx + cx);
      }
      while (cx != dcx) {
        cx--;
        path.push_back(cy * mesh_cx + cx);
      }
    } else {
      // East-bound / North-South: standard XY routing
      while (cx != dcx) {
        cx++;
        path.push_back(cy * mesh_cx + cx);
      }
      while (cy != dcy) {
        cy += (dcy > cy) ? 1 : -1;
        path.push_back(cy * mesh_cx + cx);
      }
    }
    return path;
  }

  // DFS helper for finding optimal path under constraints
  void dfsFindRoute(
      int curr,
      int target,
      std::vector<int>& current_path,
      std::vector<bool>& visited,
      bool has_moved_east,
      double current_cost,
      std::vector<int>& best_path,
      double& min_cost,
      int mesh_cx,
      int mesh_cy,
      const std::map<int, double>& evaluations
  ) {
    if (curr == target) {
      if (current_cost < min_cost) {
        min_cost = current_cost;
        best_path = current_path;
      }
      return;
    }

    // Branch and Bound pruning
    if (current_cost >= min_cost) {
      return;
    }

    // Path length constraint: max_route_len
    if (current_path.size() >= (size_t)max_route_len) {
      return;
    }

    int cx = curr % mesh_cx;
    int cy = curr / mesh_cx;

    struct Neighbor {
      int id;
      int x;
      int y;
    };

    std::vector<Neighbor> neighbors;
    if (cy > 0) neighbors.push_back({ (cy - 1) * mesh_cx + cx, cx, cy - 1 });
    if (cx < mesh_cx - 1) neighbors.push_back({ cy * mesh_cx + (cx + 1), cx + 1, cy });
    if (cy < mesh_cy - 1) neighbors.push_back({ (cy + 1) * mesh_cx + cx, cx, cy + 1 });
    if (cx > 0) neighbors.push_back({ cy * mesh_cx + (cx - 1), cx - 1, cy });

    for (const auto& next : neighbors) {
      if (next.id >= (int)route_cache.size() || visited[next.id]) {
        continue;
      }

      // 1. Odd-Even Turn Model constraints
      if (current_path.size() >= 2) {
        int prev = current_path[current_path.size() - 2];
        int prev_x = prev % mesh_cx;
        int prev_y = prev / mesh_cx;

        // Even columns: E-N and E-S turns are prohibited
        if (cx % 2 == 0) {
          if (cx > prev_x && next.y != cy) {
            continue; // Prohibited E-N/E-S turn
          }
        }
        // Odd columns: N-W and S-W turns are prohibited
        if (cx % 2 == 1) {
          if (cy != prev_y && next.x < cx) {
            continue; // Prohibited N-W/S-W turn
          }
        }
      }

      // 2. Convex detour to East restriction
      bool next_has_moved_east = has_moved_east;
      if (next.x > cx) {
        next_has_moved_east = true;
      } else if (next.x < cx) {
        if (has_moved_east) {
          continue; // Prohibited West step after East step
        }
      }

      // Cost calculation: 1.0 (base hop) + penalty
      double eval = 0.0;
      auto it = evaluations.find(next.id);
      if (it != evaluations.end()) {
        eval = it->second;
      }
      double step_cost = 1.0 + (GlobalParams::eval_success - eval);

      // Recurse
      visited[next.id] = true;
      current_path.push_back(next.id);

      dfsFindRoute(next.id, target, current_path, visited, next_has_moved_east, current_cost + step_cost, best_path, min_cost, mesh_cx, mesh_cy, evaluations);

      current_path.pop_back();
      visited[next.id] = false;
    }
  }

public:
  ClusterRoutingManager(int mesh_size) : N(mesh_size), is_routes_generated(false) {
    num_clusters = (N * N) / 4;
    max_route_len = (3 * N) / 2 - 3;
    int mesh_cx = (N + 1) / 2;
    int mesh_cy = (N + 1) / 2;
    int actual_clusters = mesh_cx * mesh_cy;
    // Resize to max of num_clusters and actual_clusters to prevent out-of-bounds
    route_cache.resize(std::max(num_clusters, actual_clusters));
  }

  void recalculateAllRoutes(int src_node_id, const std::map<int, double>& evaluations) {
    int src_cluster = getClusterId(src_node_id);
    int mesh_cx = (N + 1) / 2;
    int mesh_cy = (N + 1) / 2;
    int num_destinations = route_cache.size();

    for (int dst_cluster = 0; dst_cluster < num_destinations; ++dst_cluster) {
      if (src_cluster == dst_cluster) {
        route_cache[dst_cluster] = { src_cluster };
        continue;
      }

      std::vector<int> best_path;
      double min_cost = 1e9;

      std::vector<int> current_path;
      current_path.push_back(src_cluster);

      std::vector<bool> visited(num_destinations, false);
      visited[src_cluster] = true;

      dfsFindRoute(src_cluster, dst_cluster, current_path, visited, false, 0.0, best_path, min_cost, mesh_cx, mesh_cy, evaluations);

      if (!best_path.empty()) {
        route_cache[dst_cluster] = best_path;
        if (GlobalParams::verbose_mode != VERBOSE_OFF) {
          std::cout << "PE " << src_node_id << " @ " << sc_time_stamp().to_double() / GlobalParams::clock_period_ps
                    << ": Recalculated optimal route to cluster " << dst_cluster << ": ";
          for (size_t i = 0; i < best_path.size(); ++i) {
            std::cout << best_path[i] << (i + 1 < best_path.size() ? "->" : "");
          }
          std::cout << " (cost: " << min_cost << ")" << std::endl;
        }
      } else {
        route_cache[dst_cluster] = generateWestLastRoute(src_cluster, dst_cluster);
      }
    }
    is_routes_generated = true;
  }

  std::vector<int> getRoute(int src_node_id, int dst_node_id) {
    int dst_cluster = getClusterId(dst_node_id);
    std::vector<int> cluster_path;

    if (!is_routes_generated) {
      int src_cluster = getClusterId(src_node_id);
      cluster_path = generateWestLastRoute(src_cluster, dst_cluster);
    } else {
      if (dst_cluster >= 0 && dst_cluster < (int)route_cache.size()) {
        cluster_path = route_cache[dst_cluster];
      }
      if (cluster_path.empty()) {
        int src_cluster = getClusterId(src_node_id);
        cluster_path = generateWestLastRoute(src_cluster, dst_cluster);
      }
    }

    std::vector<int> route;
    route.push_back(0); // current_idx
    route.insert(route.end(), cluster_path.begin(), cluster_path.end());

    if (route.size() > (size_t)(1 + max_route_len)) {
      route.resize(1 + max_route_len);
    }

    return route;
  }
};

#endif
