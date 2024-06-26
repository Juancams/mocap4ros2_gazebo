// Copyright 2022 Intelligent Robotics Lab
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

// Author: Jose Miguel Guerrero Hernandez <josemiguel.guerrero@urjc.es>


#include <memory>
#include <string>
#include <utility>

#include <gazebo/physics/Model.hh>
#include <gazebo_plugins/gazebo_ros_mocap.hpp>
#include <gazebo_ros/node.hpp>

#include "mocap4r2_msgs/msg/markers.hpp"
#include "mocap4r2_msgs/msg/rigid_bodies.hpp"
#include "lifecycle_msgs/msg/state.hpp"

#include "rclcpp/rclcpp.hpp"
#include "mocap4r2_control/ControlledLifecycleNode.hpp"

namespace gazebo
{

GZ_REGISTER_MODEL_PLUGIN(GazeboRosMocap)

using CallbackReturnT =
  rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn;

/// Class to hold private data members (PIMPL pattern)
class GazeboRosMocapPrivate : public mocap4r2_control::ControlledLifecycleNode
{
public:
  GazeboRosMocapPrivate();

  CallbackReturnT on_configure(const rclcpp_lifecycle::State & state) override;
  CallbackReturnT on_activate(const rclcpp_lifecycle::State & state) override;
  CallbackReturnT on_deactivate(const rclcpp_lifecycle::State & state) override;

  void control_start(const mocap4r2_control_msgs::msg::Control::SharedPtr msg) override;
  void control_stop(const mocap4r2_control_msgs::msg::Control::SharedPtr msg) override;

  /// Connection to world update event. Callback is called while this is alive.
  gazebo::event::ConnectionPtr update_connection_;
  physics::WorldPtr world_;
  physics::LinkPtr link_;
  std::string link_name_;

