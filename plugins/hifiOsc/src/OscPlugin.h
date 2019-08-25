//
//  OscPlugin.h
//
//  Created by Anthony Thibault on 2019/8/24
//  Copyright 2019 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_OscPlugin_h
#define hifi_OscPlugin_h

#include <controllers/InputDevice.h>
#include <controllers/StandardControls.h>
#include <plugins/InputPlugin.h>

// OSC (Open Sound Control) input plugin.
class OscPlugin : public InputPlugin {
    Q_OBJECT
public:

    // Plugin functions
    virtual void init() override;
    virtual bool isSupported() const override;
    virtual const QString getName() const override { return NAME; }
    const QString getID() const override { return OSC_ID_STRING; }

    virtual bool activate() override;
    virtual void deactivate() override;

    virtual void pluginFocusOutEvent() override { _inputDevice->focusOutEvent(); }
    virtual void pluginUpdate(float deltaTime, const controller::InputCalibrationData& inputCalibrationData) override;

    virtual void saveSettings() const override;
    virtual void loadSettings() override;

protected:

    class InputDevice : public controller::InputDevice {
    public:
        friend class OscPlugin;

        InputDevice() : controller::InputDevice("OSC") {}

        // Device functions
        virtual controller::Input::NamedVector getAvailableInputs() const override;
        virtual QString getDefaultMappingConfig() const override;
        virtual void update(float deltaTime, const controller::InputCalibrationData& inputCalibrationData) override {};
        virtual void focusOutEvent() override {};

        /* AJT: TODO REMOVE
        void update(float deltaTime, const controller::InputCalibrationData& inputCalibrationData, 
                const std::vector<KinectPlugin::KinectJoint>& joints, const std::vector<KinectPlugin::KinectJoint>& prevJoints);
        */

        void clearState();
    };

    std::shared_ptr<InputDevice> _inputDevice { std::make_shared<InputDevice>() };

    static const char* NAME;
    static const char* OSC_ID_STRING;

    bool _enabled { false };
    bool _debug { false };
    mutable bool _initialized { false };
};

#endif // hifi_OscPlugin_h

