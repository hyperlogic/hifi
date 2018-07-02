//
//  Created by Dante Ruiz 2017/04/16
//  Copyright 2017 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "InputRecorder.h"

#include <QJsonArray>
#include <QJsonDocument>
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

QString SAVE_DIRECTORY = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/" + BuildInfo::MODIFIED_ORGANIZATION + "/" + BuildInfo::INTERFACE_NAME + "/hifi-input-recordings/";
QString FILE_PREFIX_NAME = "input-recording-";
QString COMPRESS_EXTENSION = ".json.gz";

// version "0.0" - original version.
QString ORIGINAL_VERSION_STRING = "0.0";
// version "0.1" - adds the inputCalibrationDataList, which saves the avatar position and sensorToWorldMatrix
QString INPUT_CALIBRATION_VERSION_STRING = "0.1";
QString CURRENT_VERSION_STRING = INPUT_CALIBRATION_VERSION_STRING;

namespace controller {

    QJsonObject poseToJsonObject(const Pose pose) {
        QJsonObject newPose;

        QJsonArray translation;
        translation.append(pose.translation.x);
        translation.append(pose.translation.y);
        translation.append(pose.translation.z);

        QJsonArray rotation;
        rotation.append(pose.rotation.x);
        rotation.append(pose.rotation.y);
        rotation.append(pose.rotation.z);
        rotation.append(pose.rotation.w);

        QJsonArray velocity;
        velocity.append(pose.velocity.x);
        velocity.append(pose.velocity.y);
        velocity.append(pose.velocity.z);

        QJsonArray angularVelocity;
        angularVelocity.append(pose.angularVelocity.x);
        angularVelocity.append(pose.angularVelocity.y);
        angularVelocity.append(pose.angularVelocity.z);

        newPose["translation"] = translation;
        newPose["rotation"] = rotation;
        newPose["velocity"] = velocity;
        newPose["angularVelocity"] = angularVelocity;
        newPose["valid"] = pose.valid;

        return newPose;
    }

    Pose jsonObjectToPose(const QJsonObject object) {
        Pose pose;
        QJsonArray translation = object["translation"].toArray();
        QJsonArray rotation = object["rotation"].toArray();
        QJsonArray velocity = object["velocity"].toArray();
        QJsonArray angularVelocity = object["angularVelocity"].toArray();

        pose.valid = object["valid"].toBool();

        pose.translation.x = translation[0].toDouble();
        pose.translation.y = translation[1].toDouble();
        pose.translation.z = translation[2].toDouble();

        pose.rotation.x = rotation[0].toDouble();
        pose.rotation.y = rotation[1].toDouble();
        pose.rotation.z = rotation[2].toDouble();
        pose.rotation.w = rotation[3].toDouble();

        pose.velocity.x = velocity[0].toDouble();
        pose.velocity.y = velocity[1].toDouble();
        pose.velocity.z = velocity[2].toDouble();

        pose.angularVelocity.x = angularVelocity[0].toDouble();
        pose.angularVelocity.y = angularVelocity[1].toDouble();
        pose.angularVelocity.z = angularVelocity[2].toDouble();

        return pose;
    }

    QJsonObject matrixToJsonObject(const glm::mat4& mat) {

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

        QJsonObject result;

        QJsonArray transObj;
        transObj.append(trans.x);
        transObj.append(trans.y);
        transObj.append(trans.z);

        QJsonArray rotObj;
        rotObj.append(rot.x);
        rotObj.append(rot.y);
        rotObj.append(rot.z);
        rotObj.append(rot.w);

        QJsonArray scaleObj;
        scaleObj.append(scale.x);
        scaleObj.append(scale.y);
        scaleObj.append(scale.z);

        result["translation"] = transObj;
        result["rotation"] = rotObj;
        result["scale"] = scaleObj;

        return result;
    }

    QJsonObject inputCalibrationDataToJsonObject(const InputCalibrationData& inputCalibrationData) {
        QJsonObject result;

        result["sensorToWorld"] = matrixToJsonObject(inputCalibrationData.sensorToWorldMat);
        result["avatar"] = matrixToJsonObject(inputCalibrationData.avatarMat);
        result["hmdSensor"] = matrixToJsonObject(inputCalibrationData.hmdSensorMat);

        // I don't think there is much value in saving the other InputCalibrationData fields to the input recording.

        return result;
    }


    void exportToFile(const QJsonObject& object, const QString& fileName) {
        if (!QDir(SAVE_DIRECTORY).exists()) {
            QDir().mkdir(SAVE_DIRECTORY);
        }

        QFile saveFile (fileName);
        if (!saveFile.open(QIODevice::WriteOnly)) {
            qWarning() << "could not open file: " << fileName;
            return;
        }
        QJsonDocument saveData(object);
        QByteArray jsonData = saveData.toJson(QJsonDocument::Indented);
        QByteArray jsonDataForFile;

        if (!gzip(jsonData, jsonDataForFile, -1)) {
            qCritical("unable to gzip while saving to json.");
            return;
        }

        saveFile.write(jsonDataForFile);
        saveFile.close();
    }

