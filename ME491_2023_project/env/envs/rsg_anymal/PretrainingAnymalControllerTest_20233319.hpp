// Copyright (c) 2020 Robotics and Artificial Intelligence Lab, KAIST
//
// Any unauthorized copying, alteration, distribution, transmission,
// performance, display or use of this material is prohibited.
//
// All rights reserved.

#pragma once

#include <set>
#include "../../BasicEigenTypes.hpp"
#include "raisim/World.hpp"
#include "math.h"

namespace raisim {

/// change the class name and file name ex) AnymalController_00000000 -> AnymalController_STUDENT_ID
class PretrainingAnymalControllerTest_20233319 {

 public:
  inline bool create(raisim::World *world) {
    anymal_ = reinterpret_cast<raisim::ArticulatedSystem *>(world->getObject(name_));
    cage_ = reinterpret_cast<raisim::Visuals *>(world->getObject("cage"));
    opponent_ = reinterpret_cast<raisim::ArticulatedSystem *>(world->getObject(opponentName_));
    /// get robot data
    gcDim_ = anymal_->getGeneralizedCoordinateDim();
    gvDim_ = anymal_->getDOF();
    nJoints_ = gvDim_ - 6;

    /// initialize containers
    gc_.setZero(gcDim_);
    gc_init_.setZero(gcDim_);
    gv_.setZero(gvDim_);
    gv_init_.setZero(gvDim_);
    pTarget_.setZero(gcDim_);
    vTarget_.setZero(gvDim_);
    pTarget12_.setZero(nJoints_);
    previousAction_.setZero(nJoints_);
    prepreviousAction_.setZero(nJoints_);

    /// this is nominal configuration of anymal
    gc_init_ << 0, 0, 0.50, 1.0, 0.0, 0.0, 0.0, 0.03, 0.4, -0.8, -0.03, 0.4, -0.8, 0.03, -0.4, 0.8, -0.03, -0.4, 0.8;

    opponent_gc_.setZero(gcDim_);
    opponent_gv_.setZero(gvDim_);
    opponent_gc_init_.setZero(gcDim_);

    cage2base_pos_xy_.setZero(2);
    opponent_cage2base_pos_xy_.setZero(2);

    /// set pd gains
    Eigen::VectorXd jointPgain(gvDim_), jointDgain(gvDim_);
    jointPgain.setZero();
    jointPgain.tail(nJoints_).setConstant(50.0);
    jointDgain.setZero();
    jointDgain.tail(nJoints_).setConstant(0.2);
    anymal_->setPdGains(jointPgain, jointDgain);
    anymal_->setGeneralizedForce(Eigen::VectorXd::Zero(gvDim_));

    /// MUST BE DONE FOR ALL ENVIRONMENTS
    obDim_ = 74;
    actionDim_ = nJoints_;
    actionMean_.setZero(actionDim_);
    actionStd_.setZero(actionDim_);
    obDouble_.setZero(obDim_);

    /// action scaling
    actionMean_ = gc_init_.tail(nJoints_);
    actionStd_.setConstant(0.1);

    /// indices of links that should not make contact with ground
    footIndices_.insert(anymal_->getBodyIdx("LF_SHANK"));
    footIndices_.insert(anymal_->getBodyIdx("RF_SHANK"));
    footIndices_.insert(anymal_->getBodyIdx("LH_SHANK"));
    footIndices_.insert(anymal_->getBodyIdx("RH_SHANK"));

    return true;
  }

  inline bool init(raisim::World *world) {
    return true;
  }

  inline bool advance(raisim::World *world, const Eigen::Ref<EigenVec> &action) {
    /// action scaling

    prepreviousAction_ = previousAction_;
    previousAction_ = action.cast<double>();

    pTarget12_ = action.cast<double>();
    pTarget12_ = pTarget12_.cwiseProduct(actionStd_);
    pTarget12_ += actionMean_;
    pTarget_.tail(nJoints_) = pTarget12_;

    anymal_->setPdTarget(pTarget_, vTarget_);
//    opponent_->setPdTarget(actionMean_, vTarget_);

    if(israndomizeBoxVelocity) randomizeBoxVelocity(world);

    return true;
  }

