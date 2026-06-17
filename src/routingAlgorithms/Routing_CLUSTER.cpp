#include "Routing_CLUSTER.h"
#include <algorithm>

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

// Core routing function
vector<int> Routing_CLUSTER::route(Router * router, Flit & flit, const RouteData & routeData) {
  int current_node_id = routeData.current_id;
  int dest_node_id = routeData.dst_id;

  // Safe fallback if cluster metadata is not initialized or invalid
  if (flit.route_metadata.custom_data.size() < 2) {
    return routeLocally(current_node_id, dest_node_id, routeData.src_id);
  }

  int current_cluster = getClusterId(current_node_id);

  // custom_data[0] is current_idx
  int current_idx = flit.route_metadata.custom_data[0];
  int route_size = flit.route_metadata.custom_data.size() - 1; // Number of clusters in path

  if (current_idx >= route_size || current_idx < 0) {
    return routeLocally(current_node_id, dest_node_id, routeData.src_id);
  }

  int target_cluster = flit.route_metadata.custom_data[1 + current_idx];

  // ==========================================
  // STEP 2: Macro route index update
  // ==========================================
  if (current_cluster != target_cluster) {
    while (current_cluster != target_cluster && current_idx < route_size - 1) {
      current_idx++;
      target_cluster = flit.route_metadata.custom_data[1 + current_idx];
    }
    // Update the flit's in-buffer metadata
    flit.route_metadata.custom_data[0] = current_idx;
  }

  // Identify next cluster ID
  int next_cluster;
  if (current_idx + 1 < route_size) {
    next_cluster = flit.route_metadata.custom_data[1 + current_idx + 1];
  } else {
    next_cluster = getClusterId(dest_node_id);
  }

  // ==========================================
  // STEP 3: Routing case branching
  // ==========================================
  int destination_cluster = getClusterId(dest_node_id);
  vector<int> directions;

  if (current_cluster == destination_cluster) {
    // Case A: Inside destination cluster, route to final node
    directions = routeLocally(current_node_id, dest_node_id, routeData.src_id);
  } else {
    // Case B: Route to next cluster's exit node
    int sub_destination = getBestExitNode(current_cluster, next_cluster, dest_node_id);
    if (current_node_id == sub_destination) {
      // We are at exit node, cross cluster boundary
      directions = getBoundaryCrossingPort(router, current_node_id, next_cluster);
      if (directions.empty()) {
        // Fallback: if crossing link blocked, route locally towards sub-destination
        directions = routeLocally(current_node_id, sub_destination, routeData.src_id);
      }
    } else {
      // Inside current cluster, route locally to exit node
      directions = routeLocally(current_node_id, sub_destination, routeData.src_id);
    }
  }

  // Final fallback to prevent stalling in case of unmapped situations
  if (directions.empty()) {
    directions = routeLocally(current_node_id, dest_node_id, routeData.src_id);
  }

  return directions;
}
