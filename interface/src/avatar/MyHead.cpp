//
//  Created by Bradley Austin Davis on 2017/04/27
//  Copyright 2013-2017 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "MyHead.h"

#include <glm/gtx/quaternion.hpp>
#include <gpu/Batch.h>

#include <NodeList.h>
#include <recording/Deck.h>
#include <Rig.h>
#include <trackers/FaceTracker.h>
#include <FaceshiftConstants.h>

#include "devices/DdeFaceTracker.h"
#include "Application.h"
#include "MyAvatar.h"

using namespace std;

static const int BLENDSHAPE_COUNT = 52;
static enum controller::Action BLENDSHAPE_ACTIONS[BLENDSHAPE_COUNT] = {
    controller::Action::BROW_INNER_UP,
    controller::Action::BROW_DOWN_LEFT,
    controller::Action::BROW_DOWN_RIGHT,
    controller::Action::BROW_OUTER_UP_LEFT,
    controller::Action::BROW_OUTER_UP_RIGHT,
    controller::Action::EYE_LOOK_UP_LEFT,
    controller::Action::EYE_LOOK_UP_RIGHT,
    controller::Action::EYE_LOOK_DOWN_LEFT,
    controller::Action::EYE_LOOK_DOWN_RIGHT,
    controller::Action::EYE_LOOK_IN_LEFT,
    controller::Action::EYE_LOOK_IN_RIGHT,
    controller::Action::EYE_LOOK_OUT_LEFT,
    controller::Action::EYE_LOOK_OUT_RIGHT,
    controller::Action::EYE_BLINK_LEFT,
    controller::Action::EYE_BLINK_RIGHT,
    controller::Action::EYE_SQUINT_LEFT,
    controller::Action::EYE_SQUINT_RIGHT,
    controller::Action::EYE_WIDE_LEFT,
    controller::Action::EYE_WIDE_RIGHT,
    controller::Action::CHEEK_PUFF,
    controller::Action::CHEEK_SQUINT_LEFT,
    controller::Action::CHEEK_SQUINT_RIGHT,
    controller::Action::NOSE_SNEER_LEFT,
    controller::Action::NOSE_SNEER_RIGHT,
    controller::Action::JAW_OPEN,
    controller::Action::JAW_FORWARD,
    controller::Action::JAW_LEFT,
    controller::Action::JAW_RIGHT,
    controller::Action::MOUTH_FUNNEL,
    controller::Action::MOUTH_PUCKER,
    controller::Action::MOUTH_LEFT,
    controller::Action::MOUTH_RIGHT,
    controller::Action::MOUTH_ROLL_UPPER,
    controller::Action::MOUTH_ROLL_LOWER,
    controller::Action::MOUTH_SHRUG_UPPER,
    controller::Action::MOUTH_SHRUG_LOWER,
    controller::Action::MOUTH_CLOSE,
    controller::Action::MOUTH_SMILE_LEFT,
    controller::Action::MOUTH_SMILE_RIGHT,
    controller::Action::MOUTH_FROWN_LEFT,
    controller::Action::MOUTH_FROWN_RIGHT,
    controller::Action::MOUTH_DIMPLE_LEFT,
    controller::Action::MOUTH_DIMPLE_RIGHT,
    controller::Action::MOUTH_UPPER_UP_LEFT,
    controller::Action::MOUTH_UPPER_UP_RIGHT,
    controller::Action::MOUTH_LOWER_DOWN_LEFT,
    controller::Action::MOUTH_LOWER_DOWN_RIGHT,
    controller::Action::MOUTH_PRESS_LEFT,
    controller::Action::MOUTH_PRESS_RIGHT,
    controller::Action::MOUTH_STRETCH_LEFT,
    controller::Action::MOUTH_STRETCH_RIGHT,
    controller::Action::TONGUE_OUT
};


MyHead::MyHead(MyAvatar* owningAvatar) : Head(owningAvatar) {
}

