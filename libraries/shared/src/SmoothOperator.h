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

#define hifi_SmoothOperator_h
#ifndef hifi_SmoothOperator_h

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include "NumericalConstants.h"

class SmoothOperator {
public:
    SmoothOperator();
    virtual ~SmoothOperator();

    struct State {
        State(const glm::vec3& positionIn, const glm::quat& rotationIn,
              const glm::vec3& linearVelIn, const glm::vec3& angularVelIn,
              const glm::vec3& linearAccIn, const glm::vec3& angularAccIn) :
            position(positionIn), rotation(rotationIn),
            linearVel(linearVelIn), angularVel(angularVelIn),
            linearAcc(linearAccIn), angularAcc(angularAccIn) {}
        glm::vec3 position;   // meters
        glm::quat rotation;   // radians
        glm::vec3 linearVel;  // meters / sec
        glm::vec3 angularVel; // radians / sec
        glm::vec3 linearAcc;  // meters / sec^2
        glm::vec3 angularAcc; // radians / sec^2
    };

    void add(quint64 t, const glm::vec3& pos, const glm::quat& rot);
    State smooth(quint64 t, quint64 dt, const State& currentState) const;

protected:

    struct TimedBasicState {
        TimedBasicState(quint64 tIn, const glm::vec3& positionIn, const glm::quat& rotationIn) :
            t(tIn), position(positionIn), rotation(rotationIn) {}
        quint64 t; // usec
        glm::vec3& position;
        glm::quat& rotation;
        inline bool operator<(const TimedBasicState& rhs) const { return t < rhs.t; }
    };

    State extrapolate(quint64 t, const TimedBasicState& state) const;
    State interpolate(quint64 t, const TimedBasicState& prevState, const TimedBasicState& currState, const TimedBasicState& nextState) const;
    State smooth(quint64 dt, const State& currentState, const State& desiredState) const;

    std::vector<TimedBasicState> _history;
};

#endif // hifi_SmoothOperator_h