  inline bool reset(raisim::World *world, double theta) {

    curriculumLevelUpdate();

    currentTime_ = 0.;
    boxExternalForce_.setZero();

    if (playerNum_ == 0) {
      gc_init_.head(3) << cage_radius_ / 2 * std::cos(theta), cage_radius_ / 2 * std::sin(theta), 0.5;
      gc_init_.segment(3, 4) << cos((theta - M_PI) / 2), 0, 0, sin((theta - M_PI) / 2);
    }
    else {
      gc_init_.head(3) << 1.5 * std::cos(theta + M_PI), 1.5 * std::sin(theta + M_PI), 0.5;
      gc_init_.segment(3, 4) << cos(theta / 2), 0, 0, sin(theta / 2);
    }

    if(israndomizeBoxPosition) randomizeBoxPosition(world, theta);
    else{
      if(isOpponentAnymal_){
        opponent_gc_init_.head(3) << 0,0,0.5;
        opponent_gc_init_.segment(3, 4) << cos(theta / 2), 0, 0, sin(theta / 2);

      }
      else {
//        opponent_->setPosition(0, 0, 0.5);
//        opponent_->setOrientation(cos(theta / 2), 0, 0, sin(theta / 2));
//        opponent_gc_init_.head(3) << 0, 0, 0;//1.5 * std::cos(theta), 1.5 * std::sin(theta), 0.5;
      }
    }
    if(israndomizeBoxVelocity) randomizeBoxVelocity(world);

//    opponent_->setState(opponent_gc_init_, opponent_gv_);
    anymal_->setState(gc_init_, gv_init_);

    return true;
  }

  inline void updateObservation(raisim::World *world) {
    anymal_->getState(gc_, gv_);
    raisim::Vec<4> quat;
    raisim::Mat<3, 3> rot;
    quat[0] = gc_[3];
    quat[1] = gc_[4];
    quat[2] = gc_[5];
    quat[3] = gc_[6];
    raisim::quatToRotMat(quat, rot);
    bodyLinearVel_ = rot.e().transpose() * gv_.segment(0, 3);
    bodyAngularVel_ = rot.e().transpose() * gv_.segment(3, 3);

    //update cage's data
    Eigen::Vector3d cage2base_pos_;
    cage2base_pos_ = gc_.head(3);
    cage2base_pos_xy_ = cage2base_pos_.head(2);

    //update opponent robot's data
    raisim::Mat<3, 3> opponent_rot;
    raisim::Vec<3> opponent_pos, opponent_linearVel, opponent_angularVel;
    Eigen::Vector3d opponentLinearVel2Body_, opponentAngularVel2Body_;

    opponent_->getState(opponent_gc_, opponent_gv_);
    raisim::Vec<4> opponent_quat;
    opponent_quat[0] = opponent_gc_[3];
    opponent_quat[1] = opponent_gc_[4];
    opponent_quat[2] = opponent_gc_[5];
    opponent_quat[3] = opponent_gc_[6];
    raisim::quatToRotMat(opponent_quat, opponent_rot);
    opponentLinearVel2Body_ = opponent_rot.e().transpose() * opponent_gv_.segment(0, 3);
    opponentAngularVel2Body_ = opponent_rot.e().transpose() * opponent_gv_.segment(3, 3);

//    opponent_->getPosition(opponent_pos);
//    opponent_->getLinearVelocity(opponent_linearVel);
//    opponent_->getAngularVelocity(opponent_angularVel);
//    opponent_rot = opponent_->getOrientation();
//
//    opponent_gc_.head(3) = opponent_pos.e();
//    opponentLinearVel2Body_ =  rot.e().transpose() * opponent_linearVel.e();
//    opponentAngularVel2Body_ = rot.e().transpose() * opponent_angularVel.e();

    //update opponent cage's data
    opponent_cage2base_pos_xy_ = opponent_gc_.head(2);

    obDouble_ << bodyLinearVel_, bodyAngularVel_, /// body linear&angular velocity 6.
                 gc_[2], /// body pose 1
                 rot.e().row(2).transpose(), /// body orientation 3
                 gc_.tail(12), /// joint angles 12
                 gv_.tail(12), /// joint velocity 12
                 previousAction_, prepreviousAction_, /// previous action 24
                 cage2base_pos_xy_.norm(), /// cage2base xy position 1

                //opponent related data
                rot.e().transpose() * (opponent_gc_.head(3) - gc_.head(3)), /// Relative opponent player xyz position 3
                opponentLinearVel2Body_, /// opponent player linear velocity 3
                opponentAngularVel2Body_, /// opponent player angular velocity 3
                opponent_rot.e().row(2).transpose(), /// opponent player orientation 3
                opponent_cage2base_pos_xy_.norm(),
                40.0, /// opponent cage2base xy position 1

                cage_radius_; /// cage radius 1



  }

