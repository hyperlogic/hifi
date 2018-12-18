//
//  Created by Dante Ruiz 2017/04/16
//  Copyright 2017 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "InputRecorder.h"

#include <QFile>
#include <QDir>
#include <QDirIterator>
#include <QStandardPaths>
#include <QDateTime>
#include <QByteArray>
#include <QStandardPaths>
#include <PathUtils.h>
#include <QUrl>
#include <Gzip.h>

#include <BuildInfo.h>
#include <GLMHelpers.h>
#include <DependencyManager.h>
#include "UserInputMapper.h"
#include <glm/gtc/matrix_transform.hpp>

// input recording json files are too large for Qt, https://bugreports.qt.io/browse/QTBUG-60728
// use this library instead.
#include <nlohmann/json.hpp>

QString SAVE_DIRECTORY = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/" + BuildInfo::MODIFIED_ORGANIZATION + "/" + BuildInfo::INTERFACE_NAME + "/hifi-input-recordings/";
QString FILE_PREFIX_NAME = "input-recording-";
QString COMPRESS_EXTENSION = ".json.gz";

// version "0.0" - original version.
QString ORIGINAL_VERSION_STRING = "0.0";
// version "0.1" - adds the inputCalibrationDataList, which saves the avatar position and sensorToWorldMatrix
QString INPUT_CALIBRATION_VERSION_STRING = "0.1";
QString CURRENT_VERSION_STRING = INPUT_CALIBRATION_VERSION_STRING;

namespace controller {

    nlohmann::json poseToJsonObject(const Pose pose) {
        nlohmann::json newPose;
        newPose["translation"] = {pose.translation.x, pose.translation.y, pose.translation.z};
        newPose["rotation"] = {pose.rotation.x, pose.rotation.y, pose.rotation.z, pose.rotation.w};
        newPose["velocity"] = {pose.velocity.x, pose.velocity.y, pose.velocity.z};
        newPose["angularVelocity"] = {pose.velocity.x, pose.velocity.y, pose.velocity.z};
        newPose["valid"] = pose.valid;
        return newPose;
    }

    Pose jsonObjectToPose(const nlohmann::json& json) {
        Pose pose;

        const nlohmann::json& translation = json["translation"];
        const nlohmann::json& rotation = json["rotation"];
        const nlohmann::json& velocity = json["velocity"];
        const nlohmann::json& angularVelocity = json["angularVelocity"];

        pose.valid = json["valid"].get<bool>();

        pose.translation.x = translation[0].get<double>();
        pose.translation.y = translation[1].get<double>();
        pose.translation.z = translation[2].get<double>();

        pose.rotation.x = rotation[0].get<double>();
        pose.rotation.y = rotation[1].get<double>();
        pose.rotation.z = rotation[2].get<double>();
        pose.rotation.w = rotation[3].get<double>();

        pose.velocity.x = velocity[0].get<double>();
        pose.velocity.y = velocity[1].get<double>();
        pose.velocity.z = velocity[2].get<double>();

        pose.angularVelocity.x = angularVelocity[0].get<double>();
        pose.angularVelocity.y = angularVelocity[1].get<double>();
        pose.angularVelocity.z = angularVelocity[2].get<double>();

        return pose;
    }

    nlohmann::json matrixToJsonObject(const glm::mat4& mat) {

        // same code from AnimPose constructor
        static const float EPSILON = 0.0001f;
        glm::vec3 scale = extractScale(mat);
        // quat_cast doesn't work so well with scaled matrices, so cancel it out.
        glm::mat4 tmp = glm::scale(mat, 1.0f / scale);
        glm::quat rot = glm::quat_cast(tmp);
        float lengthSquared = glm::length2(rot);
        if (glm::abs(lengthSquared - 1.0f) > EPSILON) {
            float oneOverLength = 1.0f / sqrtf(lengthSquared);
            rot = glm::quat(rot.w * oneOverLength, rot.x * oneOverLength, rot.y * oneOverLength, rot.z * oneOverLength);
        }
        glm::vec3 trans = extractTranslation(mat);

        nlohmann::json result;
        result["translation"] = {trans.x, trans.y, trans.z};
        result["rotation"] = {rot.x, rot.y, rot.z, rot.w};
        result["scale"] = {scale.x, scale.y, scale.z};
        return result;
    }

