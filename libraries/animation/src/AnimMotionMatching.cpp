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
#include "GLMHelpers.h"

// motion matching files are too large for Qt, https://bugreports.qt.io/browse/QTBUG-60728
// use this library instead.
#include <nlohmann/json.hpp>

const QString DATA_FILENAME = "C:/Users/tony/hifi-ml/motion-matching/data.json";

enum RowIndices {
    FRAME_INDEX = 0,
    HIPS_WX_INDEX,
    HIPS_WY_INDEX,
    HIPS_WZ_INDEX,
/*
    HIPS_RX_INDEX,
    HIPS_RY_INDEX,
    HIPS_RZ_INDEX,
    HIPS_PX_INDEX,
    HIPS_PY_INDEX,
    HIPS_PZ_INDEX,
*/
    HIPS_DX_INDEX,
    HIPS_DY_INDEX,
    HIPS_DZ_INDEX,
    LEFTFOOT_WX_INDEX,
    LEFTFOOT_WY_INDEX,
    LEFTFOOT_WZ_INDEX,
    LEFTFOOT_RX_INDEX,
    LEFTFOOT_RY_INDEX,
    LEFTFOOT_RZ_INDEX,
    LEFTFOOT_PX_INDEX,
    LEFTFOOT_PY_INDEX,
    LEFTFOOT_PZ_INDEX,
    LEFTFOOT_DX_INDEX,
    LEFTFOOT_DY_INDEX,
    LEFTFOOT_DZ_INDEX,
    RIGHTFOOT_WX_INDEX,
    RIGHTFOOT_WY_INDEX,
    RIGHTFOOT_WZ_INDEX,
    RIGHTFOOT_RX_INDEX,
    RIGHTFOOT_RY_INDEX,
    RIGHTFOOT_RZ_INDEX,
    RIGHTFOOT_PX_INDEX,
    RIGHTFOOT_PY_INDEX,
    RIGHTFOOT_PZ_INDEX,
    RIGHTFOOT_DX_INDEX,
    RIGHTFOOT_DY_INDEX,
    RIGHTFOOT_DZ_INDEX
};

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
    const float K_AV = _currentFootAngularVelocityCostFactor;

    float cost = 0.0f;

    cost += K_P * glm::length2(glm::vec3(currentRow.data[LEFTFOOT_PX_INDEX], currentRow.data[LEFTFOOT_PY_INDEX], currentRow.data[LEFTFOOT_PZ_INDEX]) -
                               glm::vec3(candidateRow.data[LEFTFOOT_PX_INDEX], candidateRow.data[LEFTFOOT_PY_INDEX], candidateRow.data[LEFTFOOT_PZ_INDEX]));
    cost += K_R * glm::length2(glm::vec3(currentRow.data[LEFTFOOT_RX_INDEX], currentRow.data[LEFTFOOT_RY_INDEX], currentRow.data[LEFTFOOT_RZ_INDEX]) -
                               glm::vec3(candidateRow.data[LEFTFOOT_RX_INDEX], candidateRow.data[LEFTFOOT_RY_INDEX], candidateRow.data[LEFTFOOT_RZ_INDEX]));
    cost += K_V * glm::length2(glm::vec3(currentRow.data[LEFTFOOT_DX_INDEX], currentRow.data[LEFTFOOT_DY_INDEX], currentRow.data[LEFTFOOT_DZ_INDEX]) -
                               glm::vec3(candidateRow.data[LEFTFOOT_DX_INDEX], candidateRow.data[LEFTFOOT_DY_INDEX], candidateRow.data[LEFTFOOT_DZ_INDEX]));
    cost += K_AV * glm::length2(glm::vec3(currentRow.data[LEFTFOOT_WX_INDEX], currentRow.data[LEFTFOOT_WY_INDEX], currentRow.data[LEFTFOOT_WZ_INDEX]) -
                                glm::vec3(candidateRow.data[LEFTFOOT_WX_INDEX], candidateRow.data[LEFTFOOT_WY_INDEX], candidateRow.data[LEFTFOOT_WZ_INDEX]));

    cost += K_P * glm::length2(glm::vec3(currentRow.data[RIGHTFOOT_PX_INDEX], currentRow.data[RIGHTFOOT_PY_INDEX], currentRow.data[RIGHTFOOT_PZ_INDEX]) -
                               glm::vec3(candidateRow.data[RIGHTFOOT_PX_INDEX], candidateRow.data[RIGHTFOOT_PY_INDEX], candidateRow.data[RIGHTFOOT_PZ_INDEX]));
    cost += K_R * glm::length2(glm::vec3(currentRow.data[RIGHTFOOT_RX_INDEX], currentRow.data[RIGHTFOOT_RY_INDEX], currentRow.data[RIGHTFOOT_RZ_INDEX]) -
                               glm::vec3(candidateRow.data[RIGHTFOOT_RX_INDEX], candidateRow.data[RIGHTFOOT_RY_INDEX], candidateRow.data[RIGHTFOOT_RZ_INDEX]));
    cost += K_V * glm::length2(glm::vec3(currentRow.data[RIGHTFOOT_DX_INDEX], currentRow.data[RIGHTFOOT_DY_INDEX], currentRow.data[RIGHTFOOT_DZ_INDEX]) -
                               glm::vec3(candidateRow.data[RIGHTFOOT_DX_INDEX], candidateRow.data[RIGHTFOOT_DY_INDEX], candidateRow.data[RIGHTFOOT_DZ_INDEX]));
    cost += K_AV * glm::length2(glm::vec3(currentRow.data[RIGHTFOOT_WX_INDEX], currentRow.data[RIGHTFOOT_WY_INDEX], currentRow.data[RIGHTFOOT_WZ_INDEX]) -
                                glm::vec3(candidateRow.data[RIGHTFOOT_WX_INDEX], candidateRow.data[RIGHTFOOT_WY_INDEX], candidateRow.data[RIGHTFOOT_WZ_INDEX]));

    if (debug) {
        qDebug() << "AJT:        currentCost =" << cost;
    }

    return cost;
}

