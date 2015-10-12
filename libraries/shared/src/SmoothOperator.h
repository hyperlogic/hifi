//
//  SmoothOperator.h
//  libraries/shared/src/
//
//  Produces smooth rigid body motion from a history of timestamped
//  samples, usually from a unreliable or low frequency data, such
//  as network packets, or threads with low or unpredictable update rates.
//
//  Created by Anthony Thibault on 10/9/15.
//  Copyright (c) 2015 High Fidelity, Inc. All rights reserved.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_SmoothOperator_h
#define hifi_SmoothOperator_h

#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include "NumericalConstants.h"

class SmoothOperatorTests;

class SmoothOperator {
    friend class SmoothOperatorTests;
public:
    SmoothOperator();
    virtual ~SmoothOperator();

    struct State {
        State() {}
        State(quint64 tIn, const glm::vec3& positionIn, const glm::quat& rotationIn,
              const glm::vec3& linearVelIn, const glm::vec3& angularVelIn) :
            t(tIn), position(positionIn), rotation(rotationIn),
            linearVel(linearVelIn), angularVel(angularVelIn) {}
        quint64 t; // usec
        glm::vec3 position;   // meters
        glm::quat rotation;   // radians
        glm::vec3 linearVel;  // meters / sec
        glm::vec3 angularVel; // radians / sec
        inline bool operator<(const State& rhs) const { return t < rhs.t; }
    };

    void add(quint64 t, const glm::vec3& pos, const glm::quat& rot);
    State smooth(quint64 t, quint64 dt, const State& currentState) const;

protected:

    void computeDerivatives();
    State extrapolate(quint64 t, const State& s) const;
    State interpolate(quint64 t, const State& s0, const State& s1) const;

    std::vector<State> _history;
};

#endif // hifi_SmoothOperator_h