glm::quat MyHead::getHeadOrientation() const {
    // NOTE: Head::getHeadOrientation() is not used for orienting the camera "view" while in Oculus mode, so
    // you may wonder why this code is here. This method will be called while in Oculus mode to determine how
    // to change the driving direction while in Oculus mode. It is used to support driving toward where your
    // head is looking. Note that in oculus mode, your actual camera view and where your head is looking is not
    // always the same.

    MyAvatar* myAvatar = static_cast<MyAvatar*>(_owningAvatar);
    auto headPose = myAvatar->getControllerPoseInWorldFrame(controller::Action::HEAD);
    if (headPose.isValid()) {
        return headPose.rotation * Quaternions::Y_180;
    }

    return myAvatar->getWorldOrientation() * glm::quat(glm::radians(glm::vec3(_basePitch, 0.0f, 0.0f)));
}



void MyHead::simulate(float deltaTime) {
    auto player = DependencyManager::get<recording::Deck>();
    // Only use face trackers when not playing back a recording.
    if (!player->isPlaying()) {
        // TODO -- finish removing face-tracker specific code.  To do this, add input channels for
        // each blendshape-coefficient and update the various json files to relay them in a useful way.
        // After that, input plugins can be used to drive the avatar's face, and the various "DDE" files
        // can be ported into the plugin and removed.
        //
        // auto faceTracker = qApp->getActiveFaceTracker();
        // const bool hasActualFaceTrackerConnected = faceTracker && !faceTracker->isMuted();
        // _isFaceTrackerConnected = hasActualFaceTrackerConnected || _owningAvatar->getHasScriptedBlendshapes();
        // if (_isFaceTrackerConnected) {
        //     if (hasActualFaceTrackerConnected) {
        //         _blendshapeCoefficients = faceTracker->getBlendshapeCoefficients();
        //     }
        // }


        auto userInputMapper = DependencyManager::get<UserInputMapper>();

        // AJT: TODO probably want to split this into zones...
        bool faceTrackerConnected = false;
        for (int i = 0; i < BLENDSHAPE_COUNT; i++) {
            faceTrackerConnected |= userInputMapper->getActionStateValid(BLENDSHAPE_ACTIONS[i]);
        }

        // AJT: TODO HOOK UP ALL BLEND SHAPES HERE.

        setFaceTrackerConnected(faceTrackerConnected);

        if (faceTrackerConnected) {
            setHasProceduralEyeFaceMovement(false);
            setHasProceduralBlinkFaceMovement(false);
            setHasAudioEnabledFaceMovement(false);
            setHasProceduralEyeMovement(false);

            // AJT TODO: fix me in blendshape overhall
            // At the moment the input actions and blendshapes are not 1 to 1.
            // This is because the input actions follow the Apple ARKit standard set,
            // but the blendshapes still use a legacy Faceshift set.
            // Here we attempt to map between the two.

            _blendshapeCoefficients.resize(std::max(_blendshapeCoefficients.size(), (int)Faceshift::BlendshapeCount));

            // Eyes map directly
            _blendshapeCoefficients[(int)Faceshift::EyeBlink_L] = userInputMapper->getActionState(controller::Action::EYE_BLINK_LEFT);
            _blendshapeCoefficients[(int)Faceshift::EyeBlink_R] = userInputMapper->getActionState(controller::Action::EYE_BLINK_RIGHT);
            _blendshapeCoefficients[(int)Faceshift::EyeSquint_L] = userInputMapper->getActionState(controller::Action::EYE_SQUINT_LEFT);
            _blendshapeCoefficients[(int)Faceshift::EyeSquint_R] = userInputMapper->getActionState(controller::Action::EYE_SQUINT_RIGHT);
            _blendshapeCoefficients[(int)Faceshift::EyeDown_L] = userInputMapper->getActionState(controller::Action::EYE_LOOK_DOWN_LEFT);
            _blendshapeCoefficients[(int)Faceshift::EyeDown_R] = userInputMapper->getActionState(controller::Action::EYE_LOOK_DOWN_RIGHT);
            _blendshapeCoefficients[(int)Faceshift::EyeIn_L] = userInputMapper->getActionState(controller::Action::EYE_LOOK_IN_LEFT);
            _blendshapeCoefficients[(int)Faceshift::EyeIn_R] = userInputMapper->getActionState(controller::Action::EYE_LOOK_IN_RIGHT);
            _blendshapeCoefficients[(int)Faceshift::EyeOpen_L] = userInputMapper->getActionState(controller::Action::EYE_WIDE_LEFT);
            _blendshapeCoefficients[(int)Faceshift::EyeOpen_R] = userInputMapper->getActionState(controller::Action::EYE_WIDE_RIGHT);
            _blendshapeCoefficients[(int)Faceshift::EyeOut_L] = userInputMapper->getActionState(controller::Action::EYE_LOOK_OUT_LEFT);
            _blendshapeCoefficients[(int)Faceshift::EyeOut_R] = userInputMapper->getActionState(controller::Action::EYE_LOOK_OUT_RIGHT);
            _blendshapeCoefficients[(int)Faceshift::EyeUp_L] = userInputMapper->getActionState(controller::Action::EYE_LOOK_UP_LEFT);
            _blendshapeCoefficients[(int)Faceshift::EyeUp_R] = userInputMapper->getActionState(controller::Action::EYE_LOOK_UP_RIGHT);

            // Brows also map directly
            _blendshapeCoefficients[(int)Faceshift::BrowsD_L] = userInputMapper->getActionState(controller::Action::BROW_DOWN_LEFT);
            _blendshapeCoefficients[(int)Faceshift::BrowsD_R] = userInputMapper->getActionState(controller::Action::BROW_DOWN_RIGHT);
            _blendshapeCoefficients[(int)Faceshift::BrowsU_C] = userInputMapper->getActionState(controller::Action::BROW_INNER_UP);
            _blendshapeCoefficients[(int)Faceshift::BrowsU_L] = userInputMapper->getActionState(controller::Action::BROW_OUTER_UP_LEFT);
            _blendshapeCoefficients[(int)Faceshift::BrowsU_R] = userInputMapper->getActionState(controller::Action::BROW_OUTER_UP_RIGHT);

            // JawChew has no corresponding ARKit shape, so set it to zero
            // But all other Jaw values map directly.
            _blendshapeCoefficients[(int)Faceshift::JawFwd] = userInputMapper->getActionState(controller::Action::JAW_FORWARD);
            _blendshapeCoefficients[(int)Faceshift::JawLeft] = userInputMapper->getActionState(controller::Action::JAW_LEFT);
            _blendshapeCoefficients[(int)Faceshift::JawOpen] = userInputMapper->getActionState(controller::Action::JAW_OPEN);
            _blendshapeCoefficients[(int)Faceshift::JawChew] = 0.0f;
            _blendshapeCoefficients[(int)Faceshift::JawRight] = userInputMapper->getActionState(controller::Action::JAW_RIGHT);

            // MouthLeft, MouthRight
            _blendshapeCoefficients[(int)Faceshift::MouthLeft] = userInputMapper->getActionState(controller::Action::MOUTH_LEFT);
            _blendshapeCoefficients[(int)Faceshift::MouthRight] = userInputMapper->getActionState(controller::Action::MOUTH_RIGHT);

            // MouthFrown_L, MouthFrown_R
            _blendshapeCoefficients[(int)Faceshift::MouthFrown_L] = userInputMapper->getActionState(controller::Action::MOUTH_FROWN_LEFT);
            _blendshapeCoefficients[(int)Faceshift::MouthFrown_R] = userInputMapper->getActionState(controller::Action::MOUTH_FROWN_RIGHT);

            // MouthSmile_L, MouthSmile_R
            _blendshapeCoefficients[(int)Faceshift::MouthSmile_L] = userInputMapper->getActionState(controller::Action::MOUTH_SMILE_LEFT);
            _blendshapeCoefficients[(int)Faceshift::MouthSmile_R] = userInputMapper->getActionState(controller::Action::MOUTH_SMILE_RIGHT);

            // MouthDimple_L, MouthDimple_R
            _blendshapeCoefficients[(int)Faceshift::MouthDimple_L] = userInputMapper->getActionState(controller::Action::MOUTH_DIMPLE_LEFT);
            _blendshapeCoefficients[(int)Faceshift::MouthDimple_R] = userInputMapper->getActionState(controller::Action::MOUTH_DIMPLE_RIGHT);

            // LipsStretch_L, LipsStretch_R
            _blendshapeCoefficients[(int)Faceshift::LipsStretch_L] = userInputMapper->getActionState(controller::Action::MOUTH_STRETCH_LEFT);
            _blendshapeCoefficients[(int)Faceshift::LipsStretch_R] = userInputMapper->getActionState(controller::Action::MOUTH_STRETCH_RIGHT);

            // LipsUpperClose, LipsLowerClose
            _blendshapeCoefficients[(int)Faceshift::LipsUpperClose] = userInputMapper->getActionState(controller::Action::MOUTH_ROLL_UPPER);
            _blendshapeCoefficients[(int)Faceshift::LipsLowerClose] = userInputMapper->getActionState(controller::Action::MOUTH_ROLL_LOWER);

            // LipsUpperUp <- avg(MouthUpperUpLeft, MouthUpperUpRight)
            _blendshapeCoefficients[(int)Faceshift::LipsUpperUp] = (userInputMapper->getActionState(controller::Action::MOUTH_UPPER_UP_LEFT) +
                                                               userInputMapper->getActionState(controller::Action::MOUTH_UPPER_UP_RIGHT)) / 2.0f;

            // LipsLowerDown <- avg(MouthLowerDownLeft, MouthLowerDownRight)
            _blendshapeCoefficients[(int)Faceshift::LipsLowerDown] = (userInputMapper->getActionState(controller::Action::MOUTH_LOWER_DOWN_LEFT) +
                                                                 userInputMapper->getActionState(controller::Action::MOUTH_LOWER_DOWN_RIGHT)) / 2.0f;
            // LipsUpperOpen <- ???
            _blendshapeCoefficients[(int)Faceshift::LipsUpperOpen] = 0.0f;

            // LipsLowerOpen <- ???
            _blendshapeCoefficients[(int)Faceshift::LipsLowerOpen] = 0.0f;

            // LipsFunnel
            _blendshapeCoefficients[(int)Faceshift::LipsFunnel] = userInputMapper->getActionState(controller::Action::MOUTH_FUNNEL);

            // LipsPucker
            _blendshapeCoefficients[(int)Faceshift::LipsPucker] = userInputMapper->getActionState(controller::Action::MOUTH_PUCKER);

            // ChinLowerRaise <- ???
            _blendshapeCoefficients[(int)Faceshift::ChinLowerRaise] = 0.0f;

            // ChinUpperRaise <- ???
            _blendshapeCoefficients[(int)Faceshift::ChinUpperRaise] = 0.0f;

            // Sneer <- avg(NoseSneerLeft, NoseSneerRight)
            _blendshapeCoefficients[(int)Faceshift::Sneer] = (userInputMapper->getActionState(controller::Action::NOSE_SNEER_LEFT) +
                                                         userInputMapper->getActionState(controller::Action::NOSE_SNEER_RIGHT)) / 2.0f;

            // Puff
            _blendshapeCoefficients[(int)Faceshift::Puff] = userInputMapper->getActionState(controller::Action::CHEEK_PUFF);

            // CheekSquint_L
            _blendshapeCoefficients[(int)Faceshift::CheekSquint_L] = userInputMapper->getActionState(controller::Action::CHEEK_SQUINT_LEFT);

            // CheekSquint_R
            _blendshapeCoefficients[(int)Faceshift::CheekSquint_R] = userInputMapper->getActionState(controller::Action::CHEEK_SQUINT_RIGHT);

            // Unused ARKit shapes
            // ??? <- MouthShrugUpper
            // ??? <- MouthShrugLower
            // ??? <- MouthClose
            // ??? <- MouthPressLeft
            // ??? <- MouthPressRight
            // ??? <- TongueOut

        } else {
            const float FULLY_OPEN = 0.0f;
            _blendshapeCoefficients.resize(std::max(_blendshapeCoefficients.size(), 2));
            _blendshapeCoefficients[EYE_BLINK_INDICES[0]] = FULLY_OPEN;
            _blendshapeCoefficients[EYE_BLINK_INDICES[1]] = FULLY_OPEN;
        }
    }
    Parent::simulate(deltaTime);
}