float AnimMotionMatching::computeFutureCost(const DataRow& currentRow, const Goal& goal) const {
    const float K_V = _futureHipsVelocityCostFactor;
    const float K_AV = _futureHipsAngularVelocityCostFactor;

    float cost = 0.0f;

    cost += K_V * glm::length2(glm::vec3(currentRow.data[HIPS_DX_INDEX], currentRow.data[HIPS_DY_INDEX], currentRow.data[HIPS_DZ_INDEX]) - goal.hipsVel);
    cost += K_AV * glm::length2(glm::vec3(currentRow.data[HIPS_WX_INDEX], currentRow.data[HIPS_WY_INDEX], currentRow.data[HIPS_WZ_INDEX]) - goal.hipsAngularVel);

    if (debug) {
        qDebug() << "AJT:        currentRow.vel =" << glm::vec3(currentRow.data[HIPS_DX_INDEX], currentRow.data[HIPS_DY_INDEX], currentRow.data[HIPS_DZ_INDEX]);
        qDebug() << "AJT:        goal.vel =" << goal.hipsVel;
        qDebug() << "AJT:        futureCost =" << cost;
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

    // look up desried hips
    int hipsIndex = _skeleton->nameToJointIndex("Hips");
    int hipsParentIndex = _skeleton->getParentIndex(hipsIndex);

    glm::quat rigHipsRot = animVars.lookupRaw(QString("hipsRotation"), glm::quat());
    glm::vec3 rigHipsPos = animVars.lookupRaw(QString("hipsPosition"), glm::vec3());
    AnimPose rigHipsPose(cancelOutRollAndPitch(rigHipsRot), rigHipsPos);
    AnimPose invRigHipsPose = rigHipsPose.inverse();

    glm::quat hipsRot = animVars.lookupRigToGeometry(QString("hipsRotation"), _skeleton->getAbsoluteDefaultPose(hipsIndex).rot());
    glm::vec3 hipsPos = animVars.lookupRigToGeometry(QString("hipsPosition"), _skeleton->getAbsoluteDefaultPose(hipsIndex).trans());
    AnimPose hipsPose(hipsRot, hipsPos);

    // set hips directly
    if (hipsParentIndex != -1) {
        AnimPose hipsParentPose = _skeleton->getAbsolutePose(hipsParentIndex, _poses);
        _poses[hipsIndex] = hipsParentPose.inverse() * hipsPose;
    } else {
        _poses[hipsIndex] = hipsPose;
    }

    DataRow currentRow = _rows[floorf(_frame)];
    float bestCost = FLT_MAX;
    DataRow bestRow;

    glm::vec3 rigDesiredVel = animVars.lookupRaw(QString("hipsVelocity"), glm::vec3());
    glm::vec3 rigDesiredAngularVel = animVars.lookupRaw(QString("hipsAngularVelocity"), glm::vec3());

    qDebug() << "AJT: MotionMatching";
    qDebug() << "AJT:     _frame = " << _frame;

    // rotate desiredVel goal into hips local space.
    Goal goal;
    goal.hipsVel = invRigHipsPose.xformVectorFast(rigDesiredVel);
    goal.hipsAngularVel = invRigHipsPose.xformVectorFast(rigDesiredAngularVel);

    for (size_t i = 0; i < _rows.size(); i++) {

        // AJT: debug the frames with the largest backward vel and largest forward vel.
        if (_rows[i].data[FRAME_INDEX] == 6479 || _rows[i].data[FRAME_INDEX] == 8396) {
            debug = true;
        } else {
            debug = false;
        }

        if (debug) {
            qDebug() << "AJT:      testing frame =" << i;
        }

        float thisCost = computeCost(currentRow, _rows[i], goal);
        if (thisCost < bestCost) {
            bestCost = thisCost;
            bestRow = _rows[i];
        }
    }

    qDebug() << "AJT:     bestRow.frame =" << bestRow.data[FRAME_INDEX] << ", bestCost =" << bestCost;

    const float SAME_LOCATION_THRESHOLD = _sameLocationWindow * FPS;
    bool theWinnerIsAtTheSameLocation = fabs(_frame - bestRow.data[FRAME_INDEX]) < SAME_LOCATION_THRESHOLD;
    if (!theWinnerIsAtTheSameLocation)
    {
        // jump to the winning location
        _frame = bestRow.data[FRAME_INDEX];
        currentRow = bestRow;
        qDebug() << "AJT:     JUMP TO FRAME " << _frame;
    } else {
        qDebug() << "AJT:     same loc, delta = " << fabs(_frame - bestRow.data[FRAME_INDEX]);
    }

    // grab leftfoot from currentRow
    glm::vec3 currentLeftFootPos(currentRow.data[LEFTFOOT_PX_INDEX], currentRow.data[LEFTFOOT_PY_INDEX], currentRow.data[LEFTFOOT_PZ_INDEX]);
    glm::quat currentLeftFootRot = exp(glm::quat(0.0f, currentRow.data[LEFTFOOT_RX_INDEX], currentRow.data[LEFTFOOT_RY_INDEX], currentRow.data[LEFTFOOT_RZ_INDEX]));
    AnimPose currentLeftFootPose(currentLeftFootRot, currentLeftFootPos);

    // grap rightfoot from currentRow
    glm::vec3 currentRightFootPos(currentRow.data[RIGHTFOOT_PX_INDEX], currentRow.data[RIGHTFOOT_PY_INDEX], currentRow.data[RIGHTFOOT_PZ_INDEX]);
    glm::quat currentRightFootRot = exp(glm::quat(0.0f, currentRow.data[RIGHTFOOT_RX_INDEX], currentRow.data[RIGHTFOOT_RY_INDEX], currentRow.data[RIGHTFOOT_RZ_INDEX]));
    AnimPose currentRightFootPose(currentRightFootRot, currentRightFootPos);

    // transform them from rig horizontal-hipsRelative into geom
    AnimPose rigToGeometryPose = AnimPose(glm::inverse(context.getGeometryToRigMatrix()));
    currentLeftFootPose = rigToGeometryPose * rigHipsPose * currentLeftFootPose;
    currentLeftFootPose.scale() = glm::vec3(1.0f);
    currentRightFootPose = rigToGeometryPose * rigHipsPose * currentRightFootPose;
    currentRightFootPose.scale() = glm::vec3(1.0f);

    // DONT DO THIS, Instead output the poses in the triggers for the IK node to pickup.
    /*
    // set the feet pose
    int leftFootIndex = _skeleton->nameToJointIndex("LeftFoot");
    int leftFootParentIndex = _skeleton->getParentIndex(leftFootIndex);
    int rightFootIndex = _skeleton->nameToJointIndex("RightFoot");
    int rightFootParentIndex = _skeleton->getParentIndex(rightFootIndex);
    AnimPose leftFootParentPose = _skeleton->getAbsolutePose(leftFootParentIndex, _poses);
    AnimPose rightFootParentPose = _skeleton->getAbsolutePose(rightFootParentIndex, _poses);
    _poses[leftFootIndex] = leftFootParentPose.inverse() * currentLeftFootPose;
    _poses[rightFootIndex] = rightFootParentPose.inverse() * currentRightFootPose;
    */

    processOutputJoints(triggersOut);

    // AJT: hack for debugging
    triggersOut.set("_frame", _frame);

    // Output feet poses for IK node to pick them up.
    triggersOut.set("mainStateMachineLeftFootPosition", currentLeftFootPose.trans());
    triggersOut.set("mainStateMachineLeftFootRotation", currentLeftFootPose.rot());
    triggersOut.set("mainStateMachineRightFootPosition", currentRightFootPose.trans());
    triggersOut.set("mainStateMachineRightFootRotation", currentRightFootPose.rot());

    return _poses;
}

const AnimPoseVec& AnimMotionMatching::getPosesInternal() const {
    return _poses;
}
