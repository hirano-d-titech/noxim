#include "Routing_CLUSTER.h"
#include "../ClusterRoutingManager.h"
#include "../NoC.h"
#include <algorithm>

extern NoC *n;

RoutingAlgorithmsRegister Routing_CLUSTER::routingAlgorithmsRegister("CLUSTER", getInstance());

Routing_CLUSTER * Routing_CLUSTER::routing_CLUSTER = 0;

Routing_CLUSTER * Routing_CLUSTER::getInstance() {
  if ( routing_CLUSTER == 0 )
    routing_CLUSTER = new Routing_CLUSTER();

  return routing_CLUSTER;
}

// Helper: Get cluster ID for a node ID
// Now using global getClusterId() from Utils.h

// Helper: Check if coordinates are valid
static bool isValidCoord(const Coord & coord) {
  return (coord.x >= 0 && coord.x < GlobalParams::mesh_dim_x &&
          coord.y >= 0 && coord.y < GlobalParams::mesh_dim_y);
}

// Helper: Convert coordinates to valid node ID
static int getValidNodeId(const Coord & coord) {
  if (isValidCoord(coord)) return coord2Id(coord);
  return NOT_VALID;
}

// Helper: Get exit node in current cluster closest to dest_node_id bordering next_cluster
static int getBestExitNode(int current_cluster, int next_cluster, int dest_node_id) {
  int mesh_cx = (GlobalParams::mesh_dim_x + 1) / 2;
  int ccx = current_cluster % mesh_cx;
  int ccy = current_cluster / mesh_cx;
  int ncx = next_cluster % mesh_cx;
  int ncy = next_cluster / mesh_cx;

  Coord c1, c2;
  if (ncx > ccx) { // East
    c1 = Coord{2 * ccx + 1, 2 * ccy};
    c2 = Coord{2 * ccx + 1, 2 * ccy + 1};
  } else if (ncx < ccx) { // West
    c1 = Coord{2 * ccx, 2 * ccy};
    c2 = Coord{2 * ccx, 2 * ccy + 1};
  } else if (ncy > ccy) { // South
    c1 = Coord{2 * ccx, 2 * ccy + 1};
    c2 = Coord{2 * ccx + 1, 2 * ccy + 1};
  } else { // North
    c1 = Coord{2 * ccx, 2 * ccy};
    c2 = Coord{2 * ccx + 1, 2 * ccy};
  }

  int id1 = getValidNodeId(c1);
  int id2 = getValidNodeId(c2);

  if (id1 == NOT_VALID) return id2;
  if (id2 == NOT_VALID) return id1;

  // Choose the one closer to the destination node
  int dist1 = getManhattanDistance(id1, dest_node_id);
  int dist2 = getManhattanDistance(id2, dest_node_id);

  return (dist1 <= dist2) ? id1 : id2;
}

// Helper: Get boundary crossing port leading to next_cluster
static vector<int> getBoundaryCrossingPort(Router * router, int current_node_id, int next_cluster) {
  vector<int> directions;
  for (int dir : {DIRECTION_NORTH, DIRECTION_EAST, DIRECTION_SOUTH, DIRECTION_WEST}) {
    int neighbor = router->getNeighborId(current_node_id, dir);
    if (neighbor != NOT_VALID && getClusterId(neighbor) == next_cluster) {
      directions.push_back(dir);
      break; // Only one boundary crossing port from this boundary exit node
    }
  }
  return directions;
}

// Helper: Odd-Even routing from current_id to target_id
static vector<int> routeOddEven(int current_id, int target_id, int src_id) {
  Coord current = id2Coord(current_id);
  Coord destination = id2Coord(target_id);
  Coord source = id2Coord(src_id);
  vector <int> directions;

  int c0 = current.x;
  int c1 = current.y;
  int s0 = source.x;
  int d0 = destination.x;
  int d1 = destination.y;
  int e0, e1;

  e0 = d0 - c0;
  e1 = -(d1 - c1);

  if (e0 == 0) {
    if (e1 > 0)
      directions.push_back(DIRECTION_NORTH);
    else if (e1 < 0)
      directions.push_back(DIRECTION_SOUTH);
  } else {
    if (e0 > 0) {
      if (e1 == 0)
        directions.push_back(DIRECTION_EAST);
      else {
        if ((c0 % 2 == 1) || (c0 == s0)) {
          if (e1 > 0)
            directions.push_back(DIRECTION_NORTH);
          else
            directions.push_back(DIRECTION_SOUTH);
        }
        if ((d0 % 2 == 1) || (e0 != 1))
          directions.push_back(DIRECTION_EAST);
      }
    } else {
      directions.push_back(DIRECTION_WEST);
      if (c0 % 2 == 0) {
        if (e1 > 0)
          directions.push_back(DIRECTION_NORTH);
        if (e1 < 0)
          directions.push_back(DIRECTION_SOUTH);
      }
    }
  }
  return directions;
}

// Helper: Local routing inside cluster using Odd-Even
static vector<int> routeLocally(int current_node_id, int target_node_id, int src_node_id) {
  return routeOddEven(current_node_id, target_node_id, src_node_id);
}

// Reads this router's local PE's own learned trust score for `cluster_id`
// (§4.1: always the local PE, never the packet's source PE).
static double localClusterEvaluation(int local_node_id, int cluster_id) {
  Tile *t = n->searchNode(local_node_id);
  if (t && t->pe) {
    auto it = t->pe->cluster_evaluations.find(cluster_id);
    if (it != t->pe->cluster_evaluations.end()) return it->second;
  }
  return 0.0;
}

