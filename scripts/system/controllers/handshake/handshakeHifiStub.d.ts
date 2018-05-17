declare namespace Script {
    function include(filename: string, callback?: Function): void;
    var update: Signal;
    var scriptEnding: Signal;
}

declare namespace Tablet {
    function getTablet(name: string): TabletProxy;
}

declare class TabletProxy {
    addButton(properties: object): TabletButtonProxy;
    removeButton(button: TabletButtonProxy);
    gotoWebScreen(url: string, injectedJavaScriptUrl?: string, loadOtherBase?: boolean): void;
    gotoHomeScreen(): void;
    emitScriptEvent(msg: any): void;
    screenChanged: Signal;
    webEventReceived: Signal;
}

declare class Signal {
    connect(callback: Function): void;
    disconnect(callback: Function): void;
}

declare class TabletButtonProxy {
    editProperties(properties: object): void;
    clicked: Signal;
}

interface StandardDevices {
    LeftHand: number;
    RightHand: number;
    LTClick: number;
    RTClick: number;
}

declare namespace Controller {
    function triggerHapticPulse(strength: number, duration: number, hand: number): void;
    function newMapping(mappingName: string): MappingObject;
    function getPoseValue(device: number): ControllerPose;
    var Standard: StandardDevices;
}

declare class ControllerPose {
    
}

declare class MappingObject {
    from(action: number): MappingObject;
    peek(): MappingObject;
    to(callback: Function): MappingObject;
    enable(): void;
    disable(): void;
}

interface vec3 {
    x: number;
    y: number;
    z: number;
}

interface quat {
    x: number;
    y: number;
    z: number;
    w: number;  
}

interface mat4 {
    r0c0: number;
    r1c0: number;
    r2c0: number;
    r3c0: number;
    r0c1: number;
    r1c1: number;
    r2c1: number;
    r3c1: number;
    r0c2: number;
    r1c2: number;
    r2c2: number;
    r3c2: number;
    r0c3: number;
    r1c3: number;
    r2c3: number;
    r3c3: number;
}

declare namespace Vec3 {
    function distance(a: vec3, b: vec3): number;
    function sum(a: vec3, b: vec3): vec3;
    function subtract(a: vec3, b: vec3): vec3;
    function multiply(a: vec3, b: number): vec3;
    function multiply(a: number, b: vec3): vec3;
}

declare namespace Quat {
    function mix(a: quat, b: quat, alpha: number): quat;
    function multiply(a: quat, b: quat): quat;
}

declare namespace Mat4 {
    function extractRotation(m: mat4): quat;
    function extractTranslation(m: mat4): vec3;
}

declare class AudioResource {
    downloaded: boolean;
}

declare namespace SoundCache {
    function getSound(url: string): AudioResource;
}

declare namespace Audio {
    function playSound(sound: object, params?: object): AudioInjector;
}

interface InjectorOptions {
    position?: vec3;
    volume?: number;
    loop?: boolean;
    orientation?: quat;
    ignorePenumbra?: boolean;
    localOnly?: boolean;
    secondOffset?: boolean;
    pitch?: boolean;    
}

declare class AudioInjector {
    finished: Signal;
    setOptions(options: InjectorOptions): void;
    restart(): void;
}

declare var print: (msg: string) => void;

declare class Xform {
    constructor(rot: quat, pos: vec3);
    static mul(lhs: Xform, rhs: Xform): Xform;
    inv(): Xform;
    xformPoint(point: vec3): vec3;
    rot: quat;
    pos: vec3;
}

declare namespace AvatarManager {
    function getAvatar(avatarID: string): AvatarData;
    function getAvatarsInRange(position: vec3, radius: number): string[];
}

declare class AvatarData {
    readonly SELF_ID: string;
    readonly sessionUUID: string;
    position: vec3;
    orientation: quat;
    controllerLeftHandMatrix: mat4;
    controllerRightHandMatrix: mat4;
    clearPinOnJoint(index: number): void;
    pinJoint(index: number, pos: vec3, rot: quat): void;
    getJointIndex(jointName: string): number;
    getAbsoluteJointRotationInObjectFrame(index: number): quat;
    getAbsoluteJointTranslationInObjectFrame(index: number): vec3;
    getLeftPalmPosition(): vec3;
    getLeftPalmRotation(): quat;
    getRightPalmPosition(): vec3;
    getRightPalmRotation(): quat;
    addAnimationStateHandler(callback: (obj: AnimVars) => AnimVars, keys: string[]): Function;
    removeAnimationStateHandler(id: Function): void;
}

declare namespace Messages {
    function sendMessage(channel: string, message: string, localOnly?: boolean): void;
    function subscribe(channel: string): void;
    var messageReceived: Signal;
}

declare namespace HMD {
    function requestHideHandControllers(): void;
    function requestShowHandControllers(): void;
}

declare var MyAvatar: AvatarData;

declare interface AnimVars {
    rightHandType?: number;
    leftHandType?: number;
    rightHandRotation?: quat;
    rightHandPosition?: vec3;
    leftHandRotation?: quat;
    leftHandPosition?: vec3;
}

declare var console: {
    log(msg: string);
    warn(msg: string);
    error(msg: string);
}