    nlohmann::json inputCalibrationDataToJsonObject(const InputCalibrationData& inputCalibrationData) {
        nlohmann::json result;

        result["sensorToWorld"] = matrixToJsonObject(inputCalibrationData.sensorToWorldMat);
        result["avatar"] = matrixToJsonObject(inputCalibrationData.avatarMat);
        result["hmdSensor"] = matrixToJsonObject(inputCalibrationData.hmdSensorMat);

        // I don't think there is much value in saving the other InputCalibrationData fields to the input recording.

        return result;
    }


    void exportToFile(const QByteArray& jsonData, const QString& fileName) {
        if (!QDir(SAVE_DIRECTORY).exists()) {
            QDir().mkdir(SAVE_DIRECTORY);
        }

        QFile saveFile (fileName);
        if (!saveFile.open(QIODevice::WriteOnly)) {
            qWarning() << "could not open file: " << fileName;
            return;
        }

        QByteArray jsonDataForFile;
        if (!gzip(jsonData, jsonDataForFile, -1)) {
            qCritical("unable to gzip while saving to json.");
            return;
        }

        saveFile.write(jsonDataForFile);
        saveFile.close();
    }

    nlohmann::json openFile(const QString& file, bool& status) {
        nlohmann::json object;
        QFile openFile(file);
        if (!openFile.open(QIODevice::ReadOnly)) {
            qWarning() << "could not open file: " << file;
            status = false;
            return object;
        }
        QByteArray compressedData = openFile.readAll();
        QByteArray jsonData;

        if (!gunzip(compressedData, jsonData)) {
            qCritical() << "json file not in gzip format: " << file;
            status = false;
            return object;
        }

        object = nlohmann::json::parse(jsonData.data());
        status = true;
        openFile.close();

        return object;
    }

    InputRecorder::InputRecorder() {}

    InputRecorder::~InputRecorder() {}

    InputRecorder* InputRecorder::getInstance() {
        static InputRecorder inputRecorder;
        return &inputRecorder;
    }

    QString InputRecorder::getSaveDirectory() {
        return SAVE_DIRECTORY;
    }

    void InputRecorder::startRecording() {
        LockGuard guard(_mutex);

        _recording = true;
        _playback = false;
        _framesRecorded = 0;
        _poseStateList.clear();
        _actionStateList.clear();
        _inputCalibrationDataList.clear();
    }

    QByteArray InputRecorder::recordDataToJson() {

        nlohmann::json json;
        json["frameCount"] = _framesRecorded;
        json["version"] = CURRENT_VERSION_STRING.toStdString();

        // actions
        nlohmann::json actionStateListJson = nlohmann::json::array();
        for(const auto& actionState: _actionStateList) {
            nlohmann::json actionStateJson = nlohmann::json::array();
            for (const auto& action: actionState) {
                nlohmann::json actionJson;
                actionJson["name"] = action.first.toStdString();
                actionJson["value"] = action.second;
                actionStateJson.push_back(actionJson);
            }
            actionStateListJson.push_back(actionStateJson);
        }
        json["actionList"] = actionStateListJson;

        // poses
        nlohmann::json poseStateListJson = nlohmann::json::array();
        for (const auto& poseState: _poseStateList) {
            nlohmann::json poseStateJson = nlohmann::json::array();
            for (const auto& pose: poseState) {
                nlohmann::json poseJson;
                poseJson["name"] = pose.first.toStdString();
                poseJson["pose"] = poseToJsonObject(pose.second);
                poseStateJson.push_back(poseJson);
            }
            poseStateListJson.push_back(poseStateJson);
        }
        json["poseList"] = poseStateListJson;

        // inputCalibrationData
        nlohmann::json inputCalibrationListJson = nlohmann::json::array();
        for (const auto& inputCalibrationData: _inputCalibrationDataList) {
            inputCalibrationListJson.push_back(inputCalibrationDataToJsonObject(inputCalibrationData));
        }
        json["inputCalibrationDataList"] = inputCalibrationListJson;

        // convert nlohmann::json to QByteArray
        std::string temp = json.dump(4);
        return QByteArray(temp.data());
    }