// Reads the DegradationMonitor delay of the router that is one hop away, in
// `direction` from `local_node_id` -- i.e. the boundary router that would
// actually receive the flit if this candidate direction is chosen.
static double neighborClusterDelay(Router *router, int local_node_id, int direction) {
  int neighbor_id = router->getNeighborId(local_node_id, direction);
  if (neighbor_id == NOT_VALID) return 0.0;
  Tile *t = n->searchNode(neighbor_id);
  if (t) return (double) t->r->degradation_monitor.getCurrentDelay();
  return 0.0;
}

static int clusterDirToRouterDirection(ClusterRouting::ClusterDir dir) {
  switch (dir) {
    case ClusterRouting::CLUSTER_DIR_NORTH: return DIRECTION_NORTH;
    case ClusterRouting::CLUSTER_DIR_EAST:  return DIRECTION_EAST;
    case ClusterRouting::CLUSTER_DIR_SOUTH: return DIRECTION_SOUTH;
    case ClusterRouting::CLUSTER_DIR_WEST:  return DIRECTION_WEST;
  }
  return NOT_VALID;
}

// Core routing function.
//
// No path is ever precomputed: the next cluster is decided on the fly, in
// O(1), every time a HEAD flit is at a cluster boundary. See
// .claude/local_adaptive_cluster_routing_design.md §2-3 for the design
// rationale (this replaces the previous DFS route-cache approach).
vector<int> Routing_CLUSTER::route(Router * router, Flit & flit, const RouteData & routeData) {
  int current_node_id = routeData.current_id;
  int dest_node_id = routeData.dst_id;

  int current_cluster = getClusterId(current_node_id);
  int destination_cluster = getClusterId(dest_node_id);

  vector<int> directions;

  if (current_cluster == destination_cluster) {
    // Case A: Inside destination cluster, route to final node.
    directions = routeLocally(current_node_id, dest_node_id, routeData.src_id);
  } else {
    // Case B: decide (or keep heading towards) the next cluster.
    int src_cluster = getClusterId(routeData.src_id);
    vector<ClusterRouting::Candidate> candidates =
        ClusterRouting::getLegalCandidates(current_cluster, destination_cluster, src_cluster,
                                            flit.route_metadata.has_moved_east);

    if (candidates.empty()) {
      // Defensive fallback: the Odd-Even Turn Model always leaves at least
      // one legal direction, so this should not happen in practice. Ignore
      // the turn-model/detour constraints rather than stalling.
      int ccx, ccy, dcx, dcy;
      ClusterRouting::clusterCoord(current_cluster, ccx, ccy);
      ClusterRouting::clusterCoord(destination_cluster, dcx, dcy);
      if (dcy != ccy) {
        int next_cy = (dcy > ccy) ? ccy + 1 : ccy - 1;
        candidates.push_back(ClusterRouting::Candidate{
            (dcy > ccy) ? ClusterRouting::CLUSTER_DIR_SOUTH : ClusterRouting::CLUSTER_DIR_NORTH,
            ClusterRouting::clusterIdFromCoord(ccx, next_cy)});
      } else {
        int next_cx = (dcx > ccx) ? ccx + 1 : ccx - 1;
        candidates.push_back(ClusterRouting::Candidate{
            (dcx > ccx) ? ClusterRouting::CLUSTER_DIR_EAST : ClusterRouting::CLUSTER_DIR_WEST,
            ClusterRouting::clusterIdFromCoord(next_cx, ccy)});
      }
    }

    int next_cluster = NOT_VALID;
    ClusterRouting::ClusterDir chosen_dir = ClusterRouting::CLUSTER_DIR_NORTH;
    int sub_destination;

    if (candidates.size() == 1) {
      next_cluster = candidates[0].cluster_id;
      chosen_dir = candidates[0].dir;
      sub_destination = getBestExitNode(current_cluster, next_cluster, dest_node_id);
    } else {
      // Diagonal destination: both candidates are only directly reachable
      // from the single node whose ports face both required directions
      // (§3.4 "decision corner").
      int corner = ClusterRouting::getDecisionCornerNode(current_cluster, destination_cluster);
      sub_destination = corner;

      if (current_node_id == corner) {
        // At the decision corner: evaluate both candidates now, in O(1).
        vector<double> costs;
        for (const auto &c : candidates) {
          int dir = clusterDirToRouterDirection(c.dir);
          double evaluation = localClusterEvaluation(current_node_id, c.cluster_id);
          double delay = neighborClusterDelay(router, current_node_id, dir);
          costs.push_back(ClusterRouting::computeStepCost(evaluation, delay));
        }
        size_t chosen = ClusterRouting::selectNextCluster(candidates, costs, destination_cluster);
        next_cluster = candidates[chosen].cluster_id;
        chosen_dir = candidates[chosen].dir;
      }
      // else: not there yet, next_cluster stays NOT_VALID -- just keep
      // routing locally towards the corner below.
    }

    if (current_node_id == sub_destination && next_cluster != NOT_VALID) {
      // We are at the exit node, cross the cluster boundary.
      directions = getBoundaryCrossingPort(router, current_node_id, next_cluster);
      if (!directions.empty() && chosen_dir == ClusterRouting::CLUSTER_DIR_EAST) {
        flit.route_metadata.has_moved_east = true;
      }
    }
    if (directions.empty()) {
      // Inside current cluster (or crossing link blocked): route locally
      // towards the exit node / decision corner.
      directions = routeLocally(current_node_id, sub_destination, routeData.src_id);
    }
  }

  // Final fallback to prevent stalling in case of unmapped situations.
  if (directions.empty()) {
    directions = routeLocally(current_node_id, dest_node_id, routeData.src_id);
  }

  return directions;
}