  /// ROS communication.
  rclcpp_lifecycle::LifecyclePublisher<mocap4r2_msgs::msg::Markers>::SharedPtr mocap_markers_pub_;
  rclcpp_lifecycle::LifecyclePublisher<mocap4r2_msgs::msg::RigidBodies>::SharedPtr
    mocap_rigid_body_pub_;
  int frame_number_{0};
};

GazeboRosMocapPrivate::GazeboRosMocapPrivate()
: ControlledLifecycleNode("gazebo_control")
{
  trigger_transition(lifecycle_msgs::msg::Transition::TRANSITION_CONFIGURE);
}


CallbackReturnT
GazeboRosMocapPrivate::on_configure(const rclcpp_lifecycle::State & state)
{
  mocap_markers_pub_ = create_publisher<mocap4r2_msgs::msg::Markers>(
    "markers", rclcpp::QoS(1000));
  mocap_rigid_body_pub_ = create_publisher<mocap4r2_msgs::msg::RigidBodies>(
    "rigid_bodies", rclcpp::QoS(1000));

  return ControlledLifecycleNode::on_configure(state);
}

CallbackReturnT
GazeboRosMocapPrivate::on_activate(const rclcpp_lifecycle::State & state)
{
  (void)state;

  mocap_markers_pub_->on_activate();
  mocap_rigid_body_pub_->on_activate();

  return ControlledLifecycleNode::on_activate(state);
}

CallbackReturnT
GazeboRosMocapPrivate::on_deactivate(const rclcpp_lifecycle::State & state)
{
  (void)state;

  mocap_markers_pub_->on_deactivate();
  mocap_rigid_body_pub_->on_deactivate();

  return ControlledLifecycleNode::on_deactivate(state);
}

void
GazeboRosMocapPrivate::control_start(const mocap4r2_control_msgs::msg::Control::SharedPtr msg)
{
  RCLCPP_INFO(get_logger(), "Starting mocap gazebo");
}

void
GazeboRosMocapPrivate::control_stop(const mocap4r2_control_msgs::msg::Control::SharedPtr msg)
{
  RCLCPP_INFO(get_logger(), "Stopping mocap gazebo");
}

GazeboRosMocap::GazeboRosMocap()
: impl_(std::make_unique<GazeboRosMocapPrivate>())
{
}

GazeboRosMocap::~GazeboRosMocap()
{
  impl_->update_connection_.reset();
}

physics::LinkPtr find_link(gazebo::physics::Link_V & links, const std::string & name)
{
  if (links.empty()) {
    return nullptr;
  }

  for (const auto & link : links) {
    std::cerr << "Link: " << link->GetName() << std::endl;
    if (link->GetName() == name) {
      return link;
    } else {
      auto child_links = link->GetChildJointsLinks();
      std::cerr << "And has " << child_links.size() << "childs:";
      for (const auto & child_link : child_links) {
        std::cerr << " " << child_link->GetName();
      }
      std::cerr << std::endl;

      auto found_link = find_link(child_links, name);
      if (found_link != nullptr) {
        return found_link;
      }
    }
  }

  return nullptr;
}

void GazeboRosMocap::Load(physics::ModelPtr _parent, sdf::ElementPtr sdf)
{
  impl_->update_connection_ = gazebo::event::Events::ConnectWorldUpdateBegin(
    std::bind(&GazeboRosMocap::OnUpdate, this));

  auto logger = impl_->get_logger();

  if (!sdf->HasElement("link_name")) {
    RCLCPP_INFO(
      logger, "Gidmap plugin missing <link_name> wasn't set,"
      "therefore it's been set as 'base_mocap'.");
    impl_->link_name_ = "base_mocap";
  } else {
    impl_->link_name_ = sdf->GetElement("link_name")->Get<std::string>();
  }
  impl_->world_ = _parent->GetWorld();

  for (const auto & model : impl_->world_->Models()) {
    auto links = model->GetLinks();
    impl_->link_ = find_link(links, impl_->link_name_);

    if (impl_->link_ != nullptr) {
      break;
    }
  }

  if (impl_->link_ != nullptr) {
    RCLCPP_INFO(logger, "Plugin MOCAP loaded for [%s] body", impl_->link_name_.c_str());
  } else {
    RCLCPP_ERROR(logger, "No link [%s] found for Plugin MOCAP", impl_->link_name_.c_str());
  }
}

void GazeboRosMocap::OnUpdate()
{
  rclcpp::spin_some(impl_->get_node_base_interface());

  if (impl_->get_current_state().id() != lifecycle_msgs::msg::State::PRIMARY_STATE_ACTIVE) {
    return;
  }

  ignition::math::v6::Pose3d pose = impl_->link_->WorldPose();

  auto & pos = pose.Pos();
  auto & rot = pose.Rot();

  if (impl_->mocap_markers_pub_->get_subscription_count() > 0) {
    mocap4r2_msgs::msg::Marker m1;
    m1.id_type = mocap4r2_msgs::msg::Marker::USE_INDEX;
    m1.marker_index = 1;
    m1.translation.x = pos.X();
    m1.translation.y = pos.Y();
    m1.translation.z = pos.Z() + 0.05;

    mocap4r2_msgs::msg::Marker m2;
    m2.id_type = mocap4r2_msgs::msg::Marker::USE_INDEX;
    m2.marker_index = 2;
    m2.translation.x = pos.X() + 0.02;
    m2.translation.y = pos.Y();
    m2.translation.z = pos.Z() + 0.03;

    mocap4r2_msgs::msg::Marker m3;
    m3.id_type = mocap4r2_msgs::msg::Marker::USE_INDEX;
    m3.marker_index = 3;
    m3.translation.x = pos.X();
    m3.translation.y = pos.Y() + 0.015;
    m3.translation.z = pos.Z() + 0.03;

    mocap4r2_msgs::msg::Markers ms;
    ms.header.stamp = impl_->now();
    ms.header.frame_id = "map";
    ms.frame_number = impl_->frame_number_++;
    ms.markers = {m1, m2, m3};

    impl_->mocap_markers_pub_->publish(ms);
  }

  if (impl_->mocap_rigid_body_pub_->get_subscription_count() > 0) {
    mocap4r2_msgs::msg::RigidBody rb1;
    rb1.rigid_body_name = "rigid_body_1";
    rb1.pose.position.x = pos.X();
    rb1.pose.position.y = pos.Y();
    rb1.pose.position.z = pos.Z();
    rb1.pose.orientation.x = rot.X();
    rb1.pose.orientation.y = rot.Y();
    rb1.pose.orientation.z = rot.Z();
    rb1.pose.orientation.w = rot.W();

    mocap4r2_msgs::msg::Marker m1;
    m1.id_type = mocap4r2_msgs::msg::Marker::USE_INDEX;
    m1.marker_index = 1;
    m1.translation.x = pos.X();
    m1.translation.y = pos.Y();
    m1.translation.z = pos.Z() + 0.05;

    mocap4r2_msgs::msg::Marker m2;
    m2.id_type = mocap4r2_msgs::msg::Marker::USE_INDEX;
    m2.marker_index = 2;
    m2.translation.x = pos.X() + 0.02;
    m2.translation.y = pos.Y();
    m2.translation.z = pos.Z() + 0.03;

    mocap4r2_msgs::msg::Marker m3;
    m3.id_type = mocap4r2_msgs::msg::Marker::USE_INDEX;
    m3.marker_index = 3;
    m3.translation.x = pos.X();
    m3.translation.y = pos.Y() + 0.015;
    m3.translation.z = pos.Z() + 0.03;

    rb1.markers = {m1, m2, m3};

    mocap4r2_msgs::msg::RigidBodies rbs;
    rbs.header.stamp = impl_->now();
    rbs.header.frame_id = "map";
    rbs.frame_number = impl_->frame_number_++;
    rbs.rigidbodies = {rb1};

    impl_->mocap_rigid_body_pub_->publish(rbs);
  }
}

}  // namespace gazebo
