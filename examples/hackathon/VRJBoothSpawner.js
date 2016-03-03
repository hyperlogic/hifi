// Spawns a booth  and a bunch of special sound cartridges that a user can grab and place close to them.
// Once cartridges are inside sphere, they will listen for Entities.callEntityMethod from client and play their sound accordingly
// cartriges should be as stateless as possible
Script.include("../libraries/utils.js");
var orientation = Camera.getOrientation();
orientation = Quat.safeEulerAngles(orientation);
orientation.x = 0;
orientation = Quat.fromVec3Degrees(orientation);

var SPHERE_RADIUS = 1;
var CARTRIDGE_SEARCH_HZ = 300;
var CARTRIDGE_PARAM_UPDATE_HZ = 50;

var activeCartridges = [];
var rightHandPosition, leftHandPosition;

var SPHERE_POSITION = MyAvatar.position;
var sphereOverlay = Overlays.addOverlay('sphere', {
    size: SPHERE_RADIUS * 2,
    position: SPHERE_POSITION,
    color: {
        red: 200,
        green: 10,
        blue: 10
    },
    alpha: 0.2,
    solid: true,
    visible: true
});


function updateCartridgeParams() {
    // go through our active list...
    print("UPDATE ")
}


function cartridgeSearch() {
    // Get all entities in search sphere and for the ones that have soundURLs and are cartridges, call their entityMethod with appropriate data
    //DOing it with entity scripts makes it more scalable to multiuser since a friend can come grab cartriges from you
    // Folks can create thier own loops and share them amongst each other.
    // True multiuser DJing

    var entities = Entities.findEntities(SPHERE_POSITION, SPHERE_RADIUS);
    entities.forEach(function(entity) {
        var name = Entities.getEntityProperties(entity, "name").name;
        var userData = getEntityUserData(entity);

        if (name === "Sound Cartridge" && userData.soundURL) {
            //We have a cartridge- play it
            if (!cartridgeInActiveList(entity)) {
                Entities.callEntityMethod(entity, "playSound");
                activeCartridges.push(entity);
            } else {
                // cartridge is 
            }
        }

    });

    removeOutOfRangeCartridgesFromActiveList();

}

function cartridgeInActiveList(cartridgeToCheck) {
    // Check to see if specified cartrige is in this active list
    for (var i = 0; i < activeCartridges.length; i++) {
        if (entitiesEqual(activeCartridges[i], cartridgeToCheck)) {
            return true;
        }
    }
    return false;
}

function cartridgeIsInRange(cartridge) {
    var cartridgePosition = Entities.getEntityProperties(cartridge, "position").position;
    var distance = Vec3.distance(cartridgePosition, SPHERE_POSITION).toFixed(2);
    if (distance > SPHERE_RADIUS) {
        return false;
    }

    return true;

}

function removeOutOfRangeCartridgesFromActiveList() {
    var cartridgeIndicesToRemove = [];
    for (var i = 0; i < activeCartridges.length; i++) {
        var activeCartridge = activeCartridges[i];
        if (!cartridgeIsInRange(activeCartridge)) {
            cartridgeIndicesToRemove.push(i);
            Entities.callEntityMethod(activeCartridge, "stopSound");
        }
    }

    cartridgeIndicesToRemove.forEach(function(cartridgeIndex) {
           activeCartridges.splice(cartridgeIndex, 1);
            print("EBL SPLICE OUT OF RANGE CLIP!")
    });

}

function entitiesEqual(entityA, entityB) {
    if (!entityA || !entityB) {
        print("ONE OR BOTH OF THESE ENTITIES ARE UNDEFINED");
        return false;
    }
    var isEqual = JSON.stringify(entityA) === JSON.stringify(entityB) ? true : false;
    return isEqual;
}

function cleanup() {
    Overlays.deleteOverlay(sphereOverlay);
}


Script.setInterval(cartridgeSearch, CARTRIDGE_SEARCH_HZ);
Script.setInterval(updateCartridgeParams, CARTRIDGE_PARAM_UPDATE_HZ);
Script.scriptEnding.connect(cleanup);