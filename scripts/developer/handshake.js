//
// handshake.js
//
/* global Xform */
/* eslint max-len: ["error", 1024, 4] */

(function() { // BEGIN LOCAL_SCOPE

    Script.include("/~/system/libraries/Xform.js");

    // globals transmitted to and from web app
    var BLEND_FACTOR = 0.5;  // 0 means ik follows your controller, 1 follows theirs
    var SHOW_CONTROLLERS = false;
    var USE_LOCAL_IK = true;
    var USE_STATIC_HAND_OFFSET = false;
    var USE_HAPTICS = true;
    var ALLOW_LIMB_GRABBING = false;

    //
    // tablet app boiler plate
    //

    var TABLET_BUTTON_NAME = "HANDSHAKE";
    var HTML_URL = "https://s3.amazonaws.com/hifi-public/tony/html/handshake2.html?2";

    var tablet = Tablet.getTablet("com.highfidelity.interface.tablet.system");
    var tabletButton = tablet.addButton({
        text: TABLET_BUTTON_NAME,
        icon: "https://s3.amazonaws.com/hifi-public/tony/icons/tpose-i.svg",
        activeIcon: "https://s3.amazonaws.com/hifi-public/tony/icons/tpose-a.svg"
    });

    tabletButton.clicked.connect(function () {
        if (shown) {
            tablet.gotoHomeScreen();
        } else {
            tablet.gotoWebScreen(HTML_URL);
        }
    });

    var shown = false;

    function onScreenChanged(type, url) {
        if (type === "Web" && url === HTML_URL) {
            tabletButton.editProperties({isActive: true});
            if (!shown) {
                // hook up to event bridge
                tablet.webEventReceived.connect(onWebEventReceived);
            }
            shown = true;
        } else {
            tabletButton.editProperties({isActive: false});
            if (shown) {
                // disconnect from event bridge
                tablet.webEventReceived.disconnect(onWebEventReceived);
            }
            shown = false;
        }
    }

    function onWebEventReceived(msg) {
        if (msg.name === "init-complete") {
            var values = [{name: "blend-factor", val: BLEND_FACTOR * 100, checked: false},
                          {name: "show-controllers", val: "on", checked: SHOW_CONTROLLERS},
                          {name: "use-local-ik", val: "on", checked: USE_LOCAL_IK},
                          {name: "use-static-hand-offset", val: "on", checked: USE_STATIC_HAND_OFFSET},
                          {name: "use-haptics", val: "on", checked: USE_HAPTICS}];
            tablet.emitScriptEvent(JSON.stringify(values));
        } else if (msg.name === "blend-factor") {
            BLEND_FACTOR = parseInt(msg.val, 10) / 100;
        } else if (msg.name === "show-controllers") {
            SHOW_CONTROLLERS = msg.checked;
        } else if (msg.name === "use-local-ik") {
            USE_LOCAL_IK = msg.checked;
        } else if (msg.name === "use-static-hand-offset") {
            USE_STATIC_HAND_OFFSET = msg.checked;
        } else if (msg.name === "use-haptics") {
            USE_HAPTICS = msg.checked;
        }
    }

    tablet.screenChanged.connect(onScreenChanged);

    function shutdownTabletApp() {
        tablet.removeButton(tabletButton);
        if (shown) {
            tablet.webEventReceived.disconnect(onWebEventReceived);
            tablet.gotoHomeScreen();
        }
        tablet.screenChanged.disconnect(onScreenChanged);
    }

    //
    // end tablet app boiler plate
    //

    function clamp(val, min, max) {
        return Math.max(min, Math.min(max, val));
    }

    var RED = {x: 1, y: 0, z: 0, w: 1};
    var MAGENTA = {x: 1, y: 0, z: 1, w: 1};
    var BLUE = {x: 0, y: 0, z: 1, w: 1};
    var CYAN = {x: 0, y: 1, z: 1, w: 1};
    var QUAT_Y_180 = {x: 0, y: 1, z: 0, w: 0};
    var QUAT_IDENTITY = {x: 0, y: 0, z: 0, w: 1};
    var LEFT_HAND = 0;
    var RIGHT_HAND = 1;

    var STATIC_RIGHT_HAND_DELTA_XFORM = new Xform({"x":-0.9805938601493835,"y":-0.06915176659822464,"z":0.10610653460025787,"w":-0.14965057373046875},
                                                  {"x":-0.010323882102966309,"y":0.20587468147277832,"z":0.035437583923339844});
    var STATIC_LEFT_HAND_DELTA_XFORM = new Xform({"x":-0.9791237115859985,"y":-0.16682694852352142,"z":-0.0822417140007019,"w":0.0819871574640274},
                                                 {"x":-0.037941932678222656,"y":0.19198226928710938,"z":0.05402088165283203});
    var rightHandDeltaXform = STATIC_RIGHT_HAND_DELTA_XFORM;
    var leftHandDeltaXform = STATIC_LEFT_HAND_DELTA_XFORM;

    var shakeRightHandKey;
    var shakeLeftHandKey;

    var SHAKE_TRIGGER_DISTANCE = 0.5;

    var HAPTIC_PULSE_FIRST_STRENGTH = 1.0;
    var HAPTIC_PULSE_FIRST_DURATION = 16.0;
    var HAPTIC_PULSE_DISTANCE = 0.01;  // 1 cm
    var HAPTIC_PULSE_DURATION = 1.0;
    var HAPTIC_PULSE_MIN_STRENGTH = 0.3;
    var HAPTIC_PULSE_MAX_STRENGTH = 0.5;
    var HAPTIC_PULSE_MIN_DISTANCE = 0.1;
    var HAPTIC_PULSE_MAX_DISTANCE = 0.5;

    var HAPTIC_PULSE_REMOTE_STRENGTH = 1.0;
    var HAPTIC_PULSE_REMOTE_DURATION = 1.0;

    // ctor
    function HapticBuddy(hand) {
        this.hand = hand;
        this.enabled = false;
        this.lastPulseDistance = 0;
    }

    HapticBuddy.prototype.start = function (myHand, otherHand) {
        if (USE_HAPTICS) {
            Controller.triggerHapticPulse(HAPTIC_PULSE_FIRST_STRENGTH, HAPTIC_PULSE_FIRST_DURATION, this.hand);
            this.enabled = true;
            this.lastPulseDistance = Vec3.distance(myHand.controllerPos, otherHand.controllerPos);
        }
    };

    HapticBuddy.prototype.update = function (myHand, otherHand) {
        if (this.enabled) {
            var distance = Vec3.distance(myHand.controllerPos, otherHand.controllerPos);
            if (Math.abs(this.lastPulseDistance - distance) > HAPTIC_PULSE_DISTANCE) {
                this.lastPulseDistance = distance;
                // convert distance into a value from 0 to 1
                var factor = clamp((distance - HAPTIC_PULSE_MIN_DISTANCE) / (HAPTIC_PULSE_MAX_DISTANCE - HAPTIC_PULSE_MIN_DISTANCE), 0, 1);
                // convert factor into a strength factor
                var strength = factor * (HAPTIC_PULSE_MAX_STRENGTH - HAPTIC_PULSE_MIN_STRENGTH) + HAPTIC_PULSE_MIN_STRENGTH;
                Controller.triggerHapticPulse(strength, HAPTIC_PULSE_DURATION, this.hand);
            }
        }
    };

    HapticBuddy.prototype.stop = function () {
        this.enabled = false;
    };

    var rightHapticBuddy = new HapticBuddy(RIGHT_HAND);
    var leftHapticBuddy = new HapticBuddy(LEFT_HAND);

    // ctor
    function SoundBuddy(url) {
        this.sound = SoundCache.getSound(url);
        this.injector = null;
    }

    SoundBuddy.prototype.play = function (options, doneCallback) {
        if (this.sound.downloaded) {
            if (this.injector) {
                this.injector.setOptions(options);
                this.injector.restart();
            } else {
                this.injector = Audio.playSound(this.sound, options);
                this.injector.finished.connect(function () {
                    if (doneCallback) {
                        doneCallback();
                    }
                });
            }
        }
    };

    var CLAP_SOUND = "https://s3.amazonaws.com/hifi-public/tony/audio/slap.wav";
    var clapSound = new SoundBuddy(CLAP_SOUND);

    var GRABBABLE_JOINT_NAMES = [
        "RightHand",
        "LeftHand"
    ];
    if (ALLOW_LIMB_GRABBING) {
        GRABBABLE_JOINT_NAMES = GRABBABLE_JOINT_NAMES.concat([
            "Hips",
            "RightUpLeg",
            "RightLeg",
            "RightFoot",
            "LeftUpLeg",
            "LeftLeg",
            "LeftFoot",
            "Spine2",
            "Neck",
            "Head",
            "RightShoulder",
            "RightArm",
            "RightForeArm",
            "RightHand",
            "LeftShoulder",
            "LeftArm",
            "LeftForeArm",
            "LeftHand"
        ]);
    }

    // ctor
    function GrabbableJointScanner(addAvatarCb, updateAvatarCb, removeAvatarCb) {
        this.addAvatarCb = addAvatarCb;
        this.updateAvatarCb = updateAvatarCb;
        this.removeAvatarCb = removeAvatarCb;
        this.prevAvatarMap = {};
    }

    GrabbableJointScanner.prototype.destroy = function () {
        // remove all avatars
        Object.keys(this.prevAvatarMap).forEach(function (id) {
            if (this.removeAvatarCb) {
                this.removeAvatarCb(id);
            }
        });
    };

    /*
     * Returns an object with jointName's as keys, and jointInfo's as values,
     * where jointInfo is an object like the following:
     *
     * {
     *   jointName: string,
     *   jointIndex: number,
     *   controllerRot: quat,
     *   controllerPos: vec3,
     *   controllerValid: bool,
     *   jointRot: quat,
     *   jointPos: pos,
     *   jointValid: bool
     * }
     *
     * All positions and rotations are in world space
     *
     */
    GrabbableJointScanner.prototype.computeJointInfoMap = function (id) {
        var avatar = id ? AvatarManager.getAvatar(id) : MyAvatar;
        var avatarXform = new Xform(avatar.orientation, avatar.position);
        var jointInfoMap = {};
        GRABBABLE_JOINT_NAMES.forEach(function (jointName) {
            var jointIndex = avatar.getJointIndex(jointName);
            if (jointIndex < 0) {
                return;
            }
            var result = {
                jointName: jointName,
                jointIndex: jointIndex,
                controllerRot: {x: 0, y: 0, z: 0, w: 1},
                controllerPos: {x: 0, y: 0, z: 0},
                controllerValid: false,
                jointRot: {x: 0, y: 0, z: 0, w: 1},
                jointPos: {x: 0, y: 0, z: 0},
                jointValid: false
            };

            // transform joint into world space
            var localXform = new Xform(avatar.getAbsoluteJointRotationInObjectFrame(jointIndex),
                                       avatar.getAbsoluteJointTranslationInObjectFrame(jointIndex));
            var worldXform = Xform.mul(avatarXform, localXform);
            result.jointRot = worldXform.rot;
            result.jointPos = worldXform.pos;
            result.jointValid = true;

            // get controller pos/rot, if available.
            if (jointName === "LeftHand" || jointName === "RightHand") {
                var mat, palmPos, palmRot;
                if (jointName === "LeftHand") {
                    mat = avatar.controllerLeftHandMatrix;
                    palmPos = avatar.getLeftPalmPosition();
                    palmRot = avatar.getLeftPalmRotation();
                } else {
                    mat = avatar.controllerRightHandMatrix;
                    palmPos = avatar.getRightPalmPosition();
                    palmRot = avatar.getRightPalmRotation();
                }

                var localPalmPos = worldXform.inv().xformPoint(palmPos);

                var PALM_Y_OFFSET_FACTOR = 0.8;
                var PALM_Z_OFFSET_FACTOR = 0.4;

                // adjust the palm position to better match the handshake palm position.
                localPalmPos.y = localPalmPos.y * PALM_Y_OFFSET_FACTOR;
                localPalmPos.z = localPalmPos.z * PALM_Z_OFFSET_FACTOR;

                // transform local palm back into world space
                result.palmPos = worldXform.xformPoint(localPalmPos);
                result.palmRot = palmRot;

                // check to see if hand controller is valid
                var localControllerXform = new Xform(Mat4.extractRotation(mat),
                                                     Mat4.extractTranslation(mat));
                if (!isIdentity(localControllerXform.rot, localControllerXform.pos)) {
                    var worldControllerXform = Xform.mul(avatarXform, localControllerXform);
                    result.controllerRot = worldControllerXform.rot;
                    result.controllerPos = worldControllerXform.pos;
                    result.controllerValid = true;
                } else {
                    result.controllerRot = result.jointRot;
                    result.controllerPos = result.jointPos;
                    result.controllerValid = false;
                }
            } else {
                result.controllerRot = result.jointRot;
                result.controllerPos = result.jointPos;
                result.controllerValid = false;
            }

            jointInfoMap[jointName] = result;
        });

        return jointInfoMap;
    };

    GrabbableJointScanner.prototype.scan = function () {

        var AVATAR_RANGE = 5; // meters
        var avatarIds = AvatarManager.getAvatarsInRange(MyAvatar.position, AVATAR_RANGE);
        var avatarMap = {};
        var self = this;
        avatarIds.forEach(function (id) {
            if (id === null || id === MyAvatar.sessionUUID) {
                id = MyAvatar.SELF_ID;
            }
            avatarMap[id] = {
                jointInfoMap: self.computeJointInfoMap(id)
            };
        });

        // detect removal
        Object.keys(this.prevAvatarMap).forEach(function (id) {
            if (!avatarMap[id]) {
                if (self.removeAvatarCb) {
                    self.removeAvatarCb(id);
                }
            }
        });

        // detect addition
        Object.keys(avatarMap).forEach(function (id) {
            if (self.prevAvatarMap[id]) {
                if (self.updateAvatarCb) {
                    self.updateAvatarCb(id, avatarMap[id]);
                }
            } else {
                if (self.addAvatarCb) {
                    self.addAvatarCb(id, avatarMap[id]);
                }
            }
        });

        this.prevAvatarMap = avatarMap;
    };

    /*
     * searches the most recently scanned jointInfoMap for a grabbable joint on other avatar.
     * it will return null, if none is found or a key object like the following:
     * {
     *   avatarId: uuid,
     *   jointName: string
     * }
     *
     */
    GrabbableJointScanner.prototype.findGrabbableJoint = function (hand, grabDistance) {

        var myHand = this.getMyHand(hand);
        if (!myHand) {
            return null;
        }

        var avatarIds = Object.keys(this.prevAvatarMap);
        var closestId;
        var closestJointName;
        var closestDist = Number.MAX_VALUE;
        var self = this;
        avatarIds.forEach(function (id) {
            if (id !== MyAvatar.SELF_ID) {
                var jointInfoMap = self.prevAvatarMap[id].jointInfoMap;
                Object.keys(jointInfoMap).forEach(function (jointName) {
                    var jointInfo = jointInfoMap[jointName];
                    var dist = Vec3.distance(myHand.jointPos, jointInfo.jointPos);
                    if (dist < grabDistance && dist < closestDist) {
                        closestId = id;
                        closestJointName = jointName;
                        closestDist = dist;
                    }
                });
            }
        });


        if (!closestId) {
            return null;
        } else {
            return {avatarId: closestId, jointName: closestJointName};
        }
    };

    /*
     * keyObj should be of the following format:
     * {
     *   avatarId: uuid,
     *   jointName: string
     * }
     *
     * the result will be null, if no jointInfo is found or an object with the following format:
     *
     * {
     *   jointName: string,
     *   jointIndex: number,
     *   controllerRot: quat,
     *   controllerPos: vec3,
     *   controllerValid: bool,
     *   jointRot: quat,
     *   jointPos: pos,
     *   jointValid: bool
     * }
     *
     */
    GrabbableJointScanner.prototype.getJointInfo = function (keyObj) {
        if (keyObj) {
            return this.prevAvatarMap[keyObj.avatarId].jointInfoMap[keyObj.jointName];
        }
    };

    GrabbableJointScanner.prototype.getMyHand = function (hand) {
        if (hand === RIGHT_HAND) {
            return this.getJointInfo({avatarId: MyAvatar.SELF_ID, jointName: "RightHand"});
        } else {
            return this.getJointInfo({avatarId: MyAvatar.SELF_ID, jointName: "LeftHand"});
        }
    };

    function tweenXform(a, b, alpha) {
        return new Xform(Quat.mix(a.rot, b.rot, alpha), Vec3.sum(Vec3.multiply(1 - alpha, a.pos), Vec3.multiply(alpha, b.pos)));
    }

    function calculateDeltaOffset(myHand, jointInfo) {
        var myJointXform = new Xform(myHand.jointRot, myHand.jointPos);
        var otherJointXform = new Xform(jointInfo.jointRot, jointInfo.jointPos);
        return Xform.mul(otherJointXform.inv(), myJointXform);
    }

    function calculateHandDeltaOffset(myHand, otherHand) {
        var myJointXform = new Xform(myHand.jointRot, myHand.jointPos);
        var otherJointXform = new Xform(otherHand.jointRot, otherHand.jointPos);
        var palmOffset = new Xform(QUAT_IDENTITY, Vec3.subtract(otherHand.palmPos, myHand.palmPos));
        return Xform.mul(otherJointXform.inv(), Xform.mul(palmOffset, myJointXform));
    }

    // Controller system callback on leftTrigger pull or release.
    function leftTrigger(value) {
        var jointInfo;
        if (value === 1) {
            if (!shakeLeftHandKey) {
                var key = scanner.findGrabbableJoint(LEFT_HAND, SHAKE_TRIGGER_DISTANCE);
                if (key) {
                    var myHand = scanner.getMyHand(LEFT_HAND);
                    jointInfo = scanner.getJointInfo(key);

                    Messages.sendMessage("Hifi-Hand-Disabler", "left");

                    if (jointInfo.jointName === "LeftHand") {
                        if (USE_STATIC_HAND_OFFSET) {
                            leftHandDeltaXform = STATIC_LEFT_HAND_DELTA_XFORM;
                        } else {
                            leftHandDeltaXform = calculateHandDeltaOffset(myHand, jointInfo);
                        }
                    } else {
                        leftHandDeltaXform = calculateDeltaOffset(myHand, jointInfo);
                    }

                    shakeLeftHandKey = key;
                    leftHapticBuddy.start(myHand, jointInfo);
                    clapSound.play({position: myHand.jointPos, loop: false, localOnly: true});

                    var msg = {type: "pulse", receiver: key.avatarId, hand: "left"};
                    Messages.sendMessage("Hifi-Handshake", JSON.stringify(msg));

                    print("AJT SEND: Hifi-Handshake = " + JSON.stringify(msg));
                }
            }
        } else {
            // unset shakeLeftHandKey
            if (shakeLeftHandKey) {
                var avatar = AvatarManager.getAvatar(shakeLeftHandKey.avatarId);
                jointInfo = scanner.getJointInfo(shakeLeftHandKey);
                avatar.clearPinOnJoint(jointInfo.jointIndex);
                shakeLeftHandKey = undefined;
            }
            leftHapticBuddy.stop();
            Messages.sendMessage("Hifi-Hand-Disabler", "none");
        }
    }

    // Controller system callback on rightTrigger pull or release.
    function rightTrigger(value) {
        var jointInfo;
        if (value === 1) {
            if (!shakeRightHandKey) {
                print("AJT: rightTrigger");
                var key = scanner.findGrabbableJoint(RIGHT_HAND, SHAKE_TRIGGER_DISTANCE);
                print("AJT:     findGrabbableJoint() key = " + JSON.stringify(key));
                if (key) {
                    var myHand = scanner.getMyHand(RIGHT_HAND);
                    jointInfo = scanner.getJointInfo(key);

                    print("AJT:     myHand = " + JSON.stringify(myHand));
                    print("AJT:     jointInfo = " + JSON.stringify(jointInfo));

                    Messages.sendMessage("Hifi-Hand-Disabler", "right");

                    if (jointInfo.jointName === "RightHand") {
                        if (USE_STATIC_HAND_OFFSET) {
                            rightHandDeltaXform = STATIC_RIGHT_HAND_DELTA_XFORM;
                        } else {
                            rightHandDeltaXform = calculateHandDeltaOffset(myHand, jointInfo);
                        }
                    } else {
                        rightHandDeltaXform = calculateDeltaOffset(myHand, jointInfo);
                    }

                    print("AJT:     rightHandDeltaXform = " + JSON.stringify(rightHandDeltaXform));

                    shakeRightHandKey = key;
                    rightHapticBuddy.start(myHand, jointInfo);
                    clapSound.play({position: myHand.jointPos, loop: false, localOnly: true});

                    var msg = {type: "pulse", receiver: key.avatarId, hand: "right"};
                    Messages.sendMessage("Hifi-Handshake", JSON.stringify(msg));

                    print("AJT SEND: Hifi-Handshake = " + JSON.stringify(msg));
                }
            }
        } else {
            // unset shakeRightHandKey
            if (shakeRightHandKey) {
                var avatar = AvatarManager.getAvatar(shakeRightHandKey.avatarId);
                jointInfo = scanner.getJointInfo(shakeRightHandKey);
                avatar.clearPinOnJoint(jointInfo.jointIndex);
                shakeRightHandKey = undefined;
            }
            rightHapticBuddy.stop();
            Messages.sendMessage("Hifi-Hand-Disabler", "none");
        }
    }

    var connectionMapping = Controller.newMapping('handshake-grip');
    var animationHandlerId;
    var ANIM_VARS = [
        "leftHandPosition",
        "leftHandRotation",
        "rightHandPosition",
        "rightHandRotation"
    ];

    function handTween(myHand, otherHand, handDelta) {
        var myXform = new Xform(myHand.controllerRot, myHand.controllerPos);
        var otherXform = Xform.mul(new Xform(otherHand.controllerRot, otherHand.controllerPos), handDelta);
        return tweenXform(myXform, otherXform, BLEND_FACTOR);
    }

    // message should look like: {type: "pulse", receiver: uuid, hand: "left"}
    function messageHandler(channel, message, sender) {
        if (channel === "Hifi-Handshake") {
            print("AJT: Receive Hifi-Handshake = " + message);
            var obj = JSON.parse(message);
            if (obj.receiver === MyAvatar.sessionUUID) {
                if (obj.type === "pulse") {
                    if (obj.hand === "left") {
                        Controller.triggerHapticPulse(HAPTIC_PULSE_REMOTE_STRENGTH, HAPTIC_PULSE_REMOTE_DURATION, LEFT_HAND);
                    } else if (obj.hand === "right") {
                        Controller.triggerHapticPulse(HAPTIC_PULSE_REMOTE_STRENGTH, HAPTIC_PULSE_REMOTE_DURATION, RIGHT_HAND);
                    }
                }
            }
        }
    }

    function init() {
        connectionMapping.from(Controller.Standard.RTClick).peek().to(rightTrigger);
        connectionMapping.from(Controller.Standard.LTClick).peek().to(leftTrigger);
        connectionMapping.enable();

        animationHandlerId = MyAvatar.addAnimationStateHandler(function (props) {

            var result = {};
            if (props.leftHandRotation) {
                result.leftHandRotation = props.leftHandRotation;
            }
            if (props.leftHandPosition) {
                result.leftHandPosition = props.leftHandPosition;
            }
            if (props.rightHandRotation) {
                result.rightHandRotation = props.rightHandRotation;
            }
            if (props.rightHandPosition) {
                result.rightHandPosition = props.rightHandPosition;
            }

            var myHand, jointInfo, targetXform, localTargetXform;
            var avatarXform = new Xform(Quat.multiply(MyAvatar.orientation, QUAT_Y_180), MyAvatar.position);

            if (shakeRightHandKey && props.rightHandRotation && props.rightHandPosition) {
                myHand = scanner.getMyHand(RIGHT_HAND);
                jointInfo = scanner.getJointInfo(shakeRightHandKey);
                if (myHand && jointInfo) {
                    targetXform = handTween(myHand, jointInfo, rightHandDeltaXform);
                    localTargetXform = Xform.mul(avatarXform.inv(), targetXform);
                    result.rightHandRotation = localTargetXform.rot;
                    result.rightHandPosition = localTargetXform.pos;
                    rightHapticBuddy.update(myHand, jointInfo);
                }
            }

            if (shakeLeftHandKey && props.leftHandRotation && props.leftHandPosition) {

                myHand = scanner.getMyHand(LEFT_HAND);
                jointInfo = scanner.getJointInfo(shakeLeftHandKey);
                if (myHand && jointInfo) {
                    targetXform = handTween(myHand, jointInfo, leftHandDeltaXform);
                    localTargetXform = Xform.mul(avatarXform.inv(), targetXform);
                    result.leftHandRotation = localTargetXform.rot;
                    result.leftHandPosition = localTargetXform.pos;
                    leftHapticBuddy.update(myHand, jointInfo);
                }
            }

            return result;
        }, ANIM_VARS);

        Script.update.connect(update);
        Script.scriptEnding.connect(shutdown);

        Messages.subscribe("Hifi-Handshake");
        Messages.messageReceived.connect(messageHandler);
    }

    function isIdentity(rot, pos) {
        var EPSILON = 0.01;
        return (Math.abs(rot.x) < EPSILON &&
                Math.abs(rot.y) < EPSILON &&
                Math.abs(rot.z) < EPSILON &&
                Math.abs(rot.w) - 1.0 < EPSILON && // 1 or -1 is ok.
                Math.abs(pos.x) < EPSILON &&
                Math.abs(pos.y) < EPSILON &&
                Math.abs(pos.z) < EPSILON);
    }

    function addAvatar(id, data) {
        print("addAvatar(" + id + ")");
        print("typeof id = " + (typeof id));
        print("JSON.stringify(id) = " + JSON.stringify(id));
        if (id === "null") {
            id = MyAvatar.SELF_ID;
        }
        print("addAvatar(" + id + ")");
        updateAvatar(id, data);
    }

    function updateAvatar(id, data) {

        // AJT: BROKEN
        /*
        if (SHOW_CONTROLLERS && data.leftHand.controllerValid) {
            DebugDraw.addMarker("leftHandController" + id, data.leftHand.controllerRot, data.leftHand.controllerPos, CYAN);
            DebugDraw.addMarker("leftHandPalm" + id, data.leftHand.palmRot, data.leftHand.palmPos, BLUE);
        } else {
            DebugDraw.removeMarker("leftHandController" + id);
            DebugDraw.removeMarker("leftHandPalm" + id);
        }

        if (SHOW_CONTROLLERS && data.rightHand.controllerValid) {
            DebugDraw.addMarker("rightHandController" + id, data.rightHand.controllerRot, data.rightHand.controllerPos, MAGENTA);
            DebugDraw.addMarker("rightHandPalm" + id, data.rightHand.palmRot, data.rightHand.palmPos, RED);
        } else {
            DebugDraw.removeMarker("rightHandController" + id);
            DebugDraw.removeMarker("rightHandPalm" + id);
        }
        */

        // local IK on other avatars hands
        var myHand, jointInfo, targetXform, otherTargetXform;
        var avatar = AvatarManager.getAvatar(id);
        if (id !== MyAvatar.SELF_ID && avatar) {
            if (shakeRightHandKey && shakeRightHandKey.avatarId === id && USE_LOCAL_IK) {
                print("AJT: updateAvatar() shakeRightHandKey = " + shakeRightHandKey);
                myHand = scanner.getMyHand(RIGHT_HAND);
                jointInfo = scanner.getJointInfo(shakeRightHandKey);

                print("AJT:    myHand = " + JSON.stringify(myHand));
                print("AJT:    jointInfo = " + JSON.stringify(jointInfo));

                if (myHand && jointInfo) {
                    targetXform = handTween(myHand, jointInfo, rightHandDeltaXform);
                    otherTargetXform = Xform.mul(targetXform, rightHandDeltaXform.inv());

                    // world frame
                    avatar.pinJoint(jointInfo.jointIndex, otherTargetXform.pos, otherTargetXform.rot);
                }
            }

            if (shakeLeftHandKey && shakeLeftHandKey.avatarId === id && USE_LOCAL_IK) {
                myHand = scanner.getMyHand(LEFT_HAND);
                jointInfo = scanner.getJointInfo(shakeLeftHandKey);
                if (myHand && jointInfo) {
                    targetXform = handTween(myHand, jointInfo, leftHandDeltaXform);
                    otherTargetXform = Xform.mul(targetXform, leftHandDeltaXform.inv());

                    // world frame
                    avatar.pinJoint(jointInfo.jointIndex, otherTargetXform.pos, otherTargetXform.rot);
                }
            }
        }
    }

    function removeAvatar(id) {
        print("removeAvatar(" + id + ")");
        DebugDraw.removeMarker("leftHandController" + id);
        DebugDraw.removeMarker("leftHandJoint" + id);
        DebugDraw.removeMarker("rightHandController" + id);
        DebugDraw.removeMarker("rightHandJoint" + id);
    }

    var scanner = new GrabbableJointScanner(addAvatar, updateAvatar, removeAvatar);

    function update(dt) {
        scanner.scan();
    }

    function shutdown() {
        Messages.messageReceived.disconnect(messageHandler);
        MyAvatar.removeAnimationStateHandler(animationHandlerId);

        scanner.destroy();

        connectionMapping.disable();
        shutdownTabletApp();
    }

    init();

}()); // END LOCAL_SCOPE
