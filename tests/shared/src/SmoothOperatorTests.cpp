//
//  SmoothOperatorTests.cpp
//  tests/shared/src
//
//  Created by Anthony J. Thibault on 10/9/2015
//  Copyright 2015 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "SmoothOperatorTests.h"
#include "SmoothOperator.h"

#include "../QTestExtensions.h"

QTEST_MAIN(SmoothOperatorTests)

static quint64 secToUsec(float t) {
    return (quint64)(t * (float)USECS_PER_SECOND);
}

const float FUZZ = EPSILON;

void SmoothOperatorTests::testInterpolation() {
    SmoothOperator op;

    glm::quat rotY90 = glm::angleAxis(PI/2.0f, glm::vec3(0.0f, 1.0f, 0.0f));
    glm::quat rotY45 = glm::angleAxis(PI/4.0f, glm::vec3(0.0f, 1.0f, 0.0f));

    SmoothOperator::State s0(secToUsec(0.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::quat(),
                             glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 0.0f));
    SmoothOperator::State s1(secToUsec(1.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::quat(),
                             glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 0.0f));
    SmoothOperator::State s2(secToUsec(2.0f), glm::vec3(3.0f, 0.0f, 0.0f), rotY90,
                             glm::vec3(2.0f, 0.0f, 0.0f), glm::vec3(0.0f, PI/2.0f, 0.0f));

    SmoothOperator::State a = op.interpolate(secToUsec(1.0f), s0, s1);

    QCOMPARE_WITH_ABS_ERROR(a.position, s1.position, FUZZ);
    QCOMPARE_WITH_ABS_ERROR(a.linearVel, glm::vec3(1.0f, 0.0f, 0.0f), FUZZ);
    QCOMPARE_WITH_ABS_ERROR(a.rotation, glm::quat(), FUZZ);
    QCOMPARE_WITH_ABS_ERROR(a.angularVel, glm::vec3(0.0f), FUZZ);

    SmoothOperator::State b = op.interpolate(secToUsec(1.5f), s1, s2);

    QCOMPARE_WITH_ABS_ERROR(b.position, glm::vec3(2.0f, 0.0f, 0.0f), FUZZ);
    QCOMPARE_WITH_ABS_ERROR(b.linearVel, glm::vec3(1.5f, 0.0f, 0.0f), FUZZ);
    QCOMPARE_WITH_ABS_ERROR(b.rotation, rotY45, FUZZ);
    QCOMPARE_WITH_ABS_ERROR(b.angularVel, glm::vec3(0.0f, PI/4.0f, 0.0f), FUZZ);
}

