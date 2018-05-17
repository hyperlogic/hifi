//
// handshake.js
//
/* global Xform */
/* eslint max-len: ["error", 1024, 4] */
/* eslint brace-style: "stroustrup" */
(function() { // BEGIN LOCAL_SCOPE

    Script.include("/~/system/libraries/Xform.js");

    var ALLOW_LIMB_GRABBING = false;

    // globals transmitted to and from web app
    var GRAB_DISTANCE = 0.2;
    var REJECT_DISTANCE = 0.5;

    //
    // tablet app boiler plate
    //

    var TABLET_BUTTON_NAME = "HANDSHAKE";
    var HTML_URL = "https://s3.amazonaws.com/hifi-public/tony/html/handshake3.html?1";

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

    function onScreenChanged(type: string, url: string) {
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

    interface GrabWebEvent {
        name: string;
        val: string;
    }

    function onWebEventReceived(msg: GrabWebEvent) {
        if (msg.name === "init-complete") {
            var values = [{name: "grab-distance", val: GRAB_DISTANCE * 100, checked: false},
                          {name: "reject-distance", val: REJECT_DISTANCE * 100, checked: false}];
            tablet.emitScriptEvent(JSON.stringify(values));
        } else if (msg.name === "grab-distance") {
            GRAB_DISTANCE = parseInt(msg.val, 10) / 100;
        } else if (msg.name === "reject-distance") {
            REJECT_DISTANCE = parseInt(msg.val, 10) / 100;
        }
    }

    tablet.screenChanged.connect(onScreenChanged);

    function shutdownTabletApp(): void {
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

    function clamp(val: number, min: number, max: number): number {
        return Math.max(min, Math.min(max, val));
    }

    var QUAT_Y_180 = {x: 0, y: 1, z: 0, w: 0};
    var QUAT_IDENTITY = {x: 0, y: 0, z: 0, w: 1};
    var LEFT_HAND = 0;
    var RIGHT_HAND = 1;

    var myAvatarRightHandXform;
    var myAvatarLeftHandXform;

    var HAPTIC_PULSE_FIRST_STRENGTH = 1.0;
    var HAPTIC_PULSE_FIRST_DURATION = 16.0;
    var HAPTIC_PULSE_DISTANCE = 0.01;  // 1 cm

    var HAPTIC_PULSE_MAX_STRENGTH = 0.5;

    // used by GrabLink
    var HAPTIC_PULSE_STRENGTH = 1.0;
    var HAPTIC_PULSE_DURATION = 1.0;

    interface JointInfo {
        jointName: string;
        jointIndex: number;
        controllerRot: quat;  // world space
        controllerPos: vec3;  // world space
        controllerValid: boolean;
        jointRot: quat;  // world space
        jointPos: vec3;  // world space
        jointValid: boolean;
    }

    interface JointInfoMap {
        [jointName: string]: JointInfo;
    }

    class HapticBuddy {
        protected hand: number;
        protected enabled: boolean;
        protected frequencyScale: number;
        protected lastPulseDistance: number;

        constructor(hand: number) {
            this.hand = hand;
            this.enabled = false;
            this.frequencyScale = 1;
            this.lastPulseDistance = 0;
        }

        setFrequencyScale(frequencyScale: number): void {
            this.frequencyScale = frequencyScale;
        }

        start(myHand: JointInfo, otherHand: JointInfo): void {
            Controller.triggerHapticPulse(HAPTIC_PULSE_FIRST_STRENGTH, HAPTIC_PULSE_FIRST_DURATION, this.hand);
            this.enabled = true;
            this.lastPulseDistance = Vec3.distance(myHand.controllerPos, otherHand.controllerPos);
        }

        update(myHand: JointInfo, otherHand: JointInfo): void {
            if (this.enabled) {
                var distance = Vec3.distance(myHand.controllerPos, otherHand.controllerPos);
                if (Math.abs(this.lastPulseDistance - distance) > (HAPTIC_PULSE_DISTANCE / this.frequencyScale)) {
                    this.lastPulseDistance = distance;

                    // AJT: disable dynamic strength
                    /*
                    // convert distance into a value from 0 to 1
                    var factor = clamp((distance - HAPTIC_PULSE_MIN_DISTANCE) / (HAPTIC_PULSE_MAX_DISTANCE - HAPTIC_PULSE_MIN_DISTANCE), 0, 1);
                    // convert factor into a strength factor
                    var strength = factor * (HAPTIC_PULSE_MAX_STRENGTH - HAPTIC_PULSE_MIN_STRENGTH) + HAPTIC_PULSE_MIN_STRENGTH;
                    */

                    Controller.triggerHapticPulse(HAPTIC_PULSE_MAX_STRENGTH, HAPTIC_PULSE_DURATION, this.hand);
                }
            }
        }

        stop(): void {
            this.enabled = false;
        }
    }

    var rightHapticBuddy = new HapticBuddy(RIGHT_HAND);
    var leftHapticBuddy = new HapticBuddy(LEFT_HAND);

    class SoundBuddy {
        protected sound: AudioResource;
        protected injector: AudioInjector;

        constructor(url: string) {
            this.sound = SoundCache.getSound(url);
            this.injector = null;
        }

        play(options: InjectorOptions, doneCallback?: Function): void {
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
        }
    }

    var CLAP_SOUND = "https://s3.amazonaws.com/hifi-public/tony/audio/slap.wav";
    var clapSound = new SoundBuddy(CLAP_SOUND);

    interface GrabKey {
        avatarId: string;
        jointName: string;
    }

    interface GrabMessage {
        type: string;
        receiver: string;
        grabbingJoint: string;
        grabbedJoint: string;
        relXform: Xform;
    }

    // A GrabLink is single grab interaction link between two avatars, typically between MyAvatar and another avatar in the scene.
    // It has an internal state machine to keep track of when to enable/disable IK and play sound effects.
    // state element of {alone, leader, follower, peer, reject}
    class GrabLink {
        static grabLinkMapMap: { [myJointName: string]: { [otherJointName: string]: GrabLink } } = {};
        static rightControllerDispatcherEnabled: boolean = false;
        static leftControllerDispatcherEnabled: boolean = false;

        protected myKey: GrabKey;
        protected otherKey: GrabKey;
        protected myJointInfo: JointInfo;
        protected otherJointInfo: JointInfo;
        protected state: string;
        protected relXform: Xform;
        protected states: object;
        protected timeInState: number;

        constructor(myKey: GrabKey, otherKey: GrabKey) {
            console.warn("AJT: new GrabLink(" + myKey.jointName + ", " + JSON.stringify(otherKey) + ")");

            this.myKey = myKey;
            this.otherKey = otherKey;
            this.state = "alone";
            this.relXform = new Xform({x: 0, y: 0, z: 0, w: 1}, {x: 0, y: 0, z: 0});
            this.states = {
                alone: {
                    enter: this.aloneEnter,
                    process: this.aloneProcess,
                    exit: this.aloneExit
                },
                follower: {
                    enter: this.followerEnter,
                    process: this.followerProcess,
                    exit: this.followerExit
                },
                peer: {
                    enter: this.peerEnter,
                    process: this.peerProcess,
                    exit: this.peerExit
                },
                leader: {
                    enter: this.leaderEnter,
                    process: this.leaderProcess,
                    exit: this.leaderExit
                },
                reject: {
                    enter: this.rejectEnter,
                    process: this.rejectProcess,
                    exit: this.rejectExit
                }
            };
            this.myJointInfo = scanner.getJointInfo(myKey);
            this.otherJointInfo = scanner.getJointInfo(otherKey);
            this.timeInState = 0;
        }

        static findOrCreateLink(myKey: GrabKey, otherKey: GrabKey): GrabLink {
            var grabLinkMap = GrabLink.grabLinkMapMap[myKey.jointName];
            var link;
            if (!grabLinkMap) {
                link = new GrabLink(myKey, otherKey);
                var map = {};
                map[otherKey.jointName] = link;
                GrabLink.grabLinkMapMap[myKey.jointName] = map;
            } else {
                link = grabLinkMap[otherKey.jointName];
                if (!link) {
                    link = new GrabLink(myKey, otherKey);
                    grabLinkMap[otherKey.jointName] = link;
                }
            }
            return link;
        }

        static findLink(myKey: GrabKey, otherKey: GrabKey): GrabLink {
            var grabLinkMap = GrabLink.grabLinkMapMap[myKey.jointName];
            if (!grabLinkMap) {
                return undefined;
            } else {
                return grabLinkMap[otherKey.jointName];
            }
        }

        static process(dt: number): void {
            var myKeys = Object.keys(GrabLink.grabLinkMapMap);
            var i, numMyKeys = myKeys.length;
            for (i = 0; i < numMyKeys; i++) {
                var otherMap = GrabLink.grabLinkMapMap[myKeys[i]];
                var otherKeys = Object.keys(otherMap);
                var j, numOtherKeys = otherKeys.length;
                for (j = 0; j < numOtherKeys; j++) {
                    GrabLink.grabLinkMapMap[myKeys[i]][otherKeys[j]].process(dt);
                }
            }
        }

        changeState(newState: string): void {
            if (this.state !== newState) {

                console.warn("AJT: GrabLink(" + this.myKey.jointName + ", " + JSON.stringify(this.otherKey) + "), changeState " + this.state + " -> " + newState);

                // exit the old state
                this.states[this.state].exit.apply(this);

                // enter the new state
                this.states[newState].enter.apply(this);

                this.state = newState;
                this.timeInState = 0;
            }
        }

        process(dt: number): void {
            this.timeInState += dt;
            this.states[this.state].process.apply(this, [dt]);
        }

        hapticPulse(): void {
            if (this.myKey.jointName.slice(0, 4) === "Left") {
                Controller.triggerHapticPulse(HAPTIC_PULSE_STRENGTH, HAPTIC_PULSE_DURATION, LEFT_HAND);
            } else if (this.myKey.jointName.slice(0, 5) === "Right") {
                Controller.triggerHapticPulse(HAPTIC_PULSE_STRENGTH, HAPTIC_PULSE_DURATION, RIGHT_HAND);
            }
        }

        clearPinOnJoint(key: GrabKey): void {
            console.warn("AJT: clearPinOnJoint key = " + JSON.stringify(key));
            if (key.avatarId === this.myKey.avatarId) {

                console.warn("AJT: clearPinOnJoint myAvatar!");

                // AJT: TODO for now we only support hands for MyAvatar
                if (key.jointName === "LeftHand") {
                    console.warn("AJT: clearPinOnJoint, myAvatar, leftHand");
                    myAvatarLeftHandXform = undefined;
                } else if (key.jointName === "RightHand") {
                    console.warn("AJT: clearPinOnJoint, myAvatar, rightHand");
                    myAvatarRightHandXform = undefined;
                }
            } else if (key.avatarId === this.otherKey.avatarId) {

                console.warn("AJT: clearPinOnJoint other avatar!");

                var avatar = AvatarManager.getAvatar(this.otherKey.avatarId);
                if (avatar) {
                    console.warn("AJT: clearPinOnJoint, otherAvatar, jointIndex = " + this.otherJointInfo.jointIndex);
                    avatar.clearPinOnJoint(this.otherJointInfo.jointIndex);
                } else {
                    console.warn("AJT: WARNING: clearPinOnJoint() no avatar found");
                }
            } else {
                console.warn("AJT: WARNING: clearPinOnJoint() bad key");
            }
        }

        pinJoint(key: GrabKey, jointInfo: JointInfo, targetXform: Xform): void {
            if (key.avatarId === this.myKey.avatarId) {
                // AJT: TODO for now we only support hands for MyAvatar
                // Modify the myAvatar*HandXform global which will control MyAvatar's IK
                if (key.jointName === "LeftHand") {
                    myAvatarLeftHandXform = targetXform;
                } else if (key.jointName === "RightHand") {
                    myAvatarRightHandXform = targetXform;
                }
            } else if (key.avatarId === this.otherKey.avatarId) {
                var avatar = AvatarManager.getAvatar(this.otherKey.avatarId);
                if (avatar) {
                    avatar.pinJoint(jointInfo.jointIndex, targetXform.pos, targetXform.rot);
                }
            } else {
                console.warn("AJT: WARNING: pinJoint unknown avatarId, key " + JSON.stringify(key) + ", myKey = " + JSON.stringify(this.myKey) + ", otherKey = " + JSON.stringify(this.otherKey));
            }
        }

        computeDeltaXform(): void {
           // compute delta xform
           if ((this.myJointInfo.jointName === "LeftHand" && this.otherJointInfo.jointName === "LeftHand") ||
               (this.myJointInfo.jointName === "RightHand" && this.otherJointInfo.jointName === "RightHand")) {
               this.relXform = calculateHandDeltaOffset(this.myJointInfo, this.otherJointInfo);
           } else {
               this.relXform = calculateDeltaOffset(this.myJointInfo, this.otherJointInfo);
           }
        }

        enableControllerDispatcher(): void {
            // enable controller dispatcher script for this hand.
            if (this.myKey.jointName === "LeftHand") {
                if (GrabLink.rightControllerDispatcherEnabled) {
                    console.warn("AJT: send disable none");
                    Messages.sendMessage("Hifi-Hand-Disabler", "none");
                } else {
                    console.warn("AJT: send disable right");
                    Messages.sendMessage("Hifi-Hand-Disabler", "right");
                }
                GrabLink.leftControllerDispatcherEnabled = true;
            } else if (this.myKey.jointName === "RightHand") {
                if (GrabLink.leftControllerDispatcherEnabled) {
                    console.warn("AJT: send disable none");
                    Messages.sendMessage("Hifi-Hand-Disabler", "none");
                } else {
                    console.warn("AJT: send disable left");
                    Messages.sendMessage("Hifi-Hand-Disabler", "left");
                }
                GrabLink.rightControllerDispatcherEnabled = true;
            }
        }

        disableControllerDispatcher(): void {
            // disable controller dispatcher script for this hand.
            if (this.myKey.jointName === "LeftHand") {
                if (GrabLink.rightControllerDispatcherEnabled) {
                    console.warn("AJT: send disable left");
                    Messages.sendMessage("Hifi-Hand-Disabler", "left");
                } else {
                    console.warn("AJT: send disable both");
                    Messages.sendMessage("Hifi-Hand-Disabler", "both");
                }
                GrabLink.leftControllerDispatcherEnabled = false;
            } else if (this.myKey.jointName === "RightHand") {
                if (GrabLink.leftControllerDispatcherEnabled) {
                    console.warn("AJT: send disable right");
                    Messages.sendMessage("Hifi-Hand-Disabler", "right");
                } else {
                    console.warn("AJT: send disable both");
                    Messages.sendMessage("Hifi-Hand-Disabler", "both");
                }
                GrabLink.rightControllerDispatcherEnabled = false;
            }
        }

        sendGrabMessage(): void {
            var msg: GrabMessage = {
                type: "grab",
                receiver: this.otherKey.avatarId,
                grabbingJoint: this.myKey.jointName,
                grabbedJoint: this.otherKey.jointName,
                relXform: this.relXform
            };

            console.warn("AJT: sendGrabMessage, msg = " + JSON.stringify(msg));

            Messages.sendMessage("Hifi-Handshake", JSON.stringify(msg));
        }

        sendReleaseMessage(): void {
            var msg: GrabMessage = {
                type: "release",
                receiver: this.otherKey.avatarId,
                grabbingJoint: this.myKey.jointName,
                grabbedJoint: this.otherKey.jointName,
                relXform: this.relXform
            };

            console.warn("AJT: sendReleaseMessage, msg = " + JSON.stringify(msg));

            Messages.sendMessage("Hifi-Handshake", JSON.stringify(msg));
        }

        sendRejectMessage(): void {
            var msg = {
                type: "release",
                receiver: this.otherKey.avatarId,
                grabbingJoint: this.myKey.jointName,
                grabbedJoint: this.otherKey.jointName,
                relXform: this.relXform
            };

            console.warn("AJT: sendReleaseMessage, msg = " + JSON.stringify(msg));

            Messages.sendMessage("Hifi-Handshake", JSON.stringify(msg));
        }

        playClap(): void {
            clapSound.play({position: this.myJointInfo.jointPos, loop: false});
        }

        getHapticBuddy(): HapticBuddy {
            if (this.myKey.jointName === "RightHand") {
                return rightHapticBuddy;
            } else if (this.myKey.jointName === "LeftHand") {
                return leftHapticBuddy;
            } else {
                return undefined;
            }
        }

        startStressHaptics(): void {
            var hapticBuddy = this.getHapticBuddy();
            if (hapticBuddy) {
                hapticBuddy.start(this.myJointInfo, this.otherJointInfo);
            }
        }

        updateStressHaptics(): void {
            var hapticBuddy = this.getHapticBuddy();
            if (hapticBuddy) {
                hapticBuddy.update(this.myJointInfo, this.otherJointInfo);
            }
        }

        stopStressHaptics(): void {
            var hapticBuddy = this.getHapticBuddy();
            if (hapticBuddy) {
                hapticBuddy.stop();
            }
        }

        startRejectHaptics(): void {
            var hapticBuddy = this.getHapticBuddy();
            if (hapticBuddy) {
                hapticBuddy.start(this.myJointInfo, this.otherJointInfo);
            }
        }

        updateRejectHaptics(): void {
            var myXform = new Xform(this.myJointInfo.controllerRot, this.myJointInfo.controllerPos);
            var otherXform = new Xform(this.otherJointInfo.controllerRot, this.otherJointInfo.controllerPos);
            var distance = Vec3.distance(myXform.pos, otherXform.pos);

            var XXX_MIN_DISTANCE = 0.0;
            var XXX_MAX_DISTANCE = REJECT_DISTANCE;
            var XXX_MIN_FREQUENCY = 0.5;
            var XXX_MAX_FREQUENCY = 3.0;

            // convert distance into a value from 0 to 1
            var factor = clamp((distance - XXX_MIN_DISTANCE) / (XXX_MAX_DISTANCE - XXX_MIN_DISTANCE), 0, 1);
            // convert factor into a frequency factor
            var frequency = factor * (XXX_MAX_FREQUENCY - XXX_MIN_FREQUENCY) + XXX_MIN_FREQUENCY;

            var hapticBuddy = this.getHapticBuddy();
            if (hapticBuddy) {
                hapticBuddy.setFrequencyScale(frequency);
                hapticBuddy.update(this.myJointInfo, this.otherJointInfo);
            }
        }

        stopRejectHaptics(): void {
            var hapticBuddy = this.getHapticBuddy();
            if (hapticBuddy) {
                hapticBuddy.stop();
            }
        }

        updateJointInfo(): void {
            this.myJointInfo = scanner.getJointInfo(this.myKey);
            this.otherJointInfo = scanner.getJointInfo(this.otherKey);
        }

        receivedGrab(relXform: Xform): void {
            console.warn("AJT: receivedGrab, relXform = " + JSON.stringify(relXform));

            this.updateJointInfo();
            switch (this.state) {
                case "alone":
                    this.relXform = relXform;
                    this.changeState("follower");
                    break;
                case "leader":
                    // Don't set relXform here, keep using the one we previously computed in triggerPress.
                    this.changeState("peer");
                    break;
                default:
                    console.warn("AJT: WARNING GrabLink.receivedGrab: illegal transition, state = " + this.state);
                    break;
            }
        }

        receivedRelease(): void {

            console.warn("AJT: receivedRelease");

            this.updateJointInfo();
            switch (this.state) {
                case "follower":
                    this.changeState("alone");
                    break;
                case "peer":
                    this.changeState("leader");
                    break;
                default:
                    console.warn("AJT: WARNING GrabLink.receivedRelease: illegal transition, state = " + this.state);
                    break;
            }
        }

        reject(): void {

            console.warn("AJT: reject");

            this.updateJointInfo();
            switch (this.state) {
                case "follower":
                case "peer":
                case "leader":
                    this.changeState("reject");
                    break;
                default:
                    console.warn("AJT: WARNING GrabLink.reject: illegal transition, state = " + this.state);
                    break;
            }
        }

        triggerPress(): void {
            this.updateJointInfo();
            switch (this.state) {
                case "alone":
                    this.changeState("leader");
                    break;
                case "follower":
                    this.changeState("peer");
                    break;
                default:
                    console.warn("AJT: WARNING GrabLink.triggerGrab: illegal transition, state = " + this.state);
                    break;
            }
        }

        triggerRelease(): void {
            this.updateJointInfo();
            switch (this.state) {
                case "leader":
                    this.changeState("alone");
                    break;
                case "peer":
                    this.changeState("follower");
                    break;
                default:
                    console.warn("AJT: WARNING GrabLink.triggerRelease: illegal transition, state = " + this.state);
                    break;
            }
        }

        //
        // alone
        //

        aloneEnter(): void {
            HMD.requestHideHandControllers();
            if (this.state === "leader") {
                // AJT_B: leader -> alone
                this.hapticPulse();
                this.clearPinOnJoint(this.otherKey);
                this.sendReleaseMessage();
                this.enableControllerDispatcher();
            } else if (this.state === "follower") {
                // AJT_H: follower -> alone
                this.hapticPulse();
                this.clearPinOnJoint(this.myKey);
                this.enableControllerDispatcher();
            } else if (this.state === "reject") {
                // do nothing
            } else {
                console.warn("AJT: WARNING aloneEnter from illegal state " + this.state);
            }
        }

        aloneProcess(): void {
            // do nothing
        }

        aloneExit(): void {
            HMD.requestShowHandControllers();
        }

        //
        // follower
        //

        followerEnter(): void {
            this.updateJointInfo();
            this.startRejectHaptics();
            if (this.state === "peer") {
                // AJT_F: peer -> follower
                this.hapticPulse();
                this.clearPinOnJoint(this.otherKey);
                this.sendReleaseMessage();
            } else if (this.state === "alone") {
                // AJT_G: alone -> follower
                this.hapticPulse();
                this.disableControllerDispatcher();
            } else {
                console.warn("AJT: WARNING followerEnter from illegal state " + this.state);
            }
        }

        followerProcess(): void {
            this.updateJointInfo();

            var myXform = new Xform(this.myJointInfo.controllerRot, this.myJointInfo.controllerPos);
            var otherXform = new Xform(this.otherJointInfo.controllerRot, this.otherJointInfo.controllerPos);

            // AJT: TODO: dynamic change of blend factor based on distance?
            var blendFactor = 1;

            // Perform IK on MyAvatar, having our hand follow their controller.
            var myOtherXform = Xform.mul(otherXform, this.relXform);
            var myTargetXform = tweenXform(myXform, myOtherXform, blendFactor);
            this.pinJoint(this.myKey, this.myJointInfo, myTargetXform);
            var distance = Vec3.distance(myXform.pos, myTargetXform.pos);

            this.updateRejectHaptics();

            if (distance > REJECT_DISTANCE) {
                this.sendRejectMessage();
                this.reject();
            }
        }

        followerExit(): void {
            this.stopRejectHaptics();
        }

        //
        // peer
        //

        peerEnter(): void {
            if (this.state === "leader") {
                // AJT_C: leader -> peer
                this.hapticPulse();
            } else if (this.state === "follower") {
                // AJT_E: follower -> peer
                this.hapticPulse();
                this.sendGrabMessage();
            } else {
                console.warn("AJT: WARNING peerEnter from illegal state " + this.state);
            }
            this.startStressHaptics();
        }

        peerProcess(): void {
            this.updateJointInfo();

            var myXform = new Xform(this.myJointInfo.controllerRot, this.myJointInfo.controllerPos);
            var otherXform = new Xform(this.otherJointInfo.controllerRot, this.otherJointInfo.controllerPos);

            // AJT: TODO: dynamic change of blend factor based on distance.
            var blendFactor = 0.5;

            // Perform IK on MyAvatar, having our hand follow their controller.
            var myOtherXform = Xform.mul(otherXform, this.relXform);
            var myTargetXform = tweenXform(myXform, myOtherXform, blendFactor);
            this.pinJoint(this.myKey, this.myJointInfo, myTargetXform);

            // Perform local IK on the other avatar, making their hand follow our controller.
            var otherMyXform = Xform.mul(myXform, this.relXform.inv());
            var otherTargetXform = tweenXform(otherMyXform, otherXform, blendFactor);
            this.pinJoint(this.otherKey, this.otherJointInfo, otherTargetXform);

            this.updateStressHaptics();

            if (Vec3.distance(myXform.pos, myTargetXform.pos) > REJECT_DISTANCE) {
                this.sendRejectMessage();
                this.reject();
            }
        }

        peerExit() {
            this.stopStressHaptics();
        }

        //
        // leader
        //

        leaderEnter(): void {
            this.updateJointInfo();
            this.startRejectHaptics();
            if (this.state === "alone") {
                // AJT_A: alone -> leader
                this.playClap();
                this.hapticPulse();
                this.disableControllerDispatcher();
                this.computeDeltaXform();
                this.sendGrabMessage();
            } else if (this.state === "peer") {
                // AJT_D: peer -> leader
                this.hapticPulse();
                this.clearPinOnJoint(this.myKey);
            } else {
                console.warn("AJT: WARNING: leaderEnter from unknown state " + this.state);
            }
        }

        leaderProcess(): void {
            this.updateJointInfo();

            var myXform = new Xform(this.myJointInfo.controllerRot, this.myJointInfo.controllerPos);
            var otherXform = new Xform(this.otherJointInfo.controllerRot, this.otherJointInfo.controllerPos);

            // AJT: TODO: dynamic change of blend factor based on distance?
            var blendFactor = 0;

            // Perform local IK on the other avatar, making their hand follow our controller.
            var otherMyXform = Xform.mul(myXform, this.relXform.inv());
            var otherTargetXform = tweenXform(otherMyXform, otherXform, blendFactor);
            this.pinJoint(this.otherKey, this.otherJointInfo, otherTargetXform);
            this.updateRejectHaptics();

            // AJT: TODO: should we reject?
        }

        leaderExit(): void {
            this.stopRejectHaptics();
        }

        //
        // reject
        //

        rejectEnter(): void {
            if (this.state === "follower") {
                // AJT_I: follower -> reject
                this.hapticPulse();
                this.clearPinOnJoint(this.myKey);
            } else if (this.state === "peer") {
                // AJT_J: peer -> reject
                this.hapticPulse();
                this.clearPinOnJoint(this.otherKey);
                this.clearPinOnJoint(this.myKey);
            } else if (this.state === "leader") {
                // AJT_K: leader -> reject
                this.hapticPulse();
                this.clearPinOnJoint(this.otherKey);
            } else {
                console.warn("AJT: WARNING: rejectEnter from unknown state " + this.state);
            }
        }

        rejectProcess(): void {
            // immediately go to the alone state
            this.changeState("alone");
        }

        rejectExit(): void {
            // do nothing
        }
    }

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

    class GrabbableJointScanner {
        protected addAvatarCb: (avatarId: string, avatar: AvatarData) => void;
        protected updateAvatarCb: (avatarId: string, avatar: AvatarData) => void;
        protected removeAvatarCb: (avatarId: string) => void;
        protected prevAvatarMap: { [avatarID: string]: { jointInfoMap: JointInfoMap } };

        constructor(addAvatarCb: (avatarId: string, avatar: AvatarData) => void, updateAvatarCb: (avatarId: string, avatar: AvatarData) => void, removeAvatarCb: (avatarId: string) => void) {
            this.addAvatarCb = addAvatarCb;
            this.updateAvatarCb = updateAvatarCb;
            this.removeAvatarCb = removeAvatarCb;
            this.prevAvatarMap = {};
        }

        destroy(): void {
            // remove all avatars
            Object.keys(this.prevAvatarMap).forEach(function (id) {
                if (this.removeAvatarCb) {
                    this.removeAvatarCb(id);
                }
            });
        }

        computeJointInfoMap(avatarId: string): JointInfoMap {
            var avatar = avatarId ? AvatarManager.getAvatar(avatarId) : MyAvatar;
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
                    jointValid: false,
                    palmPos: {x: 0, y: 0, z: 0},
                    palmRot: {x: 0, y: 0, z: 0, w: 1}
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
        }

        scan(): void {
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
        }

        // searches the most recently scanned jointInfoMap for a grabbable joint on other avatar.
        findGrabbableJoint(hand: number, grabDistance: number): GrabKey {
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
        }

        getJointInfo(key: GrabKey): JointInfo {
            if (key) {
                return this.prevAvatarMap[key.avatarId].jointInfoMap[key.jointName];
            }
        }

        getMyHand(hand: number): JointInfo {
            if (hand === RIGHT_HAND) {
                return this.getJointInfo({avatarId: MyAvatar.SELF_ID, jointName: "RightHand"});
            } else {
                return this.getJointInfo({avatarId: MyAvatar.SELF_ID, jointName: "LeftHand"});
            }
        }
    }

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

    var leftHandActiveKeys;

    // Controller system callback on leftTrigger pull or release.
    function leftTrigger(value) {
        var myKey = {avatarId: MyAvatar.SELF_ID, jointName: "LeftHand"};
        var grabLink;
        if (value === 1) {
            var otherKey = scanner.findGrabbableJoint(LEFT_HAND, GRAB_DISTANCE);
            if (otherKey) {
                grabLink = GrabLink.findOrCreateLink(myKey, otherKey);
                grabLink.triggerPress();

                leftHandActiveKeys = {myKey: myKey, otherKey: otherKey};
            }
        } else if (leftHandActiveKeys) {
            grabLink = GrabLink.findLink(leftHandActiveKeys.myKey, leftHandActiveKeys.otherKey);
            if (grabLink) {
                grabLink.triggerRelease();
            } else {
                console.warn("AJT: WARNING, leftTrigger(), could not find gripLink for LeftHand");
            }
            leftHandActiveKeys = undefined;
        }
    }

    var rightHandActiveKeys;

    // Controller system callback on rightTrigger pull or release.
    function rightTrigger(value) {
        var myKey = {avatarId: MyAvatar.SELF_ID, jointName: "RightHand"};
        var grabLink;
        if (value === 1) {
            var otherKey = scanner.findGrabbableJoint(RIGHT_HAND, GRAB_DISTANCE);
            if (otherKey) {
                grabLink = GrabLink.findOrCreateLink(myKey, otherKey);
                grabLink.triggerPress();

                rightHandActiveKeys = {myKey: myKey, otherKey: otherKey};
            }
        } else if (rightHandActiveKeys) {
            grabLink = GrabLink.findLink(rightHandActiveKeys.myKey, rightHandActiveKeys.otherKey);
            if (grabLink) {
                grabLink.triggerRelease();
            } else {
                console.warn("AJT: WARNING, rightTrigger(), could not find gripLink for RightHand");
            }
            rightHandActiveKeys = undefined;
        }
    }

    var connectionMapping = Controller.newMapping('handshake-grip');
    var animationHandlerId;
    var ANIM_VARS = [
        "leftHandPosition",
        "leftHandRotation",
        "leftHandType",
        "rightHandPosition",
        "rightHandRotation",
        "rightHandType"
    ];

    // message should look like:
    // {type: "grab", receiver: uuid, grabbingJoint: string, grabbedJoint: string, relXform: {pos: Vec3, rot: Quat}}
    // {type: "release", receiver: uuid, grabbingJoint: string, grabbedJoint: string}
    function messageHandler(channel, message, sender) {
        if (channel === "Hifi-Handshake") {

            console.warn("AJT: messageHandler, msg = " + message);

            var obj: GrabMessage = JSON.parse(message);
            if (obj.receiver === MyAvatar.sessionUUID) {
                var myKey = {avatarId: MyAvatar.SELF_ID, jointName: obj.grabbedJoint};
                var otherKey = {avatarId: sender, jointName: obj.grabbingJoint};
                var grabLink: GrabLink;
                if (obj.type === "grab") {
                    grabLink = GrabLink.findOrCreateLink(myKey, otherKey);
                    var relXform = new Xform(obj.relXform.rot, obj.relXform.pos);
                    grabLink.receivedGrab(relXform.inv());
                } else if (obj.type === "release") {
                    grabLink = GrabLink.findLink(myKey, otherKey);
                    if (grabLink) {
                        grabLink.receivedRelease();
                    } else {
                        console.warn("AJT: WARNING, messageHandler() release, could not find gripLink for " + obj.grabbingJoint);
                    }
                } else if (obj.type === "reject") {
                    grabLink = GrabLink.findLink(myKey, otherKey);
                    if (grabLink) {
                        grabLink.reject();
                    } else {
                        console.warn("AJT: WARNING, messageHandler() reject, could not find gripLink for " + obj.grabbedJoint);
                    }
                }
            } else {
                // AJT: TODO: two other avatars are grabbing each other.
            }
        }
    }

    function init() {

        rightHandPose = Controller.getPoseValue(Controller.Standard.RightHand);
        leftHandPose = Controller.getPoseValue(Controller.Standard.LeftHand);

        connectionMapping.from(Controller.Standard.RTClick).peek().to(rightTrigger);
        connectionMapping.from(Controller.Standard.LTClick).peek().to(leftTrigger);
        connectionMapping.enable();

        animationHandlerId = MyAvatar.addAnimationStateHandler(function (props) {
            var result: AnimVars = {};

            if (rightHandPose.valid) {
                result.rightHandType = 0; // RotationAndPosition
            } else {
                result.rightHandType = 3; // HipsRelativeRotationAndPosition
            }

            if (leftHandPose.valid) {
                result.leftHandType = 0; // RotationAndPosition
            } else {
                result.leftHandType = 3; // HipsRelativeRotationAndPosition
            }

            var avatarXform = new Xform(Quat.multiply(MyAvatar.orientation, QUAT_Y_180), MyAvatar.position);

            if (myAvatarRightHandXform) {
                var rightHandTargetXform = Xform.mul(avatarXform.inv(), myAvatarRightHandXform);
                result.rightHandRotation = rightHandTargetXform.rot;
                result.rightHandPosition = rightHandTargetXform.pos;
            } else {
                delete result.rightHandRotation;
                delete result.rightHandPosition;
            }

            if (myAvatarLeftHandXform) {
                var leftHandTargetXform = Xform.mul(avatarXform.inv(), myAvatarLeftHandXform);
                result.leftHandRotation = leftHandTargetXform.rot;
                result.leftHandPosition = leftHandTargetXform.pos;
            } else {
                delete result.leftHandRotation;
                delete result.leftHandPosition;
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
        console.warn("addAvatar(" + id + ")");
        console.warn("typeof id = " + (typeof id));
        console.warn("JSON.stringify(id) = " + JSON.stringify(id));
        if (id === "null") {
            id = MyAvatar.SELF_ID;
        }
        console.warn("addAvatar(" + id + ")");
        updateAvatar(id, data);
    }

    function updateAvatar(id, data) {
    }

    function removeAvatar(id) {
        console.warn("removeAvatar(" + id + ")");
    }

    var scanner = new GrabbableJointScanner(addAvatar, updateAvatar, removeAvatar);

    var leftHandPose;
    var rightHandPose;

    function update(dt) {

        // update hand poses
        rightHandPose = Controller.getPoseValue(Controller.Standard.RightHand);
        leftHandPose = Controller.getPoseValue(Controller.Standard.LeftHand);

        scanner.scan();
        GrabLink.process(dt);
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