  inline void recordReward(Reward *rewards) {
    double poseError = (gc_.head(2) - opponent_gc_.head(2)).squaredNorm() / (gc_init_.head(2) - opponent_gc_init_.head(2)).squaredNorm();

    raisim::Vec<4> quat;
    raisim::Mat<3, 3> rot;
    quat[0] = gc_[3];
    quat[1] = gc_[4];
    quat[2] = gc_[5];
    quat[3] = gc_[6];
    raisim::quatToRotMat(quat, rot);

    Eigen::Vector3d targetVector = rot.e().transpose() * (opponent_gc_.head(3) - gc_.head(3)) / (opponent_gc_.head(3) - gc_.head(3)).norm();

    rewMove2Opponent_ = exp(poseError / 3) - 1;
    rewForwardVel_ = exp(-(bodyLinearVel_.head(2) / (bodyLinearVel_.head(2).norm() + 1e-5)  - targetVector.head(2)).squaredNorm() / 0.25); // std::min(0.5, (gv_.head(2) - targetVector.head(2)*0.5).norm());
    rewTorque_ = anymal_->getGeneralizedForce().squaredNorm();
    rewTakeGoodPose = std::max((cage2base_pos_xy_.norm() - opponent_cage2base_pos_xy_.norm()), 0.0);
    rewOpponent2CageDist_ = std::max(0.0, (opponent_cage2base_pos_xy_).norm()-opponent_gc_init_.head(2).norm()) / (cage_radius_-opponent_gc_init_.head(2).norm());
    rewPushOpponentOff_ = (opponent_gc_.head(2).norm() > cage_radius_) ? 1.0 : 0.0;
    rewBaseMotion_ = (0.8 * bodyLinearVel_[2] * bodyLinearVel_[2] + 0.4* fabs(bodyAngularVel_[0]) + 0.4 * fabs(bodyAngularVel_[1]));
    rewJointPosition = (gc_.tail(nJoints_) - gc_init_.tail(nJoints_)).norm();
    rewBaseHeight = pow((gc_[2] - gc_init_[2]),2);

    //opponent_gc_init_.head(2) << opponent_cage2base_pos_xy_;

    rewards->record("forwardVel", rewForwardVel_);
    rewards->record("move2Opponent",rewMove2Opponent_);
    rewards->record("torque", rewTorque_);
    rewards->record("takeGoodPose", rewTakeGoodPose);
    rewards->record("opponent2CageDist", rewOpponent2CageDist_);
    rewards->record("pushOpponentOff", rewPushOpponentOff_);
    rewards->record("baseMotion", rewBaseMotion_);
    rewards->record("jointPosition", rewJointPosition);
    rewards->record("baseHeight", rewBaseHeight);
    rewards->record("curriculumLevel", curriculumLevel);

  }

  inline const Eigen::VectorXd &getObservation() {
    return obDouble_;
  }

//  void setCfg(const Yaml::Node &cfg)
//  {
//    cfg_ = cfg;
//  }

  void setName(const std::string &name) {
    name_ = name;
  }

