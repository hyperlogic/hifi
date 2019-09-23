//
//  OscPlugin.cpp
//
//  Created by Anthony Thibault on 2019/8/24
//  Copyright 2019 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "OscPlugin.h"

#include <controllers/UserInputMapper.h>
#include <QLoggingCategory>
#include <PathUtils.h>
#include <DebugDraw.h>
#include <cassert>
#include <NumericalConstants.h>
#include <StreamUtils.h>
#include <Preferences.h>
#include <SettingHandle.h>

Q_DECLARE_LOGGING_CATEGORY(inputplugins)
Q_LOGGING_CATEGORY(inputplugins, "hifi.inputplugins")

//#define DUMP_ALL_OSC_VALUES 1

const char* OscPlugin::NAME = "Open Sound Control (OSC)";
const char* OscPlugin::OSC_ID_STRING = "Open Sound Control (OSC)";
const bool DEFAULT_ENABLED = false;

enum class FaceCap {
    BrowInnerUp = 0,
    BrowDownLeft,
    BrowDownRight,
    BrowOuterUpLeft,
    BrowOuterUpRight,
    EyeLookUpLeft,
    EyeLookUpRight,
    EyeLookDownLeft,
    EyeLookDownRight,
    EyeLookInLeft,
    EyeLookInRight,
    EyeLookOutLeft,
    EyeLookOutRight,
    EyeBlinkLeft,
    EyeBlinkRight,
    EyeSquintLeft,
    EyeSquintRight,
    EyeWideLeft,
    EyeWideRight,
    CheekPuff,
    CheekSquintLeft,
    CheckSquintRight,
    NoseSneerLeft,
    NoseSneerRight,
    JawOpen,
    JawForward,
    JawLeft,
    JawRight,
    MouthFunnel,
    MouthPucker,
    MouthLeft,
    MouthRight,
    MouthRollUpper,
    MouthRollLower,
    MouthShrugUpper,
    MouthShrugLower,
    MouthClose,
    MouthSmileLeft,
    MouthSmileRight,
    MouthFrownLeft,
    MouthFrownRight,
    MouthDimpleLeft,
    MouthDimpleRight,
    MouthUpperUpLeft,
    MouthUpperUpRight,
    MouthLowerDownLeft,
    MouthLowerDownRight,
    MouthPressLeft,
    MouthPressRight,
    MouthStretchLeft,
    MouthStretchRight,
    ToungueOut,
    BlendshapeCount
};

/*
static const char* FaceCapEnumToString[FaceCap::BlendshapeCount] = {
    "BrowInnerUp",
    "BrowDownLeft",
    "BrowDownRight",
    "BrowOuterUpLeft",
    "BrowOuterUpRight",
    "EyeLookUpLeft",
    "EyeLookUpRight",
    "EyeLookDownLeft",
    "EyeLookDownRight",
    "EyeLookInLeft",
    "EyeLookInRight",
    "EyeLookOutLeft",
    "EyeLookOutRight",
    "EyeBlinkLeft",
    "EyeBlinkRight",
    "EyeSquintLeft",
    "EyeSquintRight",
    "EyeWideLeft",
    "EyeWideRight",
    "CheekPuff",
    "CheekSquintLeft",
    "CheckSquintRight",
    "NoseSneerLeft",
    "NoseSneerRight",
    "JawOpen",
    "JawForward",
    "JawLeft",
    "JawRight",
    "MouthFunnel",
    "MouthPucker",
    "MouthLeft",
    "MouthRight",
    "MouthRollUpper",
    "MouthRollLower",
    "MouthShrugUpper",
    "MouthShrugLower",
    "MouthClose",
    "MouthSmileLeft",
    "MouthSmileRight",
    "MouthFrownLeft",
    "MouthFrownRight",
    "MouthDimpleLeft",
    "MouthDimpleRight",
    "MouthUpperUpLeft",
    "MouthUpperUpRight",
    "MouthLowerDownLeft",
    "MouthLowerDownRight",
    "MouthPressLeft",
    "MouthPressRight",
    "MouthStretchLeft",
    "MouthStretchRight",
    "ToungueOut"
};
*/

