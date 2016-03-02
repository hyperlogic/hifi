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
            "shaderUrl": "https://hifi-public.s3.amazonaws.com/austin/shaders/skybox/omgSkybox.fs",
            "channels": [
                "https://hifi-public.s3.amazonaws.com/austin/assets/images/skybox/starmap_8k.jpg",
                "https://hifi-public.s3.amazonaws.com/austin/assets/images/skybox/celestial_grid.jpg",
                "https://hifi-public.s3.amazonaws.com/austin/assets/images/skybox/constellation_figures.jpg",
                "https://hifi-public.s3.amazonaws.com/austin/assets/images/skybox/constellation_boundaries.jpg"
            ],
            "uniforms": {
                "rotationSpeed": 0.001,
                "uDayColor": [0.9, 0.7, 0.7],
                "constellationLevel": 0.01,
                "constellationBoundaryLevel": 0.01,
                "gridLevel": 0.00
            }
        }
    })
});



function cleanup() {
    Entities.deleteEntity(zone);
}

Script.scriptEnding.connect(cleanup);