    QJsonObject openFile(const QString& file, bool& status) {
        QJsonObject object;
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

        QJsonDocument jsonDoc = QJsonDocument::fromJson(jsonData);
        object = jsonDoc.object();
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
        _recording = true;
        _playback = false;
        _framesRecorded = 0;
        _poseStateList.clear();
        _actionStateList.clear();
        _inputCalibrationDataList.clear();
    }

    QJsonObject InputRecorder::recordDataToJson() {
        QJsonObject data;
        data["frameCount"] = _framesRecorded;
        data["version"] = CURRENT_VERSION_STRING;

        QJsonArray actionArrayList;
        for(const auto& actionState: _actionStateList) {
            QJsonArray actionArray;
            for (const auto& action: actionState) {
                QJsonObject actionJson;
                actionJson["name"] = action.first;
                actionJson["value"] = action.second;
                actionArray.append(actionJson);
            }
            actionArrayList.append(actionArray);
        }
        data["actionList"] = actionArrayList;

        QJsonArray poseArrayList;
        for (const auto& poseState: _poseStateList) {
            QJsonArray poseArray;
            for (const auto& pose: poseState) {
                QJsonObject poseJson;
                poseJson["name"] = pose.first;
                poseJson["pose"] = poseToJsonObject(pose.second);
                poseArray.append(poseJson);
            }
            poseArrayList.append(poseArray);
        }
        data["poseList"] = poseArrayList;

        QJsonArray inputCalibrationDataList;
        for (const auto& inputCalibrationData: _inputCalibrationDataList) {
            inputCalibrationDataList.append(inputCalibrationDataToJsonObject(inputCalibrationData));
        }
        data["inputCalibrationDataList"] = inputCalibrationDataList;

        return data;
    }

    void InputRecorder::saveRecording() {
        QJsonObject jsonData = recordDataToJson();
        QString timeStamp = QDateTime::currentDateTime().toString(Qt::ISODate);
        timeStamp.replace(":", "-");
        QString fileName = SAVE_DIRECTORY + FILE_PREFIX_NAME + timeStamp + COMPRESS_EXTENSION;
        exportToFile(jsonData, fileName);
    }

    void InputRecorder::loadRecording(const QString& path) {
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
        QJsonObject data = openFile(filePath, success);
        auto keyValue = data.find("version");
        if (success && keyValue != data.end()) {
            _framesRecorded = data["frameCount"].toInt();
            QJsonArray actionArrayList = data["actionList"].toArray();
            QJsonArray poseArrayList = data["poseList"].toArray();

            for (int actionIndex = 0; actionIndex < actionArrayList.size(); actionIndex++) {
                QJsonArray actionState = actionArrayList[actionIndex].toArray();
                for (int index = 0; index < actionState.size(); index++) {
                    QJsonObject actionObject = actionState[index].toObject();
                    _currentFrameActions[actionObject["name"].toString()] = actionObject["value"].toDouble();
                }
                _actionStateList.push_back(_currentFrameActions);
                _currentFrameActions.clear();
            }

            for (int poseIndex = 0; poseIndex < poseArrayList.size(); poseIndex++) {
                QJsonArray poseState = poseArrayList[poseIndex].toArray();
                for (int index = 0; index < poseState.size(); index++) {
                    QJsonObject poseObject = poseState[index].toObject();
                    _currentFramePoses[poseObject["name"].toString()] = jsonObjectToPose(poseObject["pose"].toObject());
                }
                _poseStateList.push_back(_currentFramePoses);
                _currentFramePoses.clear();
            }

            // Currently, we don't do anything with the "inputCalibrationDataList" during playback, so don't bother reading it in.
        }
        _loading = false;
    }

    void InputRecorder::stopRecording() {
        _recording = false;
        _framesRecorded = (int)_actionStateList.size();
    }

    void InputRecorder::startPlayback() {
        _playback = true;
        _recording = false;
        _playCount = 0;
    }

    void InputRecorder::stopPlayback() {
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

    void InputRecorder::setInputCalibrationData(const InputCalibrationData& inputCalibrationData) {
        if (_recording) {
            _currentInputCalibrationData = inputCalibrationData;
        }
    }

    void InputRecorder::resetFrame() {
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

    void InputRecorder::frameTick() {
        if (_recording) {
            _framesRecorded++;
            _poseStateList.push_back(_currentFramePoses);
            _actionStateList.push_back(_currentFrameActions);
            _inputCalibrationDataList.push_back(_currentInputCalibrationData);
        }

        if (_playback) {
            _playCount++;
            if (_playCount == (_framesRecorded - 1)) {
                _playCount = 0;
            }
        }
    }
}
