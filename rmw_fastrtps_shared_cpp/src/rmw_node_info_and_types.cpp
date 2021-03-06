// Copyright 2019 Open Source Robotics Foundation, Inc.
// Copyright 2018 Amazon.com, Inc. or its affiliates. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <functional>
#include <map>
#include <set>
#include <string>
#include <utility>

#include "rcutils/allocator.h"
#include "rcutils/error_handling.h"
#include "rcutils/logging_macros.h"
#include "rcutils/strdup.h"
#include "rcutils/types.h"

#include "rmw/allocators.h"
#include "rmw/convert_rcutils_ret_to_rmw_ret.h"
#include "rmw/error_handling.h"
#include "rmw/get_topic_names_and_types.h"
#include "rmw/impl/cpp/macros.hpp"
#include "rmw/names_and_types.h"
#include "rmw/rmw.h"

#include "rmw_dds_common/context.hpp"

#include "demangle.hpp"
#include "rmw_fastrtps_shared_cpp/custom_participant_info.hpp"
#include "rmw_fastrtps_shared_cpp/names.hpp"
#include "rmw_fastrtps_shared_cpp/namespace_prefix.hpp"
#include "rmw_fastrtps_shared_cpp/rmw_common.hpp"
#include "rmw_fastrtps_shared_cpp/rmw_context_impl.hpp"

