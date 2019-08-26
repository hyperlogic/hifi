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

void OscPlugin::init() {
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
    qDebug() << "OscPlugin: server error" << num << "in path" << path << ":" << msg;
}

static int genericHandlerFunc(const char* path, const char* types, lo_arg** argv,
                              int argc, void* data, void* user_data) {
    int i;

    qDebug() << "AJT: path:" << path;
    for (i = 0; i < argc; i++) {
        qDebug() << "AJT:     arg" << i << types[i];
        lo_arg_pp((lo_type)types[i], argv[i]);
    }

    return 1;
}


static int volumeHandlerFunc(const char* path, const char* types, lo_arg** argv,
                             int argc, void* data, void* user_data) {
    // example showing pulling the argument values out of the argv array
    qDebug() << "AJT:" << path << ": " << argv[0]->f;

    return 0;
}


bool OscPlugin::activate() {
    InputPlugin::activate();

    loadSettings();

    if (_enabled) {

        qDebug() << "OscPlugin: activated";

        // register with userInputMapper
        auto userInputMapper = DependencyManager::get<controller::UserInputMapper>();
        userInputMapper->registerDevice(_inputDevice);

        // start a new server on port 7770
        _st = lo_server_thread_new("7770", errorHandlerFunc);

        qDebug() << "AJT: lo_sever_thread_new(7700) =" << _st;

        // add method that will match any path and args
        lo_server_thread_add_method(_st, NULL, NULL, genericHandlerFunc, NULL);

        // volume from oscTouch app
        lo_server_thread_add_method(_st, "/1/volume", "f", volumeHandlerFunc, NULL);

        lo_server_thread_start(_st);

#if 0
    /* add method that will match the path /foo/bar, with two numbers, coerced
     * to float and int */
    lo_server_thread_add_method(st, "/foo/bar", "fi", foo_handler, NULL);

    /* add method that will match the path /blobtest with one blob arg */
    lo_server_thread_add_method(st, "/blobtest", "b", blobtest_handler, NULL);

    /* add method that will match the path /quit with no args */
    lo_server_thread_add_method(st, "/quit", "", quit_handler, NULL);

#endif


        return true;
    }
    return false;
}

void OscPlugin::deactivate() {

    qDebug() << "OscPlugin: deactivated";

    // stop and free server
    lo_server_thread_stop(_st);
    lo_server_thread_free(_st);
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

// FIXME - we probably should only return the inputs we ACTUALLY have
controller::Input::NamedVector OscPlugin::InputDevice::getAvailableInputs() const {
    static controller::Input::NamedVector availableInputs;

    // AJT: TODO, HMM, how to know before hand what inputs are available?
    /*
    if (availableInputs.size() == 0) {
        for (int i = 0; i < KinectJointIndex::Size; i++) {
            auto channel = KinectJointIndexToPoseIndex(static_cast<KinectJointIndex>(i));
            availableInputs.push_back(makePair(channel, getControllerJointName(channel)));
        }
    };
    */

    return availableInputs;
}

QString OscPlugin::InputDevice::getDefaultMappingConfig() const {
    static const QString MAPPING_JSON = PathUtils::resourcesPath() + "/controllers/osc.json";
    return MAPPING_JSON;
}

void OscPlugin::InputDevice::clearState() {

    /*
    for (size_t i = 0; i < KinectJointIndex::Size; i++) {
        int poseIndex = KinectJointIndexToPoseIndex((KinectJointIndex)i);
        _poseStateMap[poseIndex] = controller::Pose();
    }
    */
}
