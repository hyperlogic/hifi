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

    OscPlugin* container = reinterpret_cast<OscPlugin*>(user_data);
    assert(container);

    QString key(path);
    std::lock_guard<std::mutex> guard(container->_oscMutex);

    // look up _oscValues index in map
    int index = -1;
    auto iter = container->_oscStringToValueMap.find(key);
    if (iter != container->_oscStringToValueMap.end()) {
        // found it
        index = iter->second;
    } else {
        // did not find it, add a new element ot the _oscValues array.
        index = (int)container->_oscValues.size();
        container->_oscValues.push_back(0.0f);

        // and add the index to the map
        container->_oscStringToValueMap[key] = index;
    }

    if (argc > 0) {
        switch (types[0]) {
        case 'i':
            container->_oscValues[index] = (float)argv[0]->i;
            break;
        case 'f':
            container->_oscValues[index] = argv[0]->f;
            break;
        case 'h':
            container->_oscValues[index] = (float)argv[0]->h;
            break;
        case 'd':
            container->_oscValues[index] = (float)argv[0]->d;
            break;
        }
    }

#ifdef DUMP_ALL_OSC_VALUES
    for (int i = 0; i < argc; i++) {
        switch (types[i]) {
        case 'i':
            // int32
            qDebug() << "OscPlugin: " << path << "=" << argv[i]->i;
            break;
        case 'f':
            // float32
            qDebug() << "OscPlugin: " << path << "=" << argv[i]->f32;
            break;
        case 's':
            // OSC-string
            qDebug() << "OscPlugin: " << path << "= <string>";
            break;
        case 'b':
            // OSC-blob
            break;
        case 'h':
            // 64 bit big-endian two's complement integer
            qDebug() << "OscPlugin: " << path << "=" << argv[i]->h;
            break;
        case 't':
            // OSC-timetag
            qDebug() << "OscPlugin: " << path << "= <OSC-timetag>";
            break;
        case 'd':
            // 64 bit ("double") IEEE 754 floating point number
            qDebug() << "OscPlugin: " << path << "=" << argv[i]->d;
            break;
        case 'S':
            // Alternate type represented as an OSC-string (for example, for systems that differentiate "symbols" from "strings")
            qDebug() << "OscPlugin: " << path << "= <OSC-symbol>";
            break;
        case 'c':
            // an ascii character, sent as 32 bits
            qDebug() << "OscPlugin: " << path << "=" << argv[i]->c;
            break;
        case 'r':
            // 32 bit RGBA color
            qDebug() << "OscPlugin: " << path << "= <color>";
            break;
        case 'm':
            // 4 byte MIDI message. Bytes from MSB to LSB are: port id, status byte, data1, data2
            qDebug() << "OscPlugin: " << path << "= <midi>";
            break;
        case 'T':
            // true
            qDebug() << "OscPlugin: " << path << "= <true>";
            break;
        case 'F':
            // false
            qDebug() << "OscPlugin: " << path << "= <false>";
            break;
        case 'N':
            // nil
            qDebug() << "OscPlugin: " << path << "= <nil>";
            break;
        case 'I':
            // inf
            qDebug() << "OscPlugin: " << path << "= <inf>";
            break;
        case '[':
            // Indicates the beginning of an array. The tags following are for data in the Array until a close brace tag is reached.
            qDebug() << "OscPlugin: " << path << "= <begin-array>";
            break;
        case ']':
            // Indicates the end of an array.
            qDebug() << "OscPlugin: " << path << "= <end-array>";
            break;
        default:
            qDebug() << "OscPlugin: " << path << "= <unknown-type>" << types[i];
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

        qDebug() << "OscPlugin: activated";

        _inputDevice->setContainer(this);

        // register with userInputMapper
        auto userInputMapper = DependencyManager::get<controller::UserInputMapper>();
        userInputMapper->registerDevice(_inputDevice);

        // start a new server on port 7770
        _oscServerThread = lo_server_thread_new("7770", errorHandlerFunc);

        qDebug() << "OscPlugin: server started on port 7770, _oscServerThread =" << _oscServerThread;

        // add method that will match any path and args
        // NOTE: callback function will be called on the OSC thread, not the appliation thread.
        lo_server_thread_add_method(_oscServerThread, NULL, NULL, genericHandlerFunc, (void*)this);

        lo_server_thread_start(_oscServerThread);

        return true;
    }
    return false;
}

void OscPlugin::deactivate() {
    qDebug() << "OscPlugin: deactivated, _oscServerThread =" << _oscServerThread;

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

    std::lock_guard<std::mutex> guard(_container->_oscMutex);
    if (availableInputs.size() != _container->_oscValues.size()) {
        availableInputs.clear();
        for (auto&& iter : _container->_oscStringToValueMap) {
            // AJT: TODO: is it ok to use actions past the end?
            int action = (int)controller::Action::NUM_ACTIONS + iter.second;
            controller::Input input = makeInput((controller::StandardAxisChannel)action);
            availableInputs.push_back(controller::Input::NamedPair(input, iter.first));
        }
    }

    return availableInputs;
}

QString OscPlugin::InputDevice::getDefaultMappingConfig() const {
    static const QString MAPPING_JSON = PathUtils::resourcesPath() + "/controllers/osc.json";
    return MAPPING_JSON;
}

void OscPlugin::InputDevice::update(float deltaTime, const controller::InputCalibrationData& inputCalibrationData) {
    std::lock_guard<std::mutex> guard(_container->_oscMutex);
    for (auto&& iter : _container->_oscStringToValueMap) {
        float value = _container->_oscValues[iter.second];
        _axisStateMap[iter.second] = controller::AxisValue(value, 0, false);
        qDebug() << "AJT: " << iter.first << ", action =" << iter.second << ", value =" << value;
    }
}

void OscPlugin::InputDevice::clearState() {
    // set all axes to invalid.
    std::lock_guard<std::mutex> guard(_container->_oscMutex);
    for (auto&& iter : _container->_oscStringToValueMap) {
        _axisStateMap[iter.second] = controller::AxisValue(0.0f, 0, false);
    }
}

