#ifndef __NOXIMROUTING_CLUSTER_H__
#define __NOXIMROUTING_CLUSTER_H__

#include "RoutingAlgorithm.h"
#include "RoutingAlgorithms.h"
#include "../Router.h"

using namespace std;

class Routing_CLUSTER : RoutingAlgorithm {
  public:
    vector<int> route(Router * router, Flit & flit, const RouteData & routeData);

    static Routing_CLUSTER * getInstance();

  private:
    Routing_CLUSTER(){};
    ~Routing_CLUSTER(){};

    static Routing_CLUSTER * routing_CLUSTER;
    static RoutingAlgorithmsRegister routingAlgorithmsRegister;
};

#endif
