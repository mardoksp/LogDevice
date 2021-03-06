/**
 * Copyright (c) 2017-present, Facebook, Inc. and its affiliates.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "logdevice/admin/CheckImpactHandler.h"

#include "logdevice/admin/AdminAPIUtils.h"
#include "logdevice/admin/Conv.h"
#include "logdevice/admin/safety/CheckImpactRequest.h"
#include "logdevice/common/configuration/UpdateableConfig.h"
#include "logdevice/common/event_log/EventLogRebuildingSet.h"
#include "logdevice/server/ServerProcessor.h"

using namespace facebook::logdevice::configuration;

namespace facebook { namespace logdevice {

folly::SemiFuture<std::unique_ptr<thrift::CheckImpactResponse>>
CheckImpactHandler::semifuture_checkImpact(
    std::unique_ptr<thrift::CheckImpactRequest> request) {
  // Fulfilling this promise means that we have a response.
  folly::Promise<std::unique_ptr<thrift::CheckImpactResponse>> promise;
  std::shared_ptr<EventLogRebuildingSet> rebuilding_set =
      processor_->rebuilding_set_.get();
  // Rebuilding Set can be nullptr if rebuilding is disabled, or if event_log is
  // not ready yet.
  if (!rebuilding_set) {
    // We can't move on if we don't have the current AuthoritativeStatus. We
    // throw NotReady in this case.
    thrift::NodeNotReady err;
    err.set_message("Node does not have the shard states yet, try a "
                    "different node");
    promise.setException(std::move(err));
    return promise.getSemiFuture();
  }

  auto server_config = processor_->config_->getServerConfig();
  ShardAuthoritativeStatusMap status_map =
      rebuilding_set->toShardStatusMap(server_config->getNodes());

  // Resolve shards from thrift to logdevice
  ShardSet shards =
      expandShardSet(request->get_shards(), server_config->getNodes());

  ld_info("SHARDS: %s", toString(shards).c_str());

  StorageState target_storage_state = StorageState::READ_WRITE;
  if (request->get_target_storage_state()) {
    target_storage_state =
        toLogDevice<StorageState>(*request->get_target_storage_state());
  }

  // Convert thrift::ReplicationProperty into logdevice's SafetyMargin
  SafetyMargin safety_margin;
  if (request->get_safety_margin()) {
    safety_margin = safetyMarginFromReplication(
        toLogDevice<ReplicationProperty>(*request->get_safety_margin()));
  }
  // logids to check
  folly::Optional<std::vector<logid_t>> logs_to_check;
  if (request->get_log_ids_to_check()) {
    logs_to_check = std::vector<logid_t>();
    for (thrift::unsigned64 id : *request->get_log_ids_to_check()) {
      logs_to_check->push_back(logid_t(to_unsigned(id)));
    }
  }

  bool check_metadata_logs = true;
  if (request->get_check_metadata_logs()) {
    check_metadata_logs = *request->get_check_metadata_logs();
  }
  bool check_internal_logs = true;
  if (request->get_check_internal_logs()) {
    check_internal_logs = *request->get_check_internal_logs();
  }

  // It's important that we get the SemiFuture before we move the promise
  // out of this function.
  auto future = promise.getSemiFuture();
  // Promise is moved into the lambda as it's not copieable. Don't use promise
  // after this point.
  auto cb = [p = std::move(promise)](Impact impact) mutable {
    if (impact.status != E::OK) {
      // An error has happened.
      thrift::OperationError ex;
      ex.set_error_code(static_cast<uint16_t>(impact.status));
      ex.set_message(error_description(impact.status));
      p.setException(std::move(ex));
      return;
    }
    std::vector<thrift::ImpactOnEpoch> logs_affected;

    for (const auto& epoch_info : impact.logs_affected) {
      logs_affected.push_back(toThrift<thrift::ImpactOnEpoch>(epoch_info));
    }
    std::unique_ptr<thrift::CheckImpactResponse> response =
        std::make_unique<thrift::CheckImpactResponse>();
    response->set_impact(
        toThrift<std::vector<thrift::OperationImpact>>(impact.result));
    response->set_internal_logs_affected(impact.internal_logs_affected);
    response->set_logs_affected(std::move(logs_affected));
    response->set_total_duration(impact.total_duration.count());
    response->set_total_logs_checked(impact.total_logs_checked);
    p.setValue(std::move(response));
  };

  std::unique_ptr<Request> rq = std::make_unique<CheckImpactRequest>(
      std::move(status_map),
      std::move(shards),
      target_storage_state,
      safety_margin,
      check_metadata_logs,
      check_internal_logs,
      logs_to_check,
      updateable_admin_server_settings_->safety_max_logs_in_flight,
      request->abort_on_negative_impact_ref().value_unchecked(),
      updateable_admin_server_settings_->safety_check_timeout,
      request->return_sample_size_ref().value_unchecked(),
      updateable_admin_server_settings_->read_metadata_from_sequencers,
      CheckImpactRequest::workerType(processor_),
      std::move(cb));
  int rv = processor_->postRequest(rq);
  if (rv != 0) {
    // We couldn't submit the request to the processor.
    ld_error("We couldn't submit the CheckImpactRequest to the logdevice "
             "processor: %s",
             error_description(err));
    ld_check(err != E::OK);
    thrift::OperationError ex;
    ex.set_error_code(static_cast<uint16_t>(err));
    ex.set_message(error_description(err));
    folly::Promise<std::unique_ptr<thrift::CheckImpactResponse>> err_promise;
    err_promise.setException(std::move(ex));
    return err_promise.getSemiFuture();
  }
  return future;
}
}} // namespace facebook::logdevice