  void setOpponentName(const std::string &name) {
    opponentName_ = name;
  }

//  void setBox(raisim::Box *box) {
//    opponent_ = box;
//  }

  void setPlayerNum(const int &playerNum) {
    playerNum_ = playerNum;
  }

  void isOpponentAnymal(const bool isOpponentAnymal){
    isOpponentAnymal_ = isOpponentAnymal;
  }

  void setCageRadius(const double &cage_radius) {
    cage_radius_ = cage_radius;
  }

  void setEpisodeTime(const double &episodeTime){
    episodeTime_ = episodeTime;
  }

  void updateCurrentTime(const double &controlTime){
    currentTime_ += controlTime;
  }

  inline int isTerminalState(raisim::World *world) {
    int terminalState = 0;
    for (auto &contact: anymal_->getContacts()) {
      if (footIndices_.find(contact.getlocalBodyIndex()) == footIndices_.end() && contact.getPairObjectIndex() == world->getObject("ground")->getIndexInWorld()) {
        terminalState = 1;
        break;
      }
      if(contact.getPairObjectIndex() == world->getObject("ground")->getIndexInWorld() &&
          contact.getlocalBodyIndex() == anymal_->getBodyIdx("base")) {
        terminalState = 1;
        break;
      }
    }
    if (gc_.head(2).norm() > cage_radius_) {
      terminalState = 2;
    }
    else if(checkTerminateEpisode()) {
      terminalState = 3;
    }
    else if(opponent_gc_.head(2).norm() > cage_radius_) {
      terminalState = 4;
    }



    return terminalState;
  }

  inline int getObDim() {
    return obDim_;
  }

  inline int getActionDim() {
    return actionDim_;
  }

  bool checkTerminateEpisode()
  {
    if(currentTime_ > episodeTime_) return true;
    else return false;
  }

  void randomizeBoxPosition(raisim::World *world,  double theta){
    auto oppositeAngle = uniDistCage_(gen_) * M_PI;
    double radius = 1.5;
//    opponent_->setPosition(0,0,0.5);
    if(isBoxPosCurriculum){
      double prob = uniDist_(gen_);

      if(prob > 0.5) radius = 1.0 + std::min(0.5, (cage_radius_ / 2) * std::min(1.0, ((double)iter_ / cageRadiusCurriculumIter)));
      else radius = 1.0 + uniDist_(gen_) * std::min(0.5, (cage_radius_ / 2) * std::min(1.0, ((double)iter_ / cageRadiusCurriculumIter)));
//      opponent_->setMass(10.0 + 30.0 * std::min(1.0 , ((double)curriculumLevel / 100.)));
    }
    else radius = 1.5;
    opponent_gc_init_.head(3) << radius * std::cos(theta + oppositeAngle), radius * std::sin(theta + oppositeAngle), 0.5;//1.5 * std::cos(theta), 1.5 * std::sin(theta), 0.5;
    opponent_gc_init_.segment(3, 4) << cos(theta / 2), 0, 0, sin(theta / 2);
//    opponent_->setPosition(radius * std::cos(theta + oppositeAngle), radius * std::sin(theta + oppositeAngle), 0.38);
//    opponent_->setOrientation(cos(theta / 2), 0, 0, sin(theta / 2));
//    opponent_gc_init_.head(3) << radius * std::cos(theta + oppositeAngle), radius * std::sin(theta + oppositeAngle), 0.38;//1.5 * std::cos(theta), 1.5 * std::sin(theta), 0.5;
  }

  void curriculumUpdate(int iter){
    iter_ = iter;
  }

  void curriculumLevelUpdate(){

    if(opponent_gc_.head(2).norm() > cage_radius_){
      curriculumLevel += 1;
    }
    else if((opponent_cage2base_pos_xy_.norm() - opponent_gc_init_.head(2).norm()) /(cage_radius_ - opponent_gc_init_.head(2).norm()) < 0.5) {
      curriculumLevel -= 1;
    }

    if(curriculumLevel < 0) {
      curriculumLevel = 0;
    }
    else if(curriculumLevel > 300){
      curriculumLevel = uniDistInt_(gen_);
    }

//    opponent_->setName("opponent_" + std::to_string(curriculumLevel));
//    setOpponentName(opponent_->getName());

  }

