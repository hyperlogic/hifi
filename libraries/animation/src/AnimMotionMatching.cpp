//
//  AnimMotionMatching.cpp
//
//  Created by Anthony J. Thibault on 12/18/18.
//  Copyright (c) 2018 High Fidelity, Inc. All rights reserved.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "AnimMotionMatching.h"
#include <QFile>
#include <DebugDraw.h>
#include "GLMHelpers.h"

// motion matching files are too large for Qt, https://bugreports.qt.io/browse/QTBUG-60728
// use this library instead.
#include <nlohmann/json.hpp>

const QString DATA_FILENAME = "c:/msys64/home/tony/code/hifi-input-recorder-viewer/data.json";

AnimMotionMatching::AnimMotionMatching(const QString& id) :
    AnimNode(AnimNode::Type::MotionMatching, id)
{
    qDebug() << "AJT: loading motion matching node!";

    QFile openFile(DATA_FILENAME);
    if (!openFile.open(QIODevice::ReadOnly)) {
        qWarning() << "could not open file: " << DATA_FILENAME;
        return;
    }

    QByteArray jsonData = openFile.readAll();
    openFile.close();
    nlohmann::json object = nlohmann::json::parse(jsonData.data());

    // parse the json
    const size_t numRows = object.size();
    _rows.reserve(numRows);
    DataRow row;
    for (size_t i = 0; i < numRows; i++) {
        for (size_t j = 0; j < DATA_ROW_SIZE; j++) {
            row.data[j] = object[i][j];
        }
        _rows.push_back(row);
    }

    // start off at the start of the mo-cap
    _frame = 0.0f;
}

AnimMotionMatching::~AnimMotionMatching() {

}

static bool debug = false;

float AnimMotionMatching::computeCurrentCost(const DataRow& currentRow, const DataRow& candidateRow) const {
    const float K_P = _currentFootPositionCostFactor;
    const float K_R = _currentFootRotationCostFactor;
    const float K_V = _currentFootVelocityCostFactor;
    const float K_AV = _currentFootAngularVelocityCostFactor;   // AJT: HACK re using this for hips pos

    float cost = 0.0f;

    cost += K_P * glm::length2(glm::vec3(currentRow.data[LEFTFOOT_PX_INDEX], currentRow.data[LEFTFOOT_PY_INDEX], currentRow.data[LEFTFOOT_PZ_INDEX]) -
                               glm::vec3(candidateRow.data[LEFTFOOT_PX_INDEX], candidateRow.data[LEFTFOOT_PY_INDEX], candidateRow.data[LEFTFOOT_PZ_INDEX]));
    cost += K_R * glm::length2(glm::vec3(currentRow.data[LEFTFOOT_RX_INDEX], currentRow.data[LEFTFOOT_RY_INDEX], currentRow.data[LEFTFOOT_RZ_INDEX]) -
                               glm::vec3(candidateRow.data[LEFTFOOT_RX_INDEX], candidateRow.data[LEFTFOOT_RY_INDEX], candidateRow.data[LEFTFOOT_RZ_INDEX]));
    // AJTMM TODO
    /*
    cost += K_V * glm::length2(glm::vec3(currentRow.data[LEFTFOOT_DX_INDEX], currentRow.data[LEFTFOOT_DY_INDEX], currentRow.data[LEFTFOOT_DZ_INDEX]) -
                               glm::vec3(candidateRow.data[LEFTFOOT_DX_INDEX], candidateRow.data[LEFTFOOT_DY_INDEX], candidateRow.data[LEFTFOOT_DZ_INDEX]));
    cost += K_AV * glm::length2(glm::vec3(currentRow.data[LEFTFOOT_WX_INDEX], currentRow.data[LEFTFOOT_WY_INDEX], currentRow.data[LEFTFOOT_WZ_INDEX]) -
                                glm::vec3(candidateRow.data[LEFTFOOT_WX_INDEX], candidateRow.data[LEFTFOOT_WY_INDEX], candidateRow.data[LEFTFOOT_WZ_INDEX]));
    */

    cost += K_P * glm::length2(glm::vec3(currentRow.data[RIGHTFOOT_PX_INDEX], currentRow.data[RIGHTFOOT_PY_INDEX], currentRow.data[RIGHTFOOT_PZ_INDEX]) -
                               glm::vec3(candidateRow.data[RIGHTFOOT_PX_INDEX], candidateRow.data[RIGHTFOOT_PY_INDEX], candidateRow.data[RIGHTFOOT_PZ_INDEX]));
    cost += K_R * glm::length2(glm::vec3(currentRow.data[RIGHTFOOT_RX_INDEX], currentRow.data[RIGHTFOOT_RY_INDEX], currentRow.data[RIGHTFOOT_RZ_INDEX]) -
                               glm::vec3(candidateRow.data[RIGHTFOOT_RX_INDEX], candidateRow.data[RIGHTFOOT_RY_INDEX], candidateRow.data[RIGHTFOOT_RZ_INDEX]));

    // AJTMM TODO
    /*
    cost += K_V * glm::length2(glm::vec3(currentRow.data[RIGHTFOOT_DX_INDEX], currentRow.data[RIGHTFOOT_DY_INDEX], currentRow.data[RIGHTFOOT_DZ_INDEX]) -
                               glm::vec3(candidateRow.data[RIGHTFOOT_DX_INDEX], candidateRow.data[RIGHTFOOT_DY_INDEX], candidateRow.data[RIGHTFOOT_DZ_INDEX]));
    cost += K_AV * glm::length2(glm::vec3(currentRow.data[RIGHTFOOT_WX_INDEX], currentRow.data[RIGHTFOOT_WY_INDEX], currentRow.data[RIGHTFOOT_WZ_INDEX]) -
                                glm::vec3(candidateRow.data[RIGHTFOOT_WX_INDEX], candidateRow.data[RIGHTFOOT_WY_INDEX], candidateRow.data[RIGHTFOOT_WZ_INDEX]));
    */

    cost += K_AV * glm::length2(glm::vec3(currentRow.data[HIPS_PX_INDEX], currentRow.data[HIPS_PY_INDEX], currentRow.data[HIPS_PZ_INDEX]) -
                                glm::vec3(candidateRow.data[HIPS_PX_INDEX], candidateRow.data[HIPS_PY_INDEX], candidateRow.data[HIPS_PZ_INDEX]));

    if (debug) {
        qDebug() << "AJT:        currentCost =" << cost;
    }

    return cost;
}

