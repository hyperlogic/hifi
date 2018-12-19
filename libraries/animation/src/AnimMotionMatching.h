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
    static const size_t DATA_ROW_SIZE = 31;
    struct DataRow {
        float data[DATA_ROW_SIZE];
    };

    struct Goal {
        glm::vec3 hipsVel;
        glm::vec3 hipsAngularVel;
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

    // no copies
    AnimMotionMatching(const AnimMotionMatching&) = delete;
    AnimMotionMatching& operator=(const AnimMotionMatching&) = delete;
};

#endif // hifi_AnimMotionMatching_h
