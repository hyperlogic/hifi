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
    CheekSquintRight,
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
    TongueOut,
    BlendshapeCount
};

// used to mirror left/right shapes from FaceCap.
FaceCap faceMirrorMap[(int)FaceCap::BlendshapeCount] = {
    FaceCap::BrowInnerUp,
    FaceCap::BrowDownRight,
    FaceCap::BrowDownLeft,
    FaceCap::BrowOuterUpRight,
    FaceCap::BrowOuterUpLeft,
    FaceCap::EyeLookUpRight,
    FaceCap::EyeLookUpLeft,
    FaceCap::EyeLookDownRight,
    FaceCap::EyeLookDownLeft,
    FaceCap::EyeLookInRight,
    FaceCap::EyeLookInLeft,
    FaceCap::EyeLookOutRight,
    FaceCap::EyeLookOutLeft,
    FaceCap::EyeBlinkRight,
    FaceCap::EyeBlinkLeft,
    FaceCap::EyeSquintRight,
    FaceCap::EyeSquintLeft,
    FaceCap::EyeWideRight,
    FaceCap::EyeWideLeft,
    FaceCap::CheekPuff,
    FaceCap::CheekSquintRight,
    FaceCap::CheekSquintLeft,
    FaceCap::NoseSneerRight,
    FaceCap::NoseSneerLeft,
    FaceCap::JawOpen,
    FaceCap::JawForward,
    FaceCap::JawRight,
    FaceCap::JawLeft,
    FaceCap::MouthFunnel,
    FaceCap::MouthPucker,
    FaceCap::MouthRight,
    FaceCap::MouthLeft,
    FaceCap::MouthRollUpper,
    FaceCap::MouthRollLower,
    FaceCap::MouthShrugUpper,
    FaceCap::MouthShrugLower,
    FaceCap::MouthClose,
    FaceCap::MouthSmileRight,
    FaceCap::MouthSmileLeft,
    FaceCap::MouthFrownRight,
    FaceCap::MouthFrownLeft,
    FaceCap::MouthDimpleRight,
    FaceCap::MouthDimpleLeft,
    FaceCap::MouthUpperUpRight,
    FaceCap::MouthUpperUpLeft,
    FaceCap::MouthLowerDownRight,
    FaceCap::MouthLowerDownLeft,
    FaceCap::MouthPressRight,
    FaceCap::MouthPressLeft,
    FaceCap::MouthStretchRight,
    FaceCap::MouthStretchLeft,
    FaceCap::TongueOut
};

static const char* STRINGS[FaceCap::BlendshapeCount] = {
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
    "CheekSquintRight",
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
    "TongueOut"
};

static enum controller::StandardAxisChannel CHANNELS[FaceCap::BlendshapeCount] = {
    controller::BROW_INNER_UP,
    controller::BROW_DOWN_LEFT,
    controller::BROW_DOWN_RIGHT,
    controller::BROW_OUTER_UP_LEFT,
    controller::BROW_OUTER_UP_RIGHT,
    controller::EYE_LOOK_UP_LEFT,
    controller::EYE_LOOK_UP_RIGHT,
    controller::EYE_LOOK_DOWN_LEFT,
    controller::EYE_LOOK_DOWN_RIGHT,
    controller::EYE_LOOK_IN_LEFT,
    controller::EYE_LOOK_IN_RIGHT,
    controller::EYE_LOOK_OUT_LEFT,
    controller::EYE_LOOK_OUT_RIGHT,
    controller::EYE_BLINK_LEFT,
    controller::EYE_BLINK_RIGHT,
    controller::EYE_SQUINT_LEFT,
    controller::EYE_SQUINT_RIGHT,
    controller::EYE_WIDE_LEFT,
    controller::EYE_WIDE_RIGHT,
    controller::CHEEK_PUFF,
    controller::CHEEK_SQUINT_LEFT,
    controller::CHEEK_SQUINT_RIGHT,
    controller::NOSE_SNEER_LEFT,
    controller::NOSE_SNEER_RIGHT,
    controller::JAW_OPEN,
    controller::JAW_FORWARD,
    controller::JAW_LEFT,
    controller::JAW_RIGHT,
    controller::MOUTH_FUNNEL,
    controller::MOUTH_PUCKER,
    controller::MOUTH_LEFT,
    controller::MOUTH_RIGHT,
    controller::MOUTH_ROLL_UPPER,
    controller::MOUTH_ROLL_LOWER,
    controller::MOUTH_SHRUG_UPPER,
    controller::MOUTH_SHRUG_LOWER,
    controller::MOUTH_CLOSE,
    controller::MOUTH_SMILE_LEFT,
    controller::MOUTH_SMILE_RIGHT,
    controller::MOUTH_FROWN_LEFT,
    controller::MOUTH_FROWN_RIGHT,
    controller::MOUTH_DIMPLE_LEFT,
    controller::MOUTH_DIMPLE_RIGHT,
    controller::MOUTH_UPPER_UP_LEFT,
    controller::MOUTH_UPPER_UP_RIGHT,
    controller::MOUTH_LOWER_DOWN_LEFT,
    controller::MOUTH_LOWER_DOWN_RIGHT,
    controller::MOUTH_PRESS_LEFT,
    controller::MOUTH_PRESS_RIGHT,
    controller::MOUTH_STRETCH_LEFT,
    controller::MOUTH_STRETCH_RIGHT,
    controller::TONGUE_OUT
};


