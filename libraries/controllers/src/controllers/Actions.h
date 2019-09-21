//
//  Created by Bradley Austin Davis on 2015/10/19
//  Copyright 2015 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#pragma once
#ifndef hifi_controller_Actions_h
#define hifi_controller_Actions_h

#include <QtCore/QObject>
#include <QtCore/QVector>

#include "InputDevice.h"

namespace controller {

// Actions are the output channels of the Mapper, that's what the InputChannel map to
// For now the Actions are hardcoded, this is bad, but we will fix that in the near future
enum class Action {
    TRANSLATE_X = 0,
    TRANSLATE_Y,
    TRANSLATE_Z,
    ROTATE_X, PITCH = ROTATE_X,
    ROTATE_Y, YAW = ROTATE_Y,
    ROTATE_Z, ROLL = ROTATE_Z,

    DELTA_PITCH,
    DELTA_YAW,
    DELTA_ROLL,

    STEP_YAW,
    // FIXME does this have a use case?
    STEP_PITCH,
    // FIXME does this have a use case?
    STEP_ROLL,

    STEP_TRANSLATE_X,
    STEP_TRANSLATE_Y,
    STEP_TRANSLATE_Z,

    TRANSLATE_CAMERA_Z,
    NUM_COMBINED_AXES,

    LEFT_HAND = NUM_COMBINED_AXES,
    RIGHT_HAND,
    LEFT_FOOT,
    RIGHT_FOOT,
    HIPS,
    SPINE2,
    HEAD,

    LEFT_HAND_CLICK,
    RIGHT_HAND_CLICK,

    ACTION1,
    ACTION2,

    CONTEXT_MENU,
    TOGGLE_MUTE,
    TOGGLE_PUSHTOTALK,
    CYCLE_CAMERA,
    TOGGLE_OVERLAY,

    SHIFT,

    UI_NAV_LATERAL,
    UI_NAV_VERTICAL,
    UI_NAV_GROUP,
    UI_NAV_SELECT,
    UI_NAV_BACK,

    // Pointer/Reticle control
    RETICLE_CLICK,
    RETICLE_X,
    RETICLE_Y,

    // Bisected aliases for RETICLE_X/RETICLE_Y
    RETICLE_LEFT,
    RETICLE_RIGHT,
    RETICLE_UP,
    RETICLE_DOWN,

    // Bisected aliases for TRANSLATE_Z
    LONGITUDINAL_BACKWARD,
    LONGITUDINAL_FORWARD,

    // Bisected aliases for TRANSLATE_X
    LATERAL_LEFT,
    LATERAL_RIGHT,

    // Bisected aliases for TRANSLATE_Y
    VERTICAL_DOWN,
    VERTICAL_UP,

    // Bisected aliases for ROTATE_Y
    YAW_LEFT,
    YAW_RIGHT,

    // Bisected aliases for ROTATE_X
    PITCH_DOWN,
    PITCH_UP,

    // Bisected aliases for TRANSLATE_CAMERA_Z
    BOOM_IN,
    BOOM_OUT,

    LEFT_ARM,
    RIGHT_ARM,

    LEFT_HAND_THUMB1,
    LEFT_HAND_THUMB2,
    LEFT_HAND_THUMB3,
    LEFT_HAND_THUMB4,
    LEFT_HAND_INDEX1,
    LEFT_HAND_INDEX2,
    LEFT_HAND_INDEX3,
    LEFT_HAND_INDEX4,
    LEFT_HAND_MIDDLE1,
    LEFT_HAND_MIDDLE2,
    LEFT_HAND_MIDDLE3,
    LEFT_HAND_MIDDLE4,
    LEFT_HAND_RING1,
    LEFT_HAND_RING2,
    LEFT_HAND_RING3,
    LEFT_HAND_RING4,
    LEFT_HAND_PINKY1,
    LEFT_HAND_PINKY2,
    LEFT_HAND_PINKY3,
    LEFT_HAND_PINKY4,

