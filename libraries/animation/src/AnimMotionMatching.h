//
//  AnimMotionMatching.h
//
//  Created by Anthony J. Thibault on 12/18/18.
//  Copyright (c) 2018 High Fidelity, Inc. All rights reserved.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_AnimMotionMatching_h
#define hifi_AnimMotionMatching_h

#include <string>
#include "AnimNode.h"


enum RowIndices {
    ID_INDEX = 0,
    HIPS_PX_INDEX,
    HIPS_PY_INDEX,
    HIPS_PZ_INDEX,
    HIPS_RX_INDEX,
    HIPS_RY_INDEX,
    HIPS_RZ_INDEX,
    HIPS_RW_INDEX,
    LEFTFOOT_PX_INDEX,
    LEFTFOOT_PY_INDEX,
    LEFTFOOT_PZ_INDEX,
    LEFTFOOT_RX_INDEX,
    LEFTFOOT_RY_INDEX,
    LEFTFOOT_RZ_INDEX,
    LEFTFOOT_RW_INDEX,
    RIGHTFOOT_PX_INDEX,
    RIGHTFOOT_PY_INDEX,
    RIGHTFOOT_PZ_INDEX,
    RIGHTFOOT_RX_INDEX,
    RIGHTFOOT_RY_INDEX,
    RIGHTFOOT_RZ_INDEX,
    RIGHTFOOT_RW_INDEX,
    HEAD_PX_INDEX,
    HEAD_PY_INDEX,
    HEAD_PZ_INDEX,
    HEAD_RX_INDEX,
    HEAD_RY_INDEX,
    HEAD_RZ_INDEX,
    HEAD_RW_INDEX,
    LEFTHAND_PX_INDEX,
    LEFTHAND_PY_INDEX,
    LEFTHAND_PZ_INDEX,
    LEFTHAND_RX_INDEX,
    LEFTHAND_RY_INDEX,
    LEFTHAND_RZ_INDEX,
    LEFTHAND_RW_INDEX,
    RIGHTHAND_PX_INDEX,
    RIGHTHAND_PY_INDEX,
    RIGHTHAND_PZ_INDEX,
    RIGHTHAND_RX_INDEX,
    RIGHTHAND_RY_INDEX,
    RIGHTHAND_RZ_INDEX,
    RIGHTHAND_RW_INDEX,
    ROOTTRAJ1_PX_INDEX,
    ROOTTRAJ1_PY_INDEX,
    ROOTTRAJ1_PZ_INDEX,
    ROOTTRAJ2_PX_INDEX,
    ROOTTRAJ2_PY_INDEX,
    ROOTTRAJ2_PZ_INDEX,
    ROOTTRAJ3_PX_INDEX,
    ROOTTRAJ3_PY_INDEX,
    ROOTTRAJ3_PZ_INDEX,
    ROOTTRAJ4_PX_INDEX,
    ROOTTRAJ4_PY_INDEX,
    ROOTTRAJ4_PZ_INDEX,
    ROOTTRAJ5_PX_INDEX,
    ROOTTRAJ5_PY_INDEX,
    ROOTTRAJ5_PZ_INDEX,
    ROOTTRAJ6_PX_INDEX,
    ROOTTRAJ6_PY_INDEX,
    ROOTTRAJ6_PZ_INDEX,
    ROOTTRAJ7_PX_INDEX,
    ROOTTRAJ7_PY_INDEX,
    ROOTTRAJ7_PZ_INDEX,
    ROOTTRAJ8_PX_INDEX,
    ROOTTRAJ8_PY_INDEX,
    ROOTTRAJ8_PZ_INDEX,
    HEADTRAJ1_PX_INDEX,
    HEADTRAJ1_PY_INDEX,
    HEADTRAJ1_PZ_INDEX,
    HEADTRAJ2_PX_INDEX,
    HEADTRAJ2_PY_INDEX,
    HEADTRAJ2_PZ_INDEX,
    HEADTRAJ3_PX_INDEX,
    HEADTRAJ3_PY_INDEX,
    HEADTRAJ3_PZ_INDEX,
    HEADTRAJ4_PX_INDEX,
    HEADTRAJ4_PY_INDEX,
    HEADTRAJ4_PZ_INDEX,
    HEADTRAJ5_PX_INDEX,
    HEADTRAJ5_PY_INDEX,
    HEADTRAJ5_PZ_INDEX,
    HEADTRAJ6_PX_INDEX,
    HEADTRAJ6_PY_INDEX,
    HEADTRAJ6_PZ_INDEX,
    HEADTRAJ7_PX_INDEX,
    HEADTRAJ7_PY_INDEX,
    HEADTRAJ7_PZ_INDEX,
    HEADTRAJ8_PX_INDEX,
    HEADTRAJ8_PY_INDEX,
    HEADTRAJ8_PZ_INDEX,
    DATA_ROW_SIZE
};

class AnimMotionMatching : public AnimNode {
public:
    AnimMotionMatching(const QString& id);
    virtual ~AnimMotionMatching() override;

    virtual const AnimPoseVec& evaluate(const AnimVariantMap& animVars, const AnimContext& context, float dt, AnimVariantMap& triggersOut) override;
protected:
    // for AnimDebugDraw rendering
    virtual const AnimPoseVec& getPosesInternal() const override;

    AnimPoseVec _poses;

public:

    struct DataRow {
        float data[DATA_ROW_SIZE];
    };

    struct TrajectoryPoint {
        glm::vec3 position;
        glm::quat rotation;
    };

    static const size_t NUM_TRAJECTORY_POINTS = 4;
    struct Goal {
        TrajectoryPoint desiredRootTrajectory[NUM_TRAJECTORY_POINTS];
        TrajectoryPoint previousHeadTrajectory[NUM_TRAJECTORY_POINTS];
        bool hmdMode;
    };
protected:

    float AnimMotionMatching::computeCurrentCost(const DataRow& currentRow, const DataRow& candidateRow) const;
    float AnimMotionMatching::computeFutureCost(const DataRow& currentRow, const Goal& goal) const;
    float AnimMotionMatching::computeCost(const DataRow& currentRow, const DataRow& candidateRow, const Goal& goal) const;

    std::vector<DataRow> _rows;
    float _frame;

    float _currentFootPositionCostFactor = 1.0f;
    float _currentFootRotationCostFactor = 0.5f;
    float _currentFootVelocityCostFactor = 0.25f;
    float _currentFootAngularVelocityCostFactor = 0.0f;
    float _futureHipsVelocityCostFactor = 1.0f;
    float _futureHipsAngularVelocityCostFactor = 0.0f;
    float _futureCostFactor = 0.25f;
    float _sameLocationWindow = 0.2f;

    float _interpTimer = -1.0f;

    struct Snapshot {
        AnimPose head;
        AnimPose hips;
        AnimPose leftFoot;
        AnimPose rightFoot;
        AnimPose leftHand;
        AnimPose rightHand;
    };
    Snapshot _snapshot;

    // no copies
    AnimMotionMatching(const AnimMotionMatching&) = delete;
    AnimMotionMatching& operator=(const AnimMotionMatching&) = delete;
};

#endif // hifi_AnimMotionMatching_h