namespace rmw_fastrtps_shared_cpp
{

/**
 * Validate the input data of node_info_and_types functions.
 *
 * \return RMW_RET_INVALID_ARGUMENT for null input args
 * \return RMW_RET_ERROR if identifier is not the same as the input node
 * \return RMW_RET_OK if all input is valid
 */
rmw_ret_t __validate_input(
  const char * identifier,
  const rmw_node_t * node,
  rcutils_allocator_t * allocator,
  const char * node_name,
  const char * node_namespace,
  rmw_names_and_types_t * topic_names_and_types)
{
  if (!allocator) {
    RMW_SET_ERROR_MSG("allocator is null");
    return RMW_RET_INVALID_ARGUMENT;
  }
  if (!node) {
    RMW_SET_ERROR_MSG("null node handle");
    return RMW_RET_INVALID_ARGUMENT;
  }

  if (!node_name) {
    RMW_SET_ERROR_MSG("null node name");
    return RMW_RET_INVALID_ARGUMENT;
  }

  if (!node_namespace) {
    RMW_SET_ERROR_MSG("null node namespace");
    return RMW_RET_INVALID_ARGUMENT;
  }

  rmw_ret_t ret = rmw_names_and_types_check_zero(topic_names_and_types);
  if (ret != RMW_RET_OK) {
    return ret;
  }

  // Get participant pointer from node
  if (node->implementation_identifier != identifier) {
    RMW_SET_ERROR_MSG("node handle not from this implementation");
    return RMW_RET_ERROR;
  }
  return RMW_RET_OK;
}

using GetNamesAndTypesByNodeFunction = rmw_ret_t (*)(
  rmw_dds_common::Context *,
  const std::string &,
  const std::string &,
  DemangleFunction,
  DemangleFunction,
  rcutils_allocator_t *,
  rmw_names_and_types_t *);

rmw_ret_t
__rmw_get_topic_names_and_types_by_node(
  const char * identifier,
  const rmw_node_t * node,
  rcutils_allocator_t * allocator,
  const char * node_name,
  const char * node_namespace,
  DemangleFunction demangle_topic,
  DemangleFunction demangle_type,
  bool no_demangle,
  GetNamesAndTypesByNodeFunction get_names_and_types_by_node,
  rmw_names_and_types_t * topic_names_and_types)
{
  rmw_ret_t valid_input = __validate_input(
    identifier, node, allocator, node_name, node_namespace, topic_names_and_types);
  if (valid_input != RMW_RET_OK) {
    return valid_input;
  }
  auto common_context = static_cast<rmw_dds_common::Context *>(node->context->impl->common);

  if (no_demangle) {
    demangle_topic = _identity_demangle;
    demangle_type = _identity_demangle;
  }

  return get_names_and_types_by_node(
    common_context,
    node_name,
    node_namespace,
    demangle_topic,
    demangle_type,
    allocator,
    topic_names_and_types);
}

rmw_ret_t
__get_reader_names_and_types_by_node(
  rmw_dds_common::Context * common_context,
  const std::string & node_name,
  const std::string & node_namespace,
  DemangleFunction demangle_topic,
  DemangleFunction demangle_type,
  rcutils_allocator_t * allocator,
  rmw_names_and_types_t * topic_names_and_types)
{
  return common_context->graph_cache.get_reader_names_and_types_by_node(
    node_name,
    node_namespace,
    demangle_topic,
    demangle_type,
    allocator,
    topic_names_and_types);
}

rmw_ret_t
__get_writer_names_and_types_by_node(
  rmw_dds_common::Context * common_context,
  const std::string & node_name,
  const std::string & node_namespace,
  DemangleFunction demangle_topic,
  DemangleFunction demangle_type,
  rcutils_allocator_t * allocator,
  rmw_names_and_types_t * topic_names_and_types)
{
  return common_context->graph_cache.get_writer_names_and_types_by_node(
    node_name,
    node_namespace,
    demangle_topic,
    demangle_type,
    allocator,
    topic_names_and_types);
}

rmw_ret_t
__rmw_get_subscriber_names_and_types_by_node(
  const char * identifier,
  const rmw_node_t * node,
  rcutils_allocator_t * allocator,
  const char * node_name,
  const char * node_namespace,
  bool no_demangle,
  rmw_names_and_types_t * topic_names_and_types)
{
  return __rmw_get_topic_names_and_types_by_node(
    identifier,
    node,
    allocator,
    node_name,
    node_namespace,
    _demangle_ros_topic_from_topic,
    _demangle_if_ros_type,
    no_demangle,
    __get_reader_names_and_types_by_node,
    topic_names_and_types);
}

rmw_ret_t
__rmw_get_publisher_names_and_types_by_node(
  const char * identifier,
  const rmw_node_t * node,
  rcutils_allocator_t * allocator,
  const char * node_name,
  const char * node_namespace,
  bool no_demangle,
  rmw_names_and_types_t * topic_names_and_types)
{
  return __rmw_get_topic_names_and_types_by_node(
    identifier,
    node,
    allocator,
    node_name,
    node_namespace,
    _demangle_ros_topic_from_topic,
    _demangle_if_ros_type,
    no_demangle,
    __get_writer_names_and_types_by_node,
    topic_names_and_types);
}

rmw_ret_t
__rmw_get_service_names_and_types_by_node(
  const char * identifier,
  const rmw_node_t * node,
  rcutils_allocator_t * allocator,
  const char * node_name,
  const char * node_namespace,
  rmw_names_and_types_t * service_names_and_types)
{
  return __rmw_get_topic_names_and_types_by_node(
    identifier,
    node,
    allocator,
    node_name,
    node_namespace,
    _demangle_service_request_from_topic,
    _demangle_service_type_only,
    false,
    __get_reader_names_and_types_by_node,
    service_names_and_types);
}

rmw_ret_t
__rmw_get_client_names_and_types_by_node(
  const char * identifier,
  const rmw_node_t * node,
  rcutils_allocator_t * allocator,
  const char * node_name,
  const char * node_namespace,
  rmw_names_and_types_t * service_names_and_types)
{
  return __rmw_get_topic_names_and_types_by_node(
    identifier,
    node,
    allocator,
    node_name,
    node_namespace,
    _demangle_service_reply_from_topic,
    _demangle_service_type_only,
    false,
    __get_reader_names_and_types_by_node,
    service_names_and_types);
}

}  // namespace rmw_fastrtps_shared_cpp
