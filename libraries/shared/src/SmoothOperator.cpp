#include "SmoothOperator.h"
#include "GLMHelpers.h"

const size_t MAX_STATE_HISTORY_SIZE = 20;
const quint64 BUFFER_LATENCY_USEC = 100 * USECS_PER_MSEC;  // 100ms (in usec)
const float MAX_EXTRAPOLATION_TIME =  0.2f; // 200ms (in sec)

struct Derivatives {
    glm::vec3 linearVel;  // meters / sec
    glm::vec3 angularVel; // radians / sec
};

static float usecToSec(quint64 t) {
    return t / (float)USECS_PER_SECOND;
}

static glm::quat angularSpinToQuat(const glm::vec3& spin) {
    float theta = glm::length(spin);
    if (theta > 0.0f) {
        return glm::angleAxis(theta, glm::normalize(spin));
    } else {
        return glm::quat();
    }
}

static glm::vec3 quatToAngularSpin(const glm::quat& q) {
    float theta = glm::angle(q);
    glm::vec3 axis = glm::axis(q);
    return axis * theta;
}

static float approach(float t) {
    return 1.0f - (1.0f / (t + 1.0f));
}

SmoothOperator::SmoothOperator() {
}

SmoothOperator::~SmoothOperator() {
}

void SmoothOperator::add(quint64 t, const glm::vec3& pos, const glm::quat& rot) {
    if (_history.size() < MAX_STATE_HISTORY_SIZE) {
        _history.push_back(State(t, pos, rot, glm::vec3(0), glm::vec3(0)));
    } else {
        // overwrite oldest
        _history[0] = State(t, pos, rot, glm::vec3(0), glm::vec3(0));
    }
    std::sort(_history.begin(), _history.end());
    computeDerivatives();
}

void SmoothOperator::computeDerivatives() {
    if (_history.empty()) {
        return;
    }

    _history[0].linearVel = glm::vec3(0);
    _history[0].angularVel = glm::vec3(0);

    for (int i = 1; i < _history.size(); i++) {
        float dt = usecToSec(_history[i].t - _history[i-1].t);
        if (dt > 0.0f) {
            _history[i].linearVel = glm::vec3(0.0f);
            _history[i].angularVel = glm::vec3(0.0f);
        } else {
            // calculate derivatives
            _history[i].linearVel = (_history[i].position - _history[i-1].position) * (1.0f / dt);
            glm::vec3 dq = quatToAngularSpin(_history[i].rotation * glm::inverse(_history[i-1].rotation));
            _history[i].angularVel = dq * (1.0f / dt);
        }
    }
}

SmoothOperator::State SmoothOperator::smooth(quint64 t, quint64 dt, const State& currentState) const {
    if (_history.size() == 0) {
        return currentState;
    }

    quint64 sampleTime = t + dt - BUFFER_LATENCY_USEC;

    State desiredState;

    // find the youngest sample that is still in the future.
    size_t i;
    for (i = 0; i < _history.size(); i++) {
        if (_history[i].t >= sampleTime)
            break;
    }

    if (i == _history.size()) {
        // we ran past the end of our history, extrapolate based on the last samples.
        desiredState = extrapolate(sampleTime, _history[i-1]);
    } else {
        // interpolate between s0 and s1
        auto s1 = std::min(0U, i);
        auto s0 = std::min(0U, s1 - 1);
        desiredState = interpolate(sampleTime, _history[s0], _history[s1]);
    }
    return smooth(dt, currentState, desiredState);
}

SmoothOperator::State SmoothOperator::extrapolate(quint64 t, const State& s) const {
    assert(s.t <= t);

    float dt = usecToSec(t - s.t);

    // modify dt, so it asymptotically approaches MAX_EXTRAPOLATION_TIME.
    dt = approach(dt / MAX_EXTRAPOLATION_TIME) * MAX_EXTRAPOLATION_TIME;

    // integrate forward
    glm::vec3 position = s.position + s.linearVel * dt;
    glm::quat rotation = angularSpinToQuat(s.angularVel * dt) * s.rotation;

    return State(t, position, rotation, s.linearVel, s.angularVel);
}

SmoothOperator::State SmoothOperator::interpolate(quint64 t, const State& s0, const State& s1) const {
    assert(t <= s1.t);
    assert(s0.t <= s1.t);

    float dt = usecToSec(t - s0.t);
    float alpha = dt / usecToSec(s1.t - s0.t);

    // interpolate position
    glm::vec3 position = lerp(s0.position, s1.position, alpha);
    glm::vec3 linearVel = lerp(s0.linearVel, s1.linearVel, alpha);

    // interpolate rotation
    glm::quat rotation = glm::normalize(glm::lerp(s0.rotation, s1.rotation, alpha));
    glm::vec3 angularVel = lerp(s0.angularVel, s1.angularVel, alpha);

    return State(t, position, rotation, linearVel, angularVel);
}

SmoothOperator::State SmoothOperator::smooth(quint64 dt, const State& currentState, const State& desiredState) const {
    // TODO: some kind of c2 smoothing, b-splines?
    return desiredState;
}


