//
//  AnimLowVelocityFilter.h
//
//  Created by Anthony J. Thibault on 11/30/17.
//  Copyright (c) 2017 High Fidelity, Inc. All rights reserved.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_AnimLowVelocityFilter_h
#define hifi_AnimLowVelocityFilter_h

#include "AnimNode.h"

// AJT: TODO DESCRIPTION HERE

class AnimLowVelocityFilter : public AnimNode {
public:
    friend class AnimTests;

    AnimLowVelocityFilter(const QString& id);
    virtual ~AnimLowVelocityFilter() override;

    virtual const AnimPoseVec& evaluate(const AnimVariantMap& animVars, const AnimContext& context, float dt, Triggers& triggersOut) override;
    virtual const AnimPoseVec& overlay(const AnimVariantMap& animVars, const AnimContext& context, float dt, Triggers& triggersOut, const AnimPoseVec& underPoses) override;

protected:
    // for AnimDebugDraw rendering
    virtual const AnimPoseVec& getPosesInternal() const override;

    AnimPoseVec _relativePoses;

    // no copies
    AnimLowVelocityFilter(const AnimLowVelocityFilter&) = delete;
    AnimLowVelocityFilter& operator=(const AnimLowVelocityFilter&) = delete;
};

#endif // hifi_AnimLowVelocityFilter_h
