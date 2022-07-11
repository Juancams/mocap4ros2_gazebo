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

#ifndef GAZEBO_PLUGINS__GAZEBO_ROS_MOCAP_HPP_
#define GAZEBO_PLUGINS__GAZEBO_ROS_MOCAP_HPP_

// For std::unique_ptr, could be removed
#include <memory>

#include "gazebo/physics/physics.hh"
#include "gazebo/common/common.hh"
#include "gazebo/gazebo.hh"


namespace gazebo
{

// Forward declaration of private data class.
class GazeboRosMocapPrivate;

class GazeboRosMocap : public ModelPlugin
{
public:
  GazeboRosMocap();

  virtual ~GazeboRosMocap();

  void Load(physics::ModelPtr _parent, sdf::ElementPtr sdf);

protected:
  virtual void OnUpdate();

private:
  /// Recommended PIMPL pattern. This variable should hold all private
  /// data members.
  std::unique_ptr<GazeboRosMocapPrivate> impl_;
};

}  // namespace gazebo

#endif  // GAZEBO_PLUGINS__GAZEBO_ROS_MOCAP_HPP_
