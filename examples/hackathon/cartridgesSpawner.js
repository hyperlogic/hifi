Script.include("../libraries/utils.js");

var orientation = Camera.getOrientation();
orientation = Quat.safeEulerAngles(orientation);
orientation.x = 0;
orientation = Quat.fromVec3Degrees(orientation);
var SPHERE_RADIUS = 1.5;
var cartridgeBasePosition = Vec3.sum(MyAvatar.getHeadPosition(), Vec3.multiply(SPHERE_RADIUS + 0.15, Quat.getFront(orientation)));
var cartridges = [];

var clipURLS = ["https://s3-us-west-1.amazonaws.com/hifi-content/eric/Sounds/hackathonSounds/chords.wav",
    "https://s3-us-west-1.amazonaws.com/hifi-content/eric/Sounds/hackathonSounds/bells.wav",
    "https://s3-us-west-1.amazonaws.com/hifi-content/eric/Sounds/hackathonSounds/piano.wav?v1",
    "https://s3-us-west-1.amazonaws.com/hifi-content/eric/Sounds/hackathonSounds/voice.wav",
    "https://s3-us-west-1.amazonaws.com/hifi-content/eric/Sounds/hackathonSounds/beat.wav"
];

function spawnCartridges() {
    var SCRIPT_URL = Script.resolvePath("soundCartridgeEntityScript.js?v1" + Math.random());
    var cartridgeProps = {
        type: "Box",
        name: "Sound Cartridge",
        position: cartridgeBasePosition,
        script: SCRIPT_URL,
        dynamic: true,
        dimensions: {
            x: 0.1,
            y: 0.1,
            z: 0.1
        },
        damping: 1,
        angularDamping: 1,
    };

    for (var i = 0; i < clipURLS.length; i++) {
        cartridgeProps.position.x = cartridgeBasePosition.x + randFloat(-0.7, 0.7);
        cartridgeProps.userData = JSON.stringify({
            soundURL: clipURLS[i]
        });
        cartridgeProps.color = {
            red: 200,
            green: 10,
            blue: 10
        };
        var cartridge = Entities.addEntity(cartridgeProps);
        cartridges.push(cartridge);
    }
}

spawnCartridges();

function cleanup() {
    cartridges.forEach(function(cartridge) {
        Entities.deleteEntity(cartridge);
    });
}

Script.scriptEnding.connect(cleanup);