    void InputRecorder::saveRecording() {
        LockGuard guard(_mutex);

        QByteArray jsonData = recordDataToJson();
        QString timeStamp = QDateTime::currentDateTime().toString(Qt::ISODate);
        timeStamp.replace(":", "-");
        QString fileName = SAVE_DIRECTORY + FILE_PREFIX_NAME + timeStamp + COMPRESS_EXTENSION;
        exportToFile(jsonData, fileName);
    }

    void InputRecorder::loadRecording(const QString& path) {
        LockGuard guard(_mutex);

        _recording = false;
        _playback = false;
        _loading = true;
        _playCount = 0;
        resetFrame();
        _poseStateList.clear();
        _actionStateList.clear();
        _inputCalibrationDataList.clear();
        QUrl urlPath(path);
        QString filePath = urlPath.toLocalFile();
        QFileInfo info(filePath);
        QString extension = info.suffix();

        if (extension != "gz") {
            qWarning() << "can not load file with exentsion of " << extension;
            return;
        }

        bool success = false;
        nlohmann::json data = openFile(filePath, success);
        if (success) {
            QString version = QString::fromStdString(data["version"].get<std::string>());
            _framesRecorded = data["frameCount"].get<int>();

            // actions
            const nlohmann::json& actionArrayList = data["actionList"];
            for (int actionIndex = 0; actionIndex < actionArrayList.size(); actionIndex++) {
                const nlohmann::json& actionState = actionArrayList[actionIndex];
                for (int index = 0; index < actionState.size(); index++) {
                    const nlohmann::json& actionObject = actionState[index];
                    QString key = QString::fromStdString(actionObject["name"].get<std::string>());
                    _currentFrameActions[key] = actionObject["value"].get<double>();
                }
                _actionStateList.push_back(_currentFrameActions);
                _currentFrameActions.clear();
            }

            // poses
            const nlohmann::json& poseArrayList = data["poseList"];
            for (int poseIndex = 0; poseIndex < poseArrayList.size(); poseIndex++) {
                const nlohmann::json& poseState = poseArrayList[poseIndex];
                for (int index = 0; index < poseState.size(); index++) {
                    const nlohmann::json& poseObject = poseState[index];
                    QString key = QString::fromStdString(poseObject["name"].get<std::string>());
                    _currentFramePoses[key] = jsonObjectToPose(poseObject["pose"]);
                }
                _poseStateList.push_back(_currentFramePoses);
                _currentFramePoses.clear();
            }

            if (version == INPUT_CALIBRATION_VERSION_STRING) {
                // Currently, we don't do anything with the "inputCalibrationDataList" during playback, so don't bother reading it in.
            }
        }
        _loading = false;
    }

    void InputRecorder::stopRecording() {
        LockGuard guard(_mutex);

        _recording = false;
        _framesRecorded = (int)_actionStateList.size();
    }

    void InputRecorder::startPlayback() {
        LockGuard guard(_mutex);

        _playback = true;
        _recording = false;
        _playCount = 0;
    }

    void InputRecorder::stopPlayback() {
        LockGuard guard(_mutex);

        _playback = false;
        _playCount = 0;
    }

    void InputRecorder::setActionState(const QString& action, float value) {
        if (_recording) {
            _currentFrameActions[action] += value;
        }
    }

    void InputRecorder::setActionState(const QString& action, const controller::Pose& pose) {
        if (_recording) {
            _currentFramePoses[action] = pose;
        }
    }

    void InputRecorder::resetFrame() {
        LockGuard guard(_mutex);

        if (_recording) {
            _currentFramePoses.clear();
            _currentFrameActions.clear();
        }
    }

    float InputRecorder::getActionState(const QString& action) {
        if (_actionStateList.size() > 0 ) {
            return _actionStateList[_playCount][action];
        }

        return 0.0f;
    }

    controller::Pose InputRecorder::getPoseState(const QString& action) {
        if (_poseStateList.size() > 0) {
            return _poseStateList[_playCount][action];
        }

        return Pose();
    }

    void InputRecorder::frameTick(const InputCalibrationData& inputCalibrationData) {
        LockGuard guard(_mutex);

        if (_recording) {
            _framesRecorded++;
            _poseStateList.push_back(_currentFramePoses);
            _actionStateList.push_back(_currentFrameActions);
            _inputCalibrationDataList.push_back(inputCalibrationData);
        }

        if (_playback) {
            _playCount++;
            if (_playCount == (_framesRecorded - 1)) {
                _playCount = 0;
            }
        }
    }
}
