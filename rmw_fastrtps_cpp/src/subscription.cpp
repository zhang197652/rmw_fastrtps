// Copyright 2019 Open Source Robotics Foundation, Inc.
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

#include <string>
#include <utility>

#include "rcutils/allocator.h"
#include "rcutils/strdup.h"

#include "rmw/allocators.h"
#include "rmw/error_handling.h"
#include "rmw/rmw.h"

#include "rmw_fastrtps_shared_cpp/custom_participant_info.hpp"
#include "rmw_fastrtps_shared_cpp/custom_subscriber_info.hpp"
#include "rmw_fastrtps_shared_cpp/names.hpp"
#include "rmw_fastrtps_shared_cpp/namespace_prefix.hpp"
#include "rmw_fastrtps_shared_cpp/qos.hpp"
#include "rmw_fastrtps_shared_cpp/rmw_common.hpp"

#include "fastrtps/participant/Participant.h"
#include "fastrtps/subscriber/Subscriber.h"

#include "rmw_fastrtps_cpp/identifier.hpp"
#include "rmw_fastrtps_cpp/subscription.hpp"

#include "type_support_common.hpp"

using Domain = eprosima::fastrtps::Domain;
using Participant = eprosima::fastrtps::Participant;
using TopicDataType = eprosima::fastrtps::TopicDataType;


namespace rmw_fastrtps_cpp
{

rmw_subscription_t *
create_subscription(
  const CustomParticipantInfo * participant_info,
  const rosidl_message_type_support_t * type_supports,
  const char * topic_name,
  const rmw_qos_profile_t * qos_policies,
  const rmw_subscription_options_t * subscription_options,
  bool keyed,
  bool create_subscription_listener)
{
  if (!topic_name || strlen(topic_name) == 0) {
    RMW_SET_ERROR_MSG("subscription topic is null or empty string");
    return nullptr;
  }
  if (!qos_policies) {
    RMW_SET_ERROR_MSG("qos_policies is null");
    return nullptr;
  }
  if (!subscription_options) {
    RMW_SET_ERROR_MSG("subscription_options is null");
    return nullptr;
  }
  if (!participant_info) {
    RMW_SET_ERROR_MSG("participant_info is null");
    return nullptr;
  }
  Participant * participant = participant_info->participant;
  if (!participant) {
    RMW_SET_ERROR_MSG("participant handle is null");
    return nullptr;
  }
  const rosidl_message_type_support_t * type_support = get_message_typesupport_handle(
    type_supports, RMW_FASTRTPS_CPP_TYPESUPPORT_C);
  if (!type_support) {
    type_support = get_message_typesupport_handle(
      type_supports, RMW_FASTRTPS_CPP_TYPESUPPORT_CPP);
    if (!type_support) {
      RMW_SET_ERROR_MSG("type support not from this implementation");
      return nullptr;
    }
  }
  if (!is_valid_qos(*qos_policies)) {
    return nullptr;
  }
  CustomSubscriberInfo * info = nullptr;
  rmw_subscription_t * rmw_subscription = nullptr;
  eprosima::fastrtps::SubscriberAttributes subscriberParam;

  // Load default XML profile.
  Domain::getDefaultSubscriberAttributes(subscriberParam);
  info = new (std::nothrow) CustomSubscriberInfo();
  if (!info) {
    RMW_SET_ERROR_MSG("failed to allocate CustomSubscriberInfo");
    return nullptr;
  }
  info->typesupport_identifier_ = type_support->typesupport_identifier;
  info->type_support_impl_ = type_support->data;

  auto callbacks = static_cast<const message_type_support_callbacks_t *>(type_support->data);
  std::string type_name = _create_type_name(callbacks);
  if (
    !Domain::getRegisteredType(
      participant, type_name.c_str(),
      reinterpret_cast<TopicDataType **>(&info->type_support_)))
  {
    info->type_support_ = new (std::nothrow) MessageTypeSupport_cpp(callbacks);
    if (!info->type_support_) {
      RMW_SET_ERROR_MSG("failed to allocate MessageTypeSupport_cpp");
      goto fail;
    }
    _register_type(participant, info->type_support_);
  }
  if (!participant_info->leave_middleware_default_qos) {
    subscriberParam.historyMemoryPolicy =
      eprosima::fastrtps::rtps::PREALLOCATED_WITH_REALLOC_MEMORY_MODE;
  }

  subscriberParam.topic.topicKind =
    keyed ? eprosima::fastrtps::rtps::WITH_KEY : eprosima::fastrtps::rtps::NO_KEY;
  subscriberParam.topic.topicDataType = type_name;
  subscriberParam.topic.topicName = _create_topic_name(qos_policies, ros_topic_prefix, topic_name);

  if (!get_datareader_qos(*qos_policies, subscriberParam)) {
    RMW_SET_ERROR_MSG("failed to get datareader qos");
    goto fail;
  }
  info->listener_ = nullptr;
  if (create_subscription_listener) {
    info->listener_ = new (std::nothrow) SubListener(info);
    if (!info->listener_) {
      RMW_SET_ERROR_MSG("create_subscriber() could not create subscriber listener");
      goto fail;
    }
  }
  info->subscriber_ = Domain::createSubscriber(participant, subscriberParam, info->listener_);
  if (!info->subscriber_) {
    RMW_SET_ERROR_MSG("create_subscriber() could not create subscriber");
    goto fail;
  }
  info->subscription_gid_ = rmw_fastrtps_shared_cpp::create_rmw_gid(
    eprosima_fastrtps_identifier, info->subscriber_->getGuid());
  rmw_subscription = rmw_subscription_allocate();
  if (!rmw_subscription) {
    RMW_SET_ERROR_MSG("failed to allocate subscription");
    goto fail;
  }
  rmw_subscription->implementation_identifier = eprosima_fastrtps_identifier;
  rmw_subscription->data = info;
  rmw_subscription->topic_name = rcutils_strdup(topic_name, rcutils_get_default_allocator());
  if (!rmw_subscription->topic_name) {
    RMW_SET_ERROR_MSG("failed to allocate memory for subscription topic name");
    goto fail;
  }

  rmw_subscription->options = *subscription_options;
  return rmw_subscription;

fail:
  if (info != nullptr) {
    delete info->type_support_;
    delete info->listener_;
    delete info;
  }
  rmw_subscription_free(rmw_subscription);
  return nullptr;
}
}  // namespace rmw_fastrtps_cpp