void OscPlugin::init() {

    {
        std::lock_guard<std::mutex> guard(_dataMutex);
        _blendshapeValues.assign((int)FaceCap::BlendshapeCount, 0.0f);
        _blendshapeValidFlags.assign((int)FaceCap::BlendshapeCount, false);
        _headRot = glm::quat();
        _headRotValid = false;
        _headTransTarget = extractTranslation(_lastInputCalibrationData.defaultHeadMat);
        _headTransSmoothed = extractTranslation(_lastInputCalibrationData.defaultHeadMat);
        _headTransValid = false;
        _eyeLeftRot = glm::quat();
        _eyeLeftRotValid = false;
        _eyeRightRot = glm::quat();
        _eyeRightRotValid = false;
    }

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
                    restartServer();
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

    {
        static const int MIN_PORT_NUMBER { 0 };
        static const int MAX_PORT_NUMBER { 65535 };

        auto getter = [this]()->float { return (float)_serverPort; };
        auto setter = [this](float value) {
            _serverPort = (int)value;
            saveSettings();
            restartServer();
        };
        auto preference = new SpinnerPreference(OSC_PLUGIN, "Server Port", getter, setter);

        preference->setMin(MIN_PORT_NUMBER);
        preference->setMax(MAX_PORT_NUMBER);
        preference->setStep(1);
        preferences->addPreference(preference);
    }
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
    std::lock_guard<std::mutex> guard(container->_dataMutex);

    // Special case: decode blendshapes from face-cap iPhone app.
    // http://www.bannaflak.com/face-cap/livemode.html
    if (path[0] == '/' && path[1] == 'W' && argc == 2 && types[0] == 'i' && types[1] == 'f') {
        int index = argv[0]->i;
        if (index >= 0 && index < (int)FaceCap::BlendshapeCount) {
            int mirroredIndex = (int)faceMirrorMap[index];
            container->_blendshapeValues[mirroredIndex] = argv[1]->f;
            container->_blendshapeValidFlags[mirroredIndex] = true;
        }
    }

    // map /HT to head translation
    if (path[0] == '/' && path[1] == 'H' && path[2] == 'T' &&
        types[0] == 'f' && types[1] == 'f' && types[2] == 'f') {
        glm::vec3 trans(-argv[0]->f, -argv[1]->f, argv[2]->f); // in cm

        // convert trans into a delta (in meters) from the sweet spot of the iphone camera.
        const float CM_TO_METERS = 0.01f;
        const glm::vec3 FACE_CAP_HEAD_SWEET_SPOT(0.0f, 0.0f, -45.0f);
        glm::vec3 delta = (trans - FACE_CAP_HEAD_SWEET_SPOT) * CM_TO_METERS;

        container->_headTransTarget = extractTranslation(container->_lastInputCalibrationData.defaultHeadMat) + delta;
        container->_headTransValid = true;
    }

    // map /HR to head rotation
    if (path[0] == '/' && path[1] == 'H' && path[2] == 'R' && path[3] == 0 &&
        types[0] == 'f' && types[1] == 'f' && types[2] == 'f') {
        glm::vec3 euler(-argv[0]->f, -argv[1]->f, argv[2]->f);
        container->_headRot = glm::quat(glm::radians(euler)) * Quaternions::Y_180;
        container->_headRotValid = true;
    }

    // map /ELR to left eye rot
    if (path[0] == '/' && path[1] == 'E' && path[2] == 'L' && path[3] == 'R' &&
        types[0] == 'f' && types[1] == 'f' && types[2] == 'f') {
        glm::vec3 euler(-argv[0]->f, -argv[1]->f, argv[2]->f);
        container->_eyeLeftRot = glm::quat(glm::radians(euler)) * Quaternions::Y_180;
        container->_eyeLeftRotValid = true;
    }

    // map /ERR to right eye rot
    if (path[0] == '/' && path[1] == 'E' && path[2] == 'R' && path[3] == 'R' &&
        types[0] == 'f' && types[1] == 'f' && types[2] == 'f') {
        glm::vec3 euler(-argv[0]->f, -argv[1]->f, argv[2]->f);
        container->_eyeRightRot = glm::quat(glm::radians(euler)) * Quaternions::Y_180;
        container->_eyeRightRotValid = true;
    }

    // AJT: TODO map /STRINGS[i] to _blendshapeValues[i]

    if (container->_debug) {
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
    }

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

        return startServer();
    }
    return false;
}

void OscPlugin::deactivate() {
    qDebug(inputplugins) << "OscPlugin: deactivated, _oscServerThread =" << _oscServerThread;

    if (_oscServerThread) {
        stopServer();
    }
}

