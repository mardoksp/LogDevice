/**
 * Copyright (c) 2017-present, Facebook, Inc. and its affiliates.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */
#pragma once

#include "logdevice/common/configuration/Configuration.h"
#include "logdevice/common/configuration/UpdateableConfig.h"
#include "logdevice/common/configuration/nodes/NodesConfigurationStore.h"
#include "logdevice/common/settings/Settings.h"

namespace facebook { namespace logdevice {

class Processor;

/**
 * NodesConfigurationInit is the class responsible for fetching the very first
 * NodesConfiguration that the client will use to get bootstrapped.
 * Given that this process happens during the bootstrapping phase of the
 * client, this is not called in a context of a processor, that's why
 * it initializes its own dummy processor to fetch the configuration.
 */
class NodesConfigurationInit {
 public:
  explicit NodesConfigurationInit(
      std::shared_ptr<configuration::nodes::NodesConfigurationStore> store)
      : store_(std::move(store)) {}

  virtual ~NodesConfigurationInit() = default;

  /**
   * Updates the passed updatable `nodes_configuration_config` with the
   * NodesConfiguration fetched using the following process:
   * 1. The server seed string is parsed and a list of initial servers are
   * fetched.
   * 2. We build a dummy ServerConfig with this list of servers, as simple as
   * it takes to satisfy the parser.
   * 3. Using the dummy config, we start a dummy processor.
   * 4. On one of the workers of the processor, we invoke a getConfig on the
   * NodesConfigurationStore of the class.
   * 5. We synchronously wait for the config to be fetched and parsed.
   * 6. When the config is successfully parsed, we update the
   * UpdateableNodesConfiguration.
   *
   * @return true on success, false otherwise.
   */
  bool
  init(std::shared_ptr<UpdateableNodesConfiguration> nodes_configuration_config,
       const std::string& server_seed_str);

 protected:
  std::shared_ptr<UpdateableConfig>
  buildDummyServerConfig(const std::vector<std::string>& host_list) const;

  // Used by the unit tests to inject extra settings to the created processor
  virtual void injectExtraSettings(Settings&) const {}

 private:
  // Takes a comma separated list of host/port pair and splits them.
  // TODO Support differet seeds (e.g. SMC) using plugins.
  std::vector<std::string> parseAndFetchHostList(const std::string& seed) const;

  std::shared_ptr<Processor>
  buildDummyProcessor(std::shared_ptr<UpdateableConfig> config) const;

  std::shared_ptr<const configuration::nodes::NodesConfiguration>
  parseNodesConfiguration(const std::string& config) const;

 private:
  std::shared_ptr<configuration::nodes::NodesConfigurationStore> store_;
};

}} // namespace facebook::logdevice
