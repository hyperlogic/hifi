#include "SmoothOperator.h"

const size_t MAX_STATE_HISTORY_SIZE = 10;
const quint64 BUFFER_LATENCY_USEC = 100 * MSECS_PER_USEC;  // 100ms (in usec)
const float MAX_EXTRAPOLATION_TIME =  0.2f; // 200ms (in sec)

static float usecToSec(quint64 t) {
    return t / (float)USECS_PER_SECOND;
}

SmoothOperator::SmoothOperator() {
}

SmoothOperator::~SmoothOperator() {
}

void SmoothOperator::add(quint64 t, const glm::vec3& pos, const glm::quat& rot) {
    if (_history.size < MAX_STATE_HISTORY_SIZE) {
        _history.push_back(TimedBasicState(t, pos, rot));
    } else {
        // overwrite oldest
        _history[0] = TimedBasicState(t, pos, rot);
    }
    std::sort(_history.begin(), _history.end());
}

State SmoothOperator::smooth(quint64 t, quint64 dt, const State& currentState) const {
    if (_history.size() == 0) {
        return currentState;
    }

    quint64 sampleTime = t + dt - BUFFER_LATENCY_USEC;

    State desiredState;
    if (sampleTime > _history.back().t) {
        // we ran past the end of our history, extrapolate from our last sample
        desiredState = extrapolate(sampleTime, _history.back());
    } else {
        // find a sample we can interpolate into.
        for (int nextIndex = 0; nextIndex < _history.size(); nextIndex++) {
            if (_history[i].t >= sampleTime)
                break;
        }
        assert(nextIndex >= 0 && nextIndex < _history.size());
        int currIndex = std::min(0, nextIndex - 1);
        int prevIndex = std::min(0, currIndex - 1);
        desiredState = interpolate(sampleTime, _history[prevIndex], _history[currIndex], _history[nextIndex]);
    }
    return smooth(dt, currentState, desiredState);
}

static float approach(float t) {
    return 1.0f - (1.0f / (t + 1.0f));
}

static glm::quat angularSpinToQuat(const glm::vec3& spin) {
    float theta = glm::length(spin);
    if (theta > 0.0f) {
        return glm::angleAxis(theta, glm::normalize(spin));
    } else {
        return glm::quat();
    }
}

static glm::quat quatToAngularSpin(const glm::quat& q) {
    float theta = glm::angle(q);
    glm::vec3 axis = glm::axis(q);
    return axis * theta;
}

SmoothOperator::State SmoothOperator::extrapolate(quint64 t, const TimedBasicState& state) const {
    float dt = usecToSec(t - state.t);

    // modify dt, so it asymptotically approaches MAX_EXTRAPOLATION_TIME.
    dt = approach(dt / MAX_EXTRAPOLATION_TIME) * MAX_EXTRAPOLATION_TIME;

    // integrate position forward
    glm::vec3 position = state.position + state.linearVel * dt + state.linearAcc * dt * dt;
    glm::vec3 linearVel = state.linearVel + state.linearAcc * dt;
    glm::vec3 linearAcc = state.linearAcc;

    // integrate rotation
    glm::quat rotation = angularSpinToQuat(state.angularVel * dt + state.angularAcc * dt * dt) * state.rotation;
    glm::vec3 angularVel = state.angularVel + state.angularAcc * dt;
    glm::vec3 angularAcc = state.angularAcc;

    return State(position, rotation, linearVel, angularVel, linearAcc, angularAcc);
}

SmoothOperator::State SmoothOperator::interpolate(quint64 t, const TimedBasicState& prevState, const TimedBasicState& currState, const TimedBasicState& nextState) const {
    float dt = usecToSec(t - currState.t);
    float alpha = dt / usecToSec(nextState.t - currState.t);

    // interpolate position
    glm::vec3 position = lerp(currState.position, nextState.position, alpha);

    // calculate derivatives
    glm::vec3 linearVel = (nextState.position - currState.position) * (1.0f / dt);
    glm::vec3 prevLinearVel = (currState.position - nextState.position) * (1.0f / dt);
    glm::vec3 linearAcc = (linearVel - prevLinearVel) * (1.0f / dt);

    // interpolate rotation
    glm::quat rotation = glm::normalize(glm::lerp(currState.rotation, nextState.rotation, alpha));

    // calculate derivatives
    glm::vec3 dq = quatToAngularSpin(nextState.rotation * glm::inverse(currState.rotation));
    glm::vec3 angularVel = dq * (1.0f / dt);
    glm::vec3 prevDq = quatToAngularSpin(currState.rotation * glm::inverse(prevState.rotation));
    glm::vec3 prevAngularVel = prevDq * (1.0f / dt);
    glm::vec3 angularAcc = (dq - prevDq) * (1.0f / dt);

    return State(position, rotation, linearVel, angularVel, linearAcc, angularAcc);
}

SmoothOperator::State SmoothOperator::smooth(quint64 dt, const State& currentState, const State& desiredState) const {
    // TODO: some kind of c2 smoothing, b-splines?
    return desiredState;
}