void OscPlugin::init() {

    _blendshapeValues.assign((int)FaceCap::BlendshapeCount, 0.0f);
    _blendshapeValidFlags.assign((int)FaceCap::BlendshapeCount, false);

    loadSettings();

    auto preferences = DependencyManager::get<Preferences>();
    static const QString OSC_PLUGIN { OscPlugin::NAME };
    {
        auto getter = [this]()->bool { return _enabled; };
        auto setter = [this](bool value) {
            _enabled = value;
            saveSettings();
            if (!_enabled) {
                auto userInputMapper = DependencyManager::get<controller::UserInputMapper>();
                userInputMapper->withLock([&, this]() {
                    _inputDevice->clearState();
                });
            }
        };
        auto preference = new CheckPreference(OSC_PLUGIN, "Enabled", getter, setter);
        preferences->addPreference(preference);
    }
    {
        auto debugGetter = [this]()->bool { return _debug; };
        auto debugSetter = [this](bool value) {
            _debug = value;
            saveSettings();
        };
        auto preference = new CheckPreference(OSC_PLUGIN, "Extra Debugging", debugGetter, debugSetter);
        preferences->addPreference(preference);
    }

    // AJT: TODO network settings
}

bool OscPlugin::isSupported() const {
    // networking/UDP is pretty much always available...
    return true;
}

static void errorHandlerFunc(int num, const char* msg, const char* path) {
    qDebug(inputplugins) << "OscPlugin: server error" << num << "in path" << path << ":" << msg;
}

static int genericHandlerFunc(const char* path, const char* types, lo_arg** argv,
                              int argc, void* data, void* user_data) {

    OscPlugin* container = reinterpret_cast<OscPlugin*>(user_data);
    assert(container);

    QString key(path);
    std::lock_guard<std::mutex> guard(container->_blendshapeMutex);

    // Special case: decode blendshapes from face-cap iPhone app.
    // http://www.bannaflak.com/face-cap/
    if (argc == 2 && types[0] == 'i' && types[1] == 'f' && path[0] == '/' && path[1] == 'W') {
        int index = argv[0]->i;
        if (index >= 0 && index < (int)FaceCap::BlendshapeCount) {
            container->_blendshapeValues[index] = argv[1]->f;
            container->_blendshapeValidFlags[index] = true;
        }
    }

#ifdef DUMP_ALL_OSC_VALUES
    for (int i = 0; i < argc; i++) {
        switch (types[i]) {
        case 'i':
            // int32
            qDebug(inputplugins) << "OscPlugin: " << path << "[" << i << "] =" << argv[i]->i;
            break;
        case 'f':
            // float32
            qDebug(inputplugins) << "OscPlugin: " << path << "[" << i << "] =" << argv[i]->f32;
            break;
        case 's':
            // OSC-string
            qDebug(inputplugins) << "OscPlugin: " << path << "[" << i << "] = <string>";
            break;
        case 'b':
            // OSC-blob
            break;
        case 'h':
            // 64 bit big-endian two's complement integer
            qDebug(inputplugins) << "OscPlugin: " << path << "[" << i << "] =" << argv[i]->h;
            break;
        case 't':
            // OSC-timetag
            qDebug(inputplugins) << "OscPlugin: " << path << "[" << i << "] = <OSC-timetag>";
            break;
        case 'd':
            // 64 bit ("double") IEEE 754 floating point number
            qDebug(inputplugins) << "OscPlugin: " << path << "[" << i << "] =" << argv[i]->d;
            break;
        case 'S':
            // Alternate type represented as an OSC-string (for example, for systems that differentiate "symbols" from "strings")
            qDebug(inputplugins) << "OscPlugin: " << path << "[" << i << "] = <OSC-symbol>";
            break;
        case 'c':
            // an ascii character, sent as 32 bits
            qDebug(inputplugins) << "OscPlugin: " << path << "[" << i << "] =" << argv[i]->c;
            break;
        case 'r':
            // 32 bit RGBA color
            qDebug(inputplugins) << "OscPlugin: " << path << "[" << i << "] = <color>";
            break;
        case 'm':
            // 4 byte MIDI message. Bytes from MSB to LSB are: port id, status byte, data1, data2
            qDebug(inputplugins) << "OscPlugin: " << path << "[" << i << "] = <midi>";
            break;
        case 'T':
            // true
            qDebug(inputplugins) << "OscPlugin: " << path << "[" << i << "] = <true>";
            break;
        case 'F':
            // false
            qDebug(inputplugins) << "OscPlugin: " << path << "[" << i << "] = <false>";
            break;
        case 'N':
            // nil
            qDebug(inputplugins) << "OscPlugin: " << path << "[" << i << "] = <nil>";
            break;
        case 'I':
            // inf
            qDebug(inputplugins) << "OscPlugin: " << path << "[" << i << "] = <inf>";
            break;
        case '[':
            // Indicates the beginning of an array. The tags following are for data in the Array until a close brace tag is reached.
            qDebug(inputplugins) << "OscPlugin: " << path << "[" << i << "] = <begin-array>";
            break;
        case ']':
            // Indicates the end of an array.
            qDebug(inputplugins) << "OscPlugin: " << path << "[" << i << "] = <end-array>";
            break;
        default:
            qDebug(inputplugins) << "OscPlugin: " << path << "[" << i << "] = <unknown-type>" << types[i];
            break;
        }
    }
#endif

    return 1;
}