float AnimMotionMatching::computeFutureCost(const DataRow& currentRow, const Goal& goal) const {

    float cost = 0.0f;

    const float K_V = _futureHipsVelocityCostFactor;

    // match future root trajectory with desired goal trajectory
    cost += K_V * glm::length2(glm::vec3(currentRow.data[ROOTTRAJ1_PX_INDEX], currentRow.data[ROOTTRAJ1_PY_INDEX], currentRow.data[ROOTTRAJ1_PZ_INDEX]) - goal.desiredRootTrajectory[0].position);
    cost += K_V * glm::length2(glm::vec3(currentRow.data[ROOTTRAJ2_PX_INDEX], currentRow.data[ROOTTRAJ2_PY_INDEX], currentRow.data[ROOTTRAJ2_PZ_INDEX]) - goal.desiredRootTrajectory[1].position);
    cost += K_V * glm::length2(glm::vec3(currentRow.data[ROOTTRAJ3_PX_INDEX], currentRow.data[ROOTTRAJ3_PY_INDEX], currentRow.data[ROOTTRAJ3_PZ_INDEX]) - goal.desiredRootTrajectory[2].position);
    cost += K_V * glm::length2(glm::vec3(currentRow.data[ROOTTRAJ4_PX_INDEX], currentRow.data[ROOTTRAJ4_PY_INDEX], currentRow.data[ROOTTRAJ4_PZ_INDEX]) - goal.desiredRootTrajectory[3].position);

    // AJT: HACK re-using this coeff
    const float K_AV = _futureHipsAngularVelocityCostFactor;

    if (goal.hmdMode) {
        // match past head trajectory with previous head trajectory
        cost += K_V * glm::length2(glm::vec3(currentRow.data[HEADTRAJ5_PX_INDEX], currentRow.data[ROOTTRAJ5_PY_INDEX], currentRow.data[ROOTTRAJ5_PZ_INDEX]) - goal.previousHeadTrajectory[0].position);
        cost += K_V * glm::length2(glm::vec3(currentRow.data[HEADTRAJ6_PX_INDEX], currentRow.data[ROOTTRAJ6_PY_INDEX], currentRow.data[ROOTTRAJ6_PZ_INDEX]) - goal.previousHeadTrajectory[1].position);
        cost += K_V * glm::length2(glm::vec3(currentRow.data[HEADTRAJ7_PX_INDEX], currentRow.data[ROOTTRAJ7_PY_INDEX], currentRow.data[ROOTTRAJ7_PZ_INDEX]) - goal.previousHeadTrajectory[2].position);
        cost += K_V * glm::length2(glm::vec3(currentRow.data[HEADTRAJ8_PX_INDEX], currentRow.data[ROOTTRAJ8_PY_INDEX], currentRow.data[ROOTTRAJ8_PZ_INDEX]) - goal.previousHeadTrajectory[3].position);
    }

    return cost;
}