void OscPlugin::pluginUpdate(float deltaTime, const controller::InputCalibrationData& inputCalibrationData) {
    if (!_enabled) {
        return;
    }

    _lastInputCalibrationData = inputCalibrationData;

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
        settings.setValue(QString("serverPort"), _serverPort);
    }
    settings.endGroup();
}

void OscPlugin::loadSettings() {
    Settings settings;
    QString idString = getID();
    settings.beginGroup(idString);
    {
        _enabled = settings.value("enabled", QVariant(DEFAULT_ENABLED)).toBool();
        _debug = settings.value("extraDebug", QVariant(DEFAULT_ENABLED)).toBool();
        _serverPort = settings.value("serverPort", QVariant(DEFAULT_OSC_SERVER_PORT)).toInt();
    }
    settings.endGroup();
}

bool OscPlugin::startServer() {
    if (_oscServerThread) {
        qWarning(inputplugins) << "OscPlugin: (startServer) server is already running, _oscServerThread =" << _oscServerThread;
        return true;
    }

    // start a new server on specified port
    const size_t BUFFER_SIZE = 64;
    char serverPortString[BUFFER_SIZE];
    snprintf(serverPortString, BUFFER_SIZE, "%d", _serverPort);
    _oscServerThread = lo_server_thread_new(serverPortString, errorHandlerFunc);

    qDebug(inputplugins) << "OscPlugin: server started on port" << serverPortString << ", _oscServerThread =" << _oscServerThread;

    // add method that will match any path and args
    // NOTE: callback function will be called on the OSC thread, not the appliation thread.
    lo_server_thread_add_method(_oscServerThread, NULL, NULL, genericHandlerFunc, (void*)this);

    lo_server_thread_start(_oscServerThread);

    return true;
}

void OscPlugin::stopServer() {
    if (!_oscServerThread) {
        qWarning(inputplugins) << "OscPlugin: (stopServer) server is already shutdown.";
    }

    // stop and free server
    lo_server_thread_stop(_oscServerThread);
    lo_server_thread_free(_oscServerThread);
    _oscServerThread = nullptr;
}

void OscPlugin::restartServer() {
    if (_oscServerThread) {
        stopServer();
    }
    startServer();
}

//
// InputDevice
//

controller::Input::NamedVector OscPlugin::InputDevice::getAvailableInputs() const {
    static controller::Input::NamedVector availableInputs;
    if (availableInputs.size() == 0) {
        for (int i = 0; i < (int)FaceCap::BlendshapeCount; i++) {
            availableInputs.push_back(makePair(CHANNELS[i], STRINGS[i]));
        }
    }
    availableInputs.push_back(makePair(controller::HEAD, "Head"));
    availableInputs.push_back(makePair(controller::LEFT_EYE, "LeftEye"));
    availableInputs.push_back(makePair(controller::RIGHT_EYE, "RightEye"));
    return availableInputs;
}

QString OscPlugin::InputDevice::getDefaultMappingConfig() const {
    static const QString MAPPING_JSON = PathUtils::resourcesPath() + "/controllers/osc.json";
    return MAPPING_JSON;
}

void OscPlugin::InputDevice::update(float deltaTime, const controller::InputCalibrationData& inputCalibrationData) {
    std::lock_guard<std::mutex> guard(_container->_dataMutex);
    for (int i = 0; i < (int)FaceCap::BlendshapeCount; i++) {
        if (_container->_blendshapeValidFlags[i]) {
            _axisStateMap[CHANNELS[i]] = controller::AxisValue(_container->_blendshapeValues[i], 0, true);
        }
    }
    if (_container->_headRotValid && _container->_headTransValid) {

        const float SMOOTH_TIMESCALE = 2.0f;
        float tau = deltaTime / SMOOTH_TIMESCALE;
        _container->_headTransSmoothed = lerp(_container->_headTransSmoothed, _container->_headTransTarget, tau);
        glm::vec3 delta = _container->_headTransSmoothed - _container->_headTransTarget;
        glm::vec3 trans = extractTranslation(inputCalibrationData.defaultHeadMat) + delta;

        _poseStateMap[controller::HEAD] = controller::Pose(trans, _container->_headRot);
    }
    if (_container->_eyeLeftRotValid) {
        _poseStateMap[controller::LEFT_EYE] = controller::Pose(vec3(0.0f), _container->_eyeLeftRot);
    }
    if (_container->_eyeRightRotValid) {
        _poseStateMap[controller::RIGHT_EYE] = controller::Pose(vec3(0.0f), _container->_eyeRightRot);
    }
}

void OscPlugin::InputDevice::clearState() {
    std::lock_guard<std::mutex> guard(_container->_dataMutex);
    for (int i = 0; i < (int)FaceCap::BlendshapeCount; i++) {
        _axisStateMap[CHANNELS[i]] = controller::AxisValue(0.0f, 0, false);
    }
    _poseStateMap[controller::HEAD] = controller::Pose();
    _poseStateMap[controller::LEFT_EYE] = controller::Pose();
    _poseStateMap[controller::RIGHT_EYE] = controller::Pose();
}