  void randomizeBoxVelocity(raisim::World *world){
//      raisim::Vec<3> opponent_pos;
//      opponent_->getPosition(0, opponent_pos);
//
//      Eigen::Vector3d poseError = (-opponent_pos.e()) / ((opponent_pos.e()).norm() + 1e-5);
//      raisim::Vec<3> boxForce;
//      boxForce[0] = poseError(0);
//      boxForce[1] = poseError(1);
//      boxForce[2] = 0.0;
//
//      raisim::Vec<3> auxForce;
//      auxForce[0] = uniDist_(gen_);
//      auxForce[1] = uniDist_(gen_);
//      auxForce[2] = 0.0;
//
//      auxForce = auxForce / (auxForce.norm() + 1e-5);
//
//      raisim::Vec<3> shearForce;
//      shearForce[0] = -poseError(1);
//      shearForce[1] = poseError(0);
//      shearForce[2] = 0.0;
//
//      shearForce = shearForce / (shearForce.norm() + 1e-5);
//
//      if(currentTime_ < 1e-5){
//        externalForceCount=0;
//        shearForce *= uniDist_(gen_) > 0.5 ? 1.0 : -1.0;
//        externalForceCount++;
//      }
//      else if(currentTime_ > 3.0 && externalForceCount == 1){
//        shearForce *= uniDist_(gen_) > 0.5 ? 1.0 : -1.0;
//        externalForceCount++;
//      }
//      else if(currentTime_ > 8.0 && externalForceCount == 2){
//        shearForce *= uniDist_(gen_) > 0.5 ? 1.0 : -1.0;
//        externalForceCount++;}
//
//
//      if(opponent_cage2base_pos_xy_.norm() > cage_radius_ * 0.75)
//      {
//        boxForce = (boxForce * 4.0 + auxForce * 0.2 + shearForce) / (boxForce * 2.0 + auxForce * 0.2 + shearForce).norm();;
//      }
//      else {
//        double randRadialForce = uniDist_(gen_);
//        boxForce = (boxForce * (randRadialForce * 0.4 + 1.5) + auxForce * 0.2 + shearForce);
//        boxForce = boxForce / (boxForce.norm() + 1e-5);
//      }
//
//      if(curriculumLevel > 100){
//        boxExternalForce_ = boxForce.e() * 10.0 * ((double)(curriculumLevel) -100.) / 200. * opponent_->getMass();
//      }raisim::Vec<3> opponent_pos;
//      opponent_->getPosition(0, opponent_pos);
//
//      Eigen::Vector3d poseError = (-opponent_pos.e()) / ((opponent_pos.e()).norm() + 1e-5);
//      raisim::Vec<3> boxForce;
//      boxForce[0] = poseError(0);
//      boxForce[1] = poseError(1);
//      boxForce[2] = 0.0;
//
//      raisim::Vec<3> auxForce;
//      auxForce[0] = uniDist_(gen_);
//      auxForce[1] = uniDist_(gen_);
//      auxForce[2] = 0.0;
//
//      auxForce = auxForce / (auxForce.norm() + 1e-5);
//
//      raisim::Vec<3> shearForce;
//      shearForce[0] = -poseError(1);
//      shearForce[1] = poseError(0);
//      shearForce[2] = 0.0;
//
//      shearForce = shearForce / (shearForce.norm() + 1e-5);
//
//      if(currentTime_ < 1e-5){
//        externalForceCount=0;
//        shearForce *= uniDist_(gen_) > 0.5 ? 1.0 : -1.0;
//        externalForceCount++;
//      }
//      else if(currentTime_ > 3.0 && externalForceCount == 1){
//        shearForce *= uniDist_(gen_) > 0.5 ? 1.0 : -1.0;
//        externalForceCount++;
//      }
//      else if(currentTime_ > 8.0 && externalForceCount == 2){
//        shearForce *= uniDist_(gen_) > 0.5 ? 1.0 : -1.0;
//        externalForceCount++;}
//
//
//      if(opponent_cage2base_pos_xy_.norm() > cage_radius_ * 0.75)
//      {
//        boxForce = (boxForce * 4.0 + auxForce * 0.2 + shearForce) / (boxForce * 2.0 + auxForce * 0.2 + shearForce).norm();;
//      }
//      else {
//        double randRadialForce = uniDist_(gen_);
//        boxForce = (boxForce * (randRadialForce * 0.4 + 1.5) + auxForce * 0.2 + shearForce);
//        boxForce = boxForce / (boxForce.norm() + 1e-5);
//      }
//
//      if(curriculumLevel > 100){
//        boxExternalForce_ = boxForce.e() * 10.0 * ((double)(curriculumLevel) -100.) / 200. * opponent_->getMass();
//      }
  }

