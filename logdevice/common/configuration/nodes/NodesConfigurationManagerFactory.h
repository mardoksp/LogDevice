/**
 * Copyright (c) 2018-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */
#pragma once

#include <string>

#include "logdevice/common/configuration/nodes/NodesConfigurationManager.h"
#include "logdevice/common/configuration/nodes/ServiceDiscoveryConfig.h"

namespace facebook { namespace logdevice {

class Configuration;
class Processor;
struct Settings;

namespace configuration { namespace nodes {

class NodesConfigurationStoreFactory {
 public:
  enum class NCSType { Zookeeper = 0, File, Server };

  struct Params {
    NCSType type;

    // used when type == NCSType::File, NCS will be created as
    // ZookeeperNodesConfigurationStore with the specified zookeeper config
    std::shared_ptr<configuration::ZookeeperConfig> zk_config;

    // used when type == NCSType::File, NCS will be created as
    // FileBasedNodesConfiguratinStore under the specified path.
    // Used in integration test only.
    std::string file_store_path;

    bool isValid() const;
  };

  static std::unique_ptr<NodesConfigurationStore>
  create(Params params) noexcept;

  /**
   * utility function that creates the store with params deducted from
   * config and settings
   */
  static std::unique_ptr<NodesConfigurationStore>
  create(const Configuration& config, const Settings& settings) noexcept;
};

class NodesConfigurationManagerFactory {
 public:
  /**
   * Create a NodesConfigurationManager with given NodesConfigurationStore and
   * other given parameters.
   */
  static std::shared_ptr<NodesConfigurationManager>
  create(NodesConfigurationManager::OperationMode mode,
         Processor* processor,
         std::unique_ptr<NodesConfigurationStore> store) noexcept;

  /**
   * Convenient utility that create NodesConfigurationManager as well as its
   * underlying NodesConfigurationStore together.
   */
  static std::shared_ptr<NodesConfigurationManager>
  create(NodesConfigurationManager::OperationMode mode,
         Processor* processor,
         NodesConfigurationStoreFactory::Params params) noexcept;

  /**
   * utility function that creates the NodesConfigurationManager with params
   * deducted from Processor config and settings.
   *
   * @param roles    if the NCM is created for a server, _roles_ must be
   *                 provided to infer the NCM operational modes
   */
  static std::shared_ptr<NodesConfigurationManager>
  create(Processor* processor,
         std::unique_ptr<configuration::nodes::NodesConfigurationStore> store,
         folly::Optional<NodeServiceDiscovery::RoleSet> roles) noexcept;
};

}} // namespace configuration::nodes
}} // namespace facebook::logdevice