float AnimMotionMatching::computeCost(const DataRow& currentRow, const DataRow& candidateRow, const Goal& goal) const {

    float cost = 0.0f;

    cost += computeCurrentCost(currentRow, candidateRow);

    const float K_FUTURE = _futureCostFactor;
    cost += K_FUTURE * computeFutureCost(candidateRow, goal);

    if (debug) {
        qDebug() << "AJT:        cost =" << cost;
    }

    return cost;
}

// AJT: TODO discover why my exp/log differ from glm?
glm::quat exp(const glm::quat& q) {
    float angle = sqrtf(glm::dot(q, q));
    if (angle > 0.0001f) {
        float sinHalfAngle = sinf(0.5f * angle);
        float cosHalfAngle = cosf(0.5f * angle);
        return glm::quat(cosHalfAngle,
                         (q.x / angle) * sinHalfAngle,
                         (q.y / angle) * sinHalfAngle,
                         (q.z / angle) * sinHalfAngle);
    } else {
        return glm::quat();
    }
}

const AnimPoseVec& AnimMotionMatching::evaluate(const AnimVariantMap& animVars, const AnimContext& context, float dt, AnimVariantMap& triggersOut) {
    if (_skeleton) {
        _poses = _skeleton->getRelativeDefaultPoses();
    } else {
        _poses.clear();
        return _poses;
    }

    _currentFootPositionCostFactor = animVars.lookup("K_FP", _currentFootPositionCostFactor);
    _currentFootRotationCostFactor = animVars.lookup("K_FR", _currentFootRotationCostFactor);
    _currentFootVelocityCostFactor = animVars.lookup("K_FV", _currentFootVelocityCostFactor);
    _currentFootAngularVelocityCostFactor = animVars.lookup("K_FAV", _currentFootAngularVelocityCostFactor);
    _futureHipsVelocityCostFactor = animVars.lookup("K_HV", _futureHipsVelocityCostFactor);
    _futureHipsAngularVelocityCostFactor = animVars.lookup("K_HAV", _futureHipsAngularVelocityCostFactor);
    _futureCostFactor = animVars.lookup("K_F", _futureCostFactor);
    _sameLocationWindow = animVars.lookup("K_LW", _sameLocationWindow);

    const float FPS = 90.0f;
    _frame += dt * FPS;
    _frame = glm::clamp(_frame, 0.0f, (float)_rows.size() - 1.0f);

    // initialize desired goal
    Goal goal;
    glm::vec3 rigDesiredVel = animVars.lookupRaw(QString("hipsVelocity"), glm::vec3());
    glm::vec3 rigDesiredAngularVel = animVars.lookupRaw(QString("hipsAngularVelocity"), glm::vec3());
    float rigDesiredAngularVelLength = glm::length(rigDesiredAngularVel);

    static const float TRAJECTORY_FRAMES[NUM_TRAJECTORY_POINTS] = {18.0f, 36.0f, 62.0f, 90.0f};
    for (size_t i = 0; i < NUM_TRAJECTORY_POINTS; i++) {
        float trajectoryTime = (TRAJECTORY_FRAMES[i] / FPS);
        // glm::angleAxis(rigDesiredAngularVelLength * trajectoryTime, glm::vec3(0.0f, 1.0f, 0.0f))
        goal.desiredRootTrajectory[i] = {rigDesiredVel * trajectoryTime, glm::quat()};
    }

    // debug desired goal
    DebugDraw::getInstance().addMyAvatarMarker("DesiredRootTraj1", goal.desiredRootTrajectory[0].rotation * Quaternions::Y_180, Quaternions::Y_180 * goal.desiredRootTrajectory[0].position, glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));
    DebugDraw::getInstance().addMyAvatarMarker("DesiredRootTraj2", goal.desiredRootTrajectory[1].rotation * Quaternions::Y_180, Quaternions::Y_180 * goal.desiredRootTrajectory[1].position, glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));
    DebugDraw::getInstance().addMyAvatarMarker("DesiredRootTraj3", goal.desiredRootTrajectory[2].rotation * Quaternions::Y_180, Quaternions::Y_180 * goal.desiredRootTrajectory[2].position, glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));
    DebugDraw::getInstance().addMyAvatarMarker("DesiredRootTraj4", goal.desiredRootTrajectory[3].rotation * Quaternions::Y_180, Quaternions::Y_180 * goal.desiredRootTrajectory[3].position, glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));

    glm::vec3 headTraj1 = animVars.lookupRaw("mmHeadTraj1", glm::vec3());
    glm::vec3 headTraj2 = animVars.lookupRaw("mmHeadTraj2", glm::vec3());
    glm::vec3 headTraj3 = animVars.lookupRaw("mmHeadTraj3", glm::vec3());
    glm::vec3 headTraj4 = animVars.lookupRaw("mmHeadTraj4", glm::vec3());

    AnimPose horizontalHeadPose(cancelOutRollAndPitch(animVars.lookupRaw("headRotation", glm::quat())), animVars.lookupRaw("headPosition", glm::vec3()));
    AnimPose invHorizontalHeadPose = horizontalHeadPose.inverse();

    // transform head trajectory into horizontal hmd
    goal.previousHeadTrajectory[0].rotation = glm::quat();
    goal.previousHeadTrajectory[0].position = invHorizontalHeadPose.xformPoint(headTraj1);
    goal.previousHeadTrajectory[1].rotation = glm::quat();
    goal.previousHeadTrajectory[1].position = invHorizontalHeadPose.xformPoint(headTraj2);
    goal.previousHeadTrajectory[2].rotation = glm::quat();
    goal.previousHeadTrajectory[2].position = invHorizontalHeadPose.xformPoint(headTraj3);
    goal.previousHeadTrajectory[3].rotation = glm::quat();
    goal.previousHeadTrajectory[3].position = invHorizontalHeadPose.xformPoint(headTraj4);

    goal.hmdMode = animVars.lookup("mmHmdMode", false);

    // debug draw head trajectory
    DebugDraw::getInstance().addMyAvatarMarker("HeadHistory1", Quaternions::Y_180 * glm::quat(), Quaternions::Y_180 * headTraj1, glm::vec4(1.0f));
    DebugDraw::getInstance().addMyAvatarMarker("HeadHistory2", Quaternions::Y_180 * glm::quat(), Quaternions::Y_180 * headTraj2, glm::vec4(1.0f));
    DebugDraw::getInstance().addMyAvatarMarker("HeadHistory3", Quaternions::Y_180 * glm::quat(), Quaternions::Y_180 * headTraj3, glm::vec4(1.0f));
    DebugDraw::getInstance().addMyAvatarMarker("HeadHistory4", Quaternions::Y_180 * glm::quat(), Quaternions::Y_180 * headTraj4, glm::vec4(1.0f));

    DataRow currentRow = _rows[floorf(_frame)];
    float bestCost = FLT_MAX;
    DataRow bestRow;

    for (size_t i = 0; i < _rows.size(); i++) {
        float thisCost = computeCost(currentRow, _rows[i], goal);
        if (thisCost < bestCost) {
            bestCost = thisCost;
            bestRow = _rows[i];
        }
    }

    qDebug() << "AJT:     bestRow.frame =" << bestRow.data[ID_INDEX] << ", bestCost =" << bestCost;

    const float JUMP_INTERP_TIME = 0.25;
    const float SAME_LOCATION_THRESHOLD = _sameLocationWindow * FPS;
    bool theWinnerIsAtTheSameLocation = fabs(_frame - bestRow.data[ID_INDEX]) < SAME_LOCATION_THRESHOLD;
    if (!theWinnerIsAtTheSameLocation)
    {
        _interpTimer = JUMP_INTERP_TIME;
        // jump to the winning location
        _frame = bestRow.data[ID_INDEX];
        currentRow = bestRow;
        qDebug() << "AJT:     JUMP TO FRAME " << _frame;
    } else {
        qDebug() << "AJT:     same loc, delta = " << fabs(_frame - bestRow.data[ID_INDEX]);
    }

    _interpTimer -= dt;

    // debug draw current trajectory
    DebugDraw::getInstance().addMyAvatarMarker("RootTraj1", Quaternions::Y_180, Quaternions::Y_180 * glm::vec3(currentRow.data[ROOTTRAJ1_PX_INDEX], currentRow.data[ROOTTRAJ1_PY_INDEX], currentRow.data[ROOTTRAJ1_PZ_INDEX]), glm::vec4(0.0f, 0.0f, 1.0f, 1.0f));
    DebugDraw::getInstance().addMyAvatarMarker("RootTraj2", Quaternions::Y_180, Quaternions::Y_180 * glm::vec3(currentRow.data[ROOTTRAJ2_PX_INDEX], currentRow.data[ROOTTRAJ2_PY_INDEX], currentRow.data[ROOTTRAJ2_PZ_INDEX]), glm::vec4(0.0f, 0.0f, 1.0f, 1.0f));
    DebugDraw::getInstance().addMyAvatarMarker("RootTraj3", Quaternions::Y_180, Quaternions::Y_180 * glm::vec3(currentRow.data[ROOTTRAJ3_PX_INDEX], currentRow.data[ROOTTRAJ3_PY_INDEX], currentRow.data[ROOTTRAJ3_PZ_INDEX]), glm::vec4(0.0f, 0.0f, 1.0f, 1.0f));
    DebugDraw::getInstance().addMyAvatarMarker("RootTraj4", Quaternions::Y_180, Quaternions::Y_180 * glm::vec3(currentRow.data[ROOTTRAJ4_PX_INDEX], currentRow.data[ROOTTRAJ4_PY_INDEX], currentRow.data[ROOTTRAJ4_PZ_INDEX]), glm::vec4(0.0f, 0.0f, 1.0f, 1.0f));

    // debug draw current previous trajectory
    AnimPose Y_180_POSE(Quaternions::Y_180, glm::vec3());
    AnimPose headTraj5Pose = Y_180_POSE * horizontalHeadPose * AnimPose(glm::quat(), glm::vec3(currentRow.data[ROOTTRAJ5_PX_INDEX], currentRow.data[ROOTTRAJ5_PY_INDEX], currentRow.data[ROOTTRAJ5_PZ_INDEX]));
    AnimPose headTraj6Pose = Y_180_POSE * horizontalHeadPose * AnimPose(glm::quat(), glm::vec3(currentRow.data[ROOTTRAJ6_PX_INDEX], currentRow.data[ROOTTRAJ6_PY_INDEX], currentRow.data[ROOTTRAJ6_PZ_INDEX]));
    AnimPose headTraj7Pose = Y_180_POSE * horizontalHeadPose * AnimPose(glm::quat(), glm::vec3(currentRow.data[ROOTTRAJ7_PX_INDEX], currentRow.data[ROOTTRAJ7_PY_INDEX], currentRow.data[ROOTTRAJ7_PZ_INDEX]));
    AnimPose headTraj8Pose = Y_180_POSE * horizontalHeadPose * AnimPose(glm::quat(), glm::vec3(currentRow.data[ROOTTRAJ8_PX_INDEX], currentRow.data[ROOTTRAJ8_PY_INDEX], currentRow.data[ROOTTRAJ8_PZ_INDEX]));
    DebugDraw::getInstance().addMyAvatarMarker("HeadTraj5", headTraj5Pose.rot(), headTraj5Pose.trans(), glm::vec4(0.0f, 1.0f, 1.0f, 1.0f));
    DebugDraw::getInstance().addMyAvatarMarker("HeadTraj6", headTraj6Pose.rot(), headTraj6Pose.trans(), glm::vec4(0.0f, 1.0f, 1.0f, 1.0f));
    DebugDraw::getInstance().addMyAvatarMarker("HeadTraj7", headTraj7Pose.rot(), headTraj7Pose.trans(), glm::vec4(0.0f, 1.0f, 1.0f, 1.0f));
    DebugDraw::getInstance().addMyAvatarMarker("HeadTraj8", headTraj8Pose.rot(), headTraj8Pose.trans(), glm::vec4(0.0f, 1.0f, 1.0f, 1.0f));

    // AJT: TODO apply offset?!?

    qDebug() << "AJT: p4" << glm::vec3(currentRow.data[ROOTTRAJ4_PX_INDEX], currentRow.data[ROOTTRAJ4_PY_INDEX], currentRow.data[ROOTTRAJ4_PZ_INDEX]);


    AnimPose rigToGeometryPose = AnimPose(glm::inverse(context.getGeometryToRigMatrix()));

    Snapshot targetSnapshot;

    // grab leftfoot from currentRow
    glm::vec3 currentLeftFootPos(currentRow.data[LEFTFOOT_PX_INDEX], currentRow.data[LEFTFOOT_PY_INDEX], currentRow.data[LEFTFOOT_PZ_INDEX]);
    glm::quat currentLeftFootRot = glm::quat(currentRow.data[LEFTFOOT_RW_INDEX], currentRow.data[LEFTFOOT_RX_INDEX], currentRow.data[LEFTFOOT_RY_INDEX], currentRow.data[LEFTFOOT_RZ_INDEX]);
    AnimPose currentLeftFootPose(currentLeftFootRot, currentLeftFootPos);
    currentLeftFootPose = rigToGeometryPose * currentLeftFootPose;
    currentLeftFootPose.scale() = glm::vec3(1.0f);
    targetSnapshot.leftFoot = currentLeftFootPose;

    // grab rightfoot from currentRow
    glm::vec3 currentRightFootPos(currentRow.data[RIGHTFOOT_PX_INDEX], currentRow.data[RIGHTFOOT_PY_INDEX], currentRow.data[RIGHTFOOT_PZ_INDEX]);
    glm::quat currentRightFootRot = glm::quat(currentRow.data[RIGHTFOOT_RW_INDEX], currentRow.data[RIGHTFOOT_RX_INDEX], currentRow.data[RIGHTFOOT_RY_INDEX], currentRow.data[RIGHTFOOT_RZ_INDEX]);
    AnimPose currentRightFootPose(currentRightFootRot, currentRightFootPos);
    currentRightFootPose = rigToGeometryPose * currentRightFootPose;
    currentRightFootPose.scale() = glm::vec3(1.0f);
    targetSnapshot.rightFoot = currentRightFootPose;

    // grab hips from currentRow
    glm::vec3 currentHipsPos(currentRow.data[HIPS_PX_INDEX], currentRow.data[HIPS_PY_INDEX], currentRow.data[HIPS_PZ_INDEX]);
    glm::quat currentHipsRot = glm::quat(currentRow.data[HIPS_RW_INDEX], currentRow.data[HIPS_RX_INDEX], currentRow.data[HIPS_RY_INDEX], currentRow.data[HIPS_RZ_INDEX]);
    AnimPose currentHipsPose(currentHipsRot, currentHipsPos);
    //currentHipsPose = rigToGeometryPose * currentHipsPose;
    currentHipsPose.scale() = glm::vec3(1.0f);
    targetSnapshot.hips = currentHipsPose;

    // grab lefthand from currentRow
    glm::vec3 currentLeftHandPos(currentRow.data[LEFTHAND_PX_INDEX], currentRow.data[LEFTHAND_PY_INDEX], currentRow.data[LEFTHAND_PZ_INDEX]);
    glm::quat currentLeftHandRot = glm::quat(currentRow.data[LEFTHAND_RW_INDEX], currentRow.data[LEFTHAND_RX_INDEX], currentRow.data[LEFTHAND_RY_INDEX], currentRow.data[LEFTHAND_RZ_INDEX]);
    AnimPose currentLeftHandPose(currentLeftHandRot, currentLeftHandPos);
    //currentLeftHandPose = rigToGeometryPose * currentLeftHandPose;
    currentLeftHandPose.scale() = glm::vec3(1.0f);
    targetSnapshot.leftHand = currentLeftHandPose;

    // grab righthand from currentRow
    glm::vec3 currentRightHandPos(currentRow.data[RIGHTHAND_PX_INDEX], currentRow.data[RIGHTHAND_PY_INDEX], currentRow.data[RIGHTHAND_PZ_INDEX]);
    glm::quat currentRightHandRot = glm::quat(currentRow.data[RIGHTHAND_RW_INDEX], currentRow.data[RIGHTHAND_RX_INDEX], currentRow.data[RIGHTHAND_RY_INDEX], currentRow.data[RIGHTHAND_RZ_INDEX]);
    AnimPose currentRightHandPose(currentRightHandRot, currentRightHandPos);
    //currentRightHandPose = rigToGeometryPose * currentRightHandPose;
    currentRightHandPose.scale() = glm::vec3(1.0f);
    targetSnapshot.rightHand = currentRightHandPose;

    // grab head from currentRow
    glm::vec3 currentHeadPos(currentRow.data[HEAD_PX_INDEX], currentRow.data[HEAD_PY_INDEX], currentRow.data[HEAD_PZ_INDEX]);
    glm::quat currentHeadRot = glm::quat(currentRow.data[HEAD_RW_INDEX], currentRow.data[HEAD_RX_INDEX], currentRow.data[HEAD_RY_INDEX], currentRow.data[HEAD_RZ_INDEX]);
    AnimPose currentHeadPose(currentHeadRot, currentHeadPos);
    //currentHeadPose = rigToGeometryPose * currentHeadPose;
    currentHeadPose.scale() = glm::vec3(1.0f);
    if (goal.hmdMode) {
        // drive the head with the actual hmd.
        currentHeadPose.trans() = animVars.lookupRaw("headPosition", glm::vec3());
        currentHeadPose.rot() = animVars.lookupRaw("headRotation", glm::quat());
    }
    targetSnapshot.head = currentHeadPose;


    // AJT: hack for debugging
    triggersOut.set("_frame", _frame);

    processOutputJoints(triggersOut);

    if (_interpTimer > 0.0f) {
        float alpha = (JUMP_INTERP_TIME - _interpTimer) / JUMP_INTERP_TIME;

        targetSnapshot.head.blend(_snapshot.head, alpha);
        targetSnapshot.hips.blend(_snapshot.hips, alpha);
        targetSnapshot.leftFoot.blend(_snapshot.leftFoot, alpha);
        targetSnapshot.rightFoot.blend(_snapshot.rightFoot, alpha);
        targetSnapshot.leftHand.blend(_snapshot.leftHand, alpha);
        targetSnapshot.rightHand.blend(_snapshot.rightHand, alpha);

    } else {
        _snapshot = targetSnapshot;
    }


    // Output feet poses for IK node to pick them up.  in geom space!?!
    triggersOut.set("mainStateMachineLeftFootPosition", targetSnapshot.leftFoot.trans());
    triggersOut.set("mainStateMachineLeftFootRotation", targetSnapshot.leftFoot.rot());
    triggersOut.set("mainStateMachineRightFootPosition", targetSnapshot.rightFoot.trans());
    triggersOut.set("mainStateMachineRightFootRotation", targetSnapshot.rightFoot.rot());

    // rig space
    triggersOut.set("mmHeadPosition", targetSnapshot.head.trans());
    triggersOut.set("mmHeadRotation", targetSnapshot.head.rot());
    triggersOut.set("mmHipsPosition", targetSnapshot.hips.trans());
    triggersOut.set("mmHipsRotation", targetSnapshot.hips.rot());
    triggersOut.set("mmLeftHandPosition", targetSnapshot.leftHand.trans());
    triggersOut.set("mmLeftHandRotation", targetSnapshot.leftHand.rot());
    triggersOut.set("mmRightHandPosition", targetSnapshot.rightHand.trans());
    triggersOut.set("mmRightHandRotation", targetSnapshot.rightHand.rot());

    return _poses;
}

const AnimPoseVec& AnimMotionMatching::getPosesInternal() const {
    return _poses;
}