    RIGHT_HAND_THUMB1,
    RIGHT_HAND_THUMB2,
    RIGHT_HAND_THUMB3,
    RIGHT_HAND_THUMB4,
    RIGHT_HAND_INDEX1,
    RIGHT_HAND_INDEX2,
    RIGHT_HAND_INDEX3,
    RIGHT_HAND_INDEX4,
    RIGHT_HAND_MIDDLE1,
    RIGHT_HAND_MIDDLE2,
    RIGHT_HAND_MIDDLE3,
    RIGHT_HAND_MIDDLE4,
    RIGHT_HAND_RING1,
    RIGHT_HAND_RING2,
    RIGHT_HAND_RING3,
    RIGHT_HAND_RING4,
    RIGHT_HAND_PINKY1,
    RIGHT_HAND_PINKY2,
    RIGHT_HAND_PINKY3,
    RIGHT_HAND_PINKY4,

    LEFT_SHOULDER,
    RIGHT_SHOULDER,
    LEFT_FORE_ARM,
    RIGHT_FORE_ARM,
    LEFT_LEG,
    RIGHT_LEG,
    LEFT_UP_LEG,
    RIGHT_UP_LEG,
    LEFT_TOE_BASE,
    RIGHT_TOE_BASE,

    TRACKED_OBJECT_00,
    TRACKED_OBJECT_01,
    TRACKED_OBJECT_02,
    TRACKED_OBJECT_03,
    TRACKED_OBJECT_04,
    TRACKED_OBJECT_05,
    TRACKED_OBJECT_06,
    TRACKED_OBJECT_07,
    TRACKED_OBJECT_08,
    TRACKED_OBJECT_09,
    TRACKED_OBJECT_10,
    TRACKED_OBJECT_11,
    TRACKED_OBJECT_12,
    TRACKED_OBJECT_13,
    TRACKED_OBJECT_14,
    TRACKED_OBJECT_15,
    SPRINT,

    LEFT_EYE,
    RIGHT_EYE,
    LEFT_EYE_BLINK,
    RIGHT_EYE_BLINK,

    /*
    // AJT: not sure if this is strictly necessary....
    USER_INPUT_00, USER_INPUT_01, USER_INPUT_02, USER_INPUT_03,
    USER_INPUT_04, USER_INPUT_05, USER_INPUT_06, USER_INPUT_07,
    USER_INPUT_08, USER_INPUT_09, USER_INPUT_10, USER_INPUT_11,
    USER_INPUT_12, USER_INPUT_13, USER_INPUT_14, USER_INPUT_15,

    USER_INPUT_16, USER_INPUT_17, USER_INPUT_18, USER_INPUT_19,
    USER_INPUT_20, USER_INPUT_21, USER_INPUT_22, USER_INPUT_23,
    USER_INPUT_24, USER_INPUT_25, USER_INPUT_26, USER_INPUT_27,
    USER_INPUT_28, USER_INPUT_29, USER_INPUT_30, USER_INPUT_31,

    USER_INPUT_32, USER_INPUT_33, USER_INPUT_34, USER_INPUT_35,
    USER_INPUT_36, USER_INPUT_37, USER_INPUT_38, USER_INPUT_39,
    USER_INPUT_40, USER_INPUT_41, USER_INPUT_42, USER_INPUT_43,
    USER_INPUT_44, USER_INPUT_45, USER_INPUT_46, USER_INPUT_47,

    USER_INPUT_48, USER_INPUT_49, USER_INPUT_50, USER_INPUT_51,
    USER_INPUT_52, USER_INPUT_53, USER_INPUT_54, USER_INPUT_55,
    USER_INPUT_56, USER_INPUT_57, USER_INPUT_58, USER_INPUT_59,
    USER_INPUT_60, USER_INPUT_61, USER_INPUT_62, USER_INPUT_63,
    */

    NUM_ACTIONS
};

template <typename T>
int toInt(T enumValue) { return static_cast<int>(enumValue); }

class ActionsDevice : public QObject, public InputDevice {
    Q_OBJECT
    Q_PROPERTY(QString name READ getName)

public:
    ActionsDevice();

    EndpointPointer createEndpoint(const Input& input) const override;
    Input::NamedVector getAvailableInputs() const override;

};

}

#endif // hifi_StandardController_h
