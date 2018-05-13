//
//  AnimSplineIK.h
//
//  Created by Anthony J. Thibault on 5/12/18.
//  Copyright (c) 2018 High Fidelity, Inc. All rights reserved.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_AnimSplineIK_h
#define hifi_AnimSplineIK_h

#include "AnimNode.h"

// Three bone IK chain

class AnimSplineIK : public AnimNode {
public:
    friend class AnimTests;

    AnimSplineIK(const QString& id, float alpha);
    virtual ~AnimSplineIK() override;

    virtual const AnimPoseVec& evaluate(const AnimVariantMap& animVars, const AnimContext& context, float dt, Triggers& triggersOut) override;

protected:
    // for AnimDebugDraw rendering
    virtual const AnimPoseVec& getPosesInternal() const override;

    AnimPoseVec _poses;

    // no copies
    AnimSplineIK(const AnimSplineIK&) = delete;
    AnimSplineIK& operator=(const AnimSplineIK&) = delete;
};

#endif // hifi_AnimSplineIK_h
