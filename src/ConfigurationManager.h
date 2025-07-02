/*
 * Noxim - the NoC Simulator
 *
 * (C) 2005-2018 by the University of Catania
 * For the complete list of authors refer to file ../doc/AUTHORS.txt
 * For the license applied to these sources refer to file ../doc/LICENSE.txt
 *
 * This file contains the declaration of the global params needed by Noxim
 * to forward configuration to every sub-block
 */

#ifndef __NOXIMCONFIGURATIONMANAGER_H__
#define __NOXIMCONFIGURATIONMANAGER_H__

#include "yaml-cpp/yaml.h"
#include "GlobalParams.h"

#include <iostream>
#include <vector>
#include <string>
#include <utility>

using namespace std;

void configure(int arg_num, char *arg_vet[]);

template <typename T> 
T readParam(YAML::Node node, string param, T default_value);

template <typename T> 
T readParam(YAML::Node node, string param);

namespace YAML {
    template<>
    struct convert<HubConfig> {
        static Node encode(const HubConfig& hubConfig) {
            Node node;
            node["attached_nodes"] = hubConfig.attachedNodes;
            node["rx_radio_channels"] = hubConfig.rxChannels;
            node["tx_radio_channels"] = hubConfig.txChannels;
            node["to_tile_buffer_size"] = hubConfig.toTileBufferSize;
            node["from_tile_buffer_size"] = hubConfig.fromTileBufferSize;
            node["tx_buffer_size"] = hubConfig.txBufferSize;
            node["rx_buffer_size"] = hubConfig.rxBufferSize;
            return node;
        }

        static bool decode(const Node& node, HubConfig& hubConfig) {
            hubConfig.attachedNodes = node["attached_nodes"].as<vector<int> >(GlobalParams::default_hub_configuration.attachedNodes);
            hubConfig.rxChannels = node["rx_radio_channels"].as<vector<int> >(GlobalParams::default_hub_configuration.rxChannels);
            hubConfig.txChannels = node["tx_radio_channels"].as<vector<int> >(GlobalParams::default_hub_configuration.txChannels);
            hubConfig.toTileBufferSize = node["to_tile_buffer_size"].as<int>(GlobalParams::default_hub_configuration.toTileBufferSize);
            hubConfig.fromTileBufferSize = node["from_tile_buffer_size"].as<int>(GlobalParams::default_hub_configuration.fromTileBufferSize);
            hubConfig.txBufferSize = node["tx_buffer_size"].as<int>(GlobalParams::default_hub_configuration.txBufferSize);
            hubConfig.rxBufferSize = node["rx_buffer_size"].as<int>(GlobalParams::default_hub_configuration.rxBufferSize);
            return true;
        }
    };
    
    template<>
    struct convert<ChannelConfig> {
        static Node encode(const ChannelConfig& channelConfig) {
            Node node;
            node["ber"] = channelConfig.ber;
            node["data_rate"] = channelConfig.dataRate;
            node["mac_policy"] = channelConfig.macPolicy;
            return node;
        }

        static bool decode(const Node& node, ChannelConfig& channelConfig) {
            channelConfig.ber = node["ber"].as<pair<double, double> >(GlobalParams::default_channel_configuration.ber);
            channelConfig.dataRate = node["data_rate"].as<int>(GlobalParams::default_channel_configuration.dataRate);
            channelConfig.macPolicy = node["mac_policy"].as<vector<string> >(GlobalParams::default_channel_configuration.macPolicy);
            return true;
        }
    };
}
#endif
