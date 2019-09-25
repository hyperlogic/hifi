//
//  FaceshiftConstants.h
//
//
//  Created by Clement on 1/23/15.
//  Copyright 2015 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_FaceshiftConstants_h
#define hifi_FaceshiftConstants_h

/// The names of the blendshapes expected by Faceshift, terminated with an empty string.
extern const char* FACESHIFT_BLENDSHAPES[];
/// The size of FACESHIFT_BLENDSHAPES
extern const int NUM_FACESHIFT_BLENDSHAPES;
// Eyes and Brows indices
extern const int EYE_BLINK_INDICES[];
extern const int EYE_OPEN_INDICES[];
extern const int BROWS_U_INDICES[];
extern const int EYE_SQUINT_INDICES[];

enum class Faceshift : int {
    EyeBlink_L = 0,
    EyeBlink_R,
    EyeSquint_L,
    EyeSquint_R,
    EyeDown_L,
    EyeDown_R,
    EyeIn_L,
    EyeIn_R,
    EyeOpen_L,
    EyeOpen_R,
    EyeOut_L,
    EyeOut_R,
    EyeUp_L,
    EyeUp_R,
    BrowsD_L,
    BrowsD_R,
    BrowsU_C,
    BrowsU_L,
    BrowsU_R,
    JawFwd,
    JawLeft,
    JawOpen,
    JawChew,
    JawRight,
    MouthLeft,
    MouthRight,
    MouthFrown_L,
    MouthFrown_R,
    MouthSmile_L,
    MouthSmile_R,
    MouthDimple_L,
    MouthDimple_R,
    LipsStretch_L,
    LipsStretch_R,
    LipsUpperClose,
    LipsLowerClose,
    LipsUpperUp,
    LipsLowerDown,
    LipsUpperOpen,
    LipsLowerOpen,
    LipsFunnel,
    LipsPucker,
    ChinLowerRaise,
    ChinUpperRaise,
    Sneer,
    Puff,
    CheekSquint_L,
    CheekSquint_R,
    BlendshapeCount,
};

#endif // hifi_FaceshiftConstants_h