bool OscPlugin::activate() {
    InputPlugin::activate();

    loadSettings();

    if (_enabled) {

        qDebug(inputplugins) << "OscPlugin: activated";

        _inputDevice->setContainer(this);

        // register with userInputMapper
        auto userInputMapper = DependencyManager::get<controller::UserInputMapper>();
        userInputMapper->registerDevice(_inputDevice);

        // start a new server on port 7770
        _oscServerThread = lo_server_thread_new("7770", errorHandlerFunc);

        qDebug(inputplugins) << "OscPlugin: server started on port 7770, _oscServerThread =" << _oscServerThread;

        // add method that will match any path and args
        // NOTE: callback function will be called on the OSC thread, not the appliation thread.
        lo_server_thread_add_method(_oscServerThread, NULL, NULL, genericHandlerFunc, (void*)this);

        lo_server_thread_start(_oscServerThread);

        return true;
    }
    return false;
}

void OscPlugin::deactivate() {
    qDebug(inputplugins) << "OscPlugin: deactivated, _oscServerThread =" << _oscServerThread;

    if (_oscServerThread) {
        // stop and free server
        lo_server_thread_stop(_oscServerThread);
        lo_server_thread_free(_oscServerThread);
        _oscServerThread = nullptr;
    }
}

void OscPlugin::pluginUpdate(float deltaTime, const controller::InputCalibrationData& inputCalibrationData) {
    if (!_enabled) {
        return;
    }

    auto userInputMapper = DependencyManager::get<controller::UserInputMapper>();
    userInputMapper->withLock([&, this]() {
        _inputDevice->update(deltaTime, inputCalibrationData);
    });
}

void OscPlugin::saveSettings() const {
    Settings settings;
    QString idString = getID();
    settings.beginGroup(idString);
    {
        settings.setValue(QString("enabled"), _enabled);
        settings.setValue(QString("extraDebug"), _debug);
    }
    settings.endGroup();
    // AJT: TODO network settings
}

void OscPlugin::loadSettings() {
    Settings settings;
    QString idString = getID();
    settings.beginGroup(idString);
    {
        _enabled = settings.value("enabled", QVariant(DEFAULT_ENABLED)).toBool();
        _debug = settings.value("extraDebug", QVariant(DEFAULT_ENABLED)).toBool();
    }
    settings.endGroup();
    // AJT: TODO network settings
}

//
// InputDevice
//

controller::Input::NamedVector OscPlugin::InputDevice::getAvailableInputs() const {
    static controller::Input::NamedVector availableInputs;
    for (int i = 0; i < (int)FaceCap::BlendshapeCount; i++) {
        if (i == (int)FaceCap::EyeBlinkLeft) {
            availableInputs.push_back(makePair(controller::EYE_BLINK_LEFT, "EyeBlinkLeft"));
        } else if (i == (int)FaceCap::EyeBlinkRight) {
            availableInputs.push_back(makePair(controller::EYE_BLINK_RIGHT, "EyeBlinkRight"));
        }
    }

    return availableInputs;
}

QString OscPlugin::InputDevice::getDefaultMappingConfig() const {
    static const QString MAPPING_JSON = PathUtils::resourcesPath() + "/controllers/osc.json";
    return MAPPING_JSON;
}

void OscPlugin::InputDevice::update(float deltaTime, const controller::InputCalibrationData& inputCalibrationData) {
    std::lock_guard<std::mutex> guard(_container->_blendshapeMutex);
    for (int i = 0; i < (int)FaceCap::BlendshapeCount; i++) {
        if (_container->_blendshapeValidFlags[i]) {
            if (i == (int)FaceCap::EyeBlinkLeft) {
                _axisStateMap[controller::EYE_BLINK_LEFT] = controller::AxisValue(_container->_blendshapeValues[i], 0, true);
            } else if (i == (int)FaceCap::EyeBlinkRight) {
                _axisStateMap[controller::EYE_BLINK_RIGHT] = controller::AxisValue(_container->_blendshapeValues[i], 0, true);
            }
        }
    }
}

void OscPlugin::InputDevice::clearState() {
    _axisStateMap[controller::EYE_BLINK_LEFT] = controller::AxisValue(0.0f, 0, false);
    _axisStateMap[controller::EYE_BLINK_RIGHT] = controller::AxisValue(0.0f, 0, false);
}