    //RandomizeRelated
  bool israndomizeBoxPosition = true;
  bool israndomizeBoxVelocity = false;
  bool isBoxPosCurriculum = true;
  bool isCageRadiusCurriculum = false;

  double cageRadiusCurriculumIter = 1000.;
  double boxExternelForceCurriculumIter = 2000.;
  double boxMassCurriculumIter = 3000.;
  int curriculumLevel = 0.;

//  raisim::Box *opponent_;
  raisim::ArticulatedSystem *opponent_;
  Eigen::Vector3d boxExternalForce_;
  int externalForceCount = 0;

 private:
  std::string name_, opponentName_;
  int gcDim_, gvDim_, nJoints_, playerNum_ = 0;
  raisim::ArticulatedSystem *anymal_;

  raisim::Visuals *cage_;
  double cage_radius_ = 3.0;
  Eigen::VectorXd gc_init_, gv_init_, gc_, gv_, pTarget_, pTarget12_, vTarget_;
  Eigen::VectorXd cage2base_pos_xy_;
  Eigen::VectorXd previousAction_, prepreviousAction_;
  Eigen::VectorXd actionMean_, actionStd_, obDouble_;
  Eigen::Vector3d bodyLinearVel_, bodyAngularVel_;
  std::set<size_t> footIndices_;
  int obDim_ = 0, actionDim_ = 0;
  double forwardVelRewardCoeff_ = 0.;
  double torqueRewardCoeff_ = 0.;

  //opponent robot's data
  Eigen::VectorXd opponent_gc_, opponent_gv_, opponent_gc_init_;
  Eigen::Vector3d opponent_bodyLinearVel_, opponent_bodyAngularVel_;
  Eigen::VectorXd opponent_cage2base_pos_xy_;

  //Train Related
  int iter_ = 0;
  double episodeTime_ = 10.0;
  double controlTime_ = 0.01;
  double currentTime_ = 0.0;

  //RewardRelated
  double rewForwardVel_ = 0., rewMove2Opponent_ = 0., rewTorque_ = 0., rewTakeGoodPose = 0., rewOpponent2CageDist_ = 0., rewPushOpponentOff_ = 0., rewBaseMotion_=0.,
          rewJointPosition = 0., rewBaseHeight=0.;

//  Yaml::Node &cfg_;

  bool isOpponentAnymal_=true;

  thread_local static std::uniform_real_distribution<double> uniDist_;
  thread_local static std::uniform_real_distribution<double> uniDistCage_;
  thread_local static std::uniform_int_distribution<int> uniDistInt_;
  thread_local static std::mt19937 gen_;
};
thread_local std::mt19937 raisim::PretrainingAnymalControllerTest_20233319::gen_;
thread_local std::uniform_real_distribution<double> raisim::PretrainingAnymalControllerTest_20233319::uniDist_(0., 1.);
thread_local std::uniform_real_distribution<double> raisim::PretrainingAnymalControllerTest_20233319::uniDistCage_(0.8, 1.2);
thread_local std::uniform_int_distribution<int> raisim::PretrainingAnymalControllerTest_20233319::uniDistInt_(80, 300);
}