var zone = Entities.addEntity({
    type: "Zone",
    name: "Epic SkyBox",
    position: MyAvatar.position,
    dimensions: {
        x: 10,
        y: 10,
        z: 10
    },
    shapeType: "sphere",
    backgroundMode: "skybox",
    skybox: {
        url: "https://hifi-public.s3.amazonaws.com/images/SkyboxTextures/CloudyDay1.jpg"
    },
    userData: JSON.stringify({

        "ProceduralEntity": {
            "version": 2,
            "shaderUrl": "file:///C:/Users/Eric/hifi/examples/hackathon/skybox.fs?v1" + Math.random(),
            "channels": [
                "https://hifi-public.s3.amazonaws.com/austin/assets/images/skybox/starmap_8k.jpg",
            ],
            "uniforms": {
                "red": 0.9
            }
        }
    })
});



function cleanup() {
    Entities.deleteEntity(zone);
}

Script.scriptEnding.connect(cleanup);