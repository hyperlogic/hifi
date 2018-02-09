/* eslint-env jasmine */

// Art3mis
// eslint-disable-next-line max-len
var DEFAULT_AVATAR_URL = "https://hifi-metaverse.s3-us-west-1.amazonaws.com/marketplace/contents/e76946cc-c272-4adf-9bb6-02cde0a4b57d/8fd984ea6fe1495147a3303f87fa6e23.fst?1460131758";

var ORIGIN = {x: 0, y: 0, z: 0};
var ONE_HUNDRED = {x: 100, y: 100, z: 100};
var ROT_IDENT = {x: 0, y: 0, z: 0, w: 1};

describe("MyAvatar", function () {

    // backup/restore current skeletonModelURL
    beforeAll(function() {
        this.oldURL = MyAvatar.skeletonModelURL;
    });

    afterAll(function() {
        MyAvatar.skeletonModelURL = this.oldURL;
    });

    // reload the avatar from scratch before each test.
    beforeEach(function (done) {
        MyAvatar.skeletonModelURL = DEFAULT_AVATAR_URL;

        // wait until we are finished loading
        var id = Script.setInterval(function () {
            if (MyAvatar.jointNames.length == 72) {
                // assume we are finished loading.
                Script.clearInterval(id);
                MyAvatar.position = ORIGIN;
                MyAvatar.orientation = ROT_IDENT;
                // give the avatar 1/2 a second to settle on the ground in the idle pose.
                Script.setTimeout(function () {
                    done();
                }, 500);
            }
        }, 500);
    }, 10000 /* timeout -- allow time to download avatar*/);

    // makes the assumption that there is solid ground somewhat underneath the avatar.
    it("position and orientation getters", function () {
        var pos = MyAvatar.position;

        expect(Math.abs(pos.x)).toBeLessThan(0.1);
        expect(Math.abs(pos.y)).toBeLessThan(1.0);
        expect(Math.abs(pos.z)).toBeLessThan(0.1);

        var rot = MyAvatar.orientation;
        expect(Math.abs(rot.x)).toBeLessThan(0.01);
        expect(Math.abs(rot.y)).toBeLessThan(0.01);
        expect(Math.abs(rot.z)).toBeLessThan(0.01);
        expect(Math.abs(1 - rot.w)).toBeLessThan(0.01);
    });

    it("position and orientation setters", function (done) {
        MyAvatar.position = ONE_HUNDRED;
        Script.setTimeout(function () {
            expect(Vec3.length(Vec3.subtract(MyAvatar.position, ONE_HUNDRED))).toBeLessThan(0.1);
            done();
        }, 100);
    });

});

// ----------------------------------------------------------------------------
// this stuff allows the unit tests to be loaded indepenently and/or as part of testRunner.js execution
function run() {}
function instrument_testrunner(force) {
    if (force || typeof describe === 'undefined') {
        var oldPrint = print;
        window = new OverlayWebWindow({
            title: 'assetUnitTests.js',
            width: 640,
            height: 480,
            source: 'about:blank',
        });
        Script.scriptEnding.connect(window, 'close');
        window.closed.connect(Script, 'stop');
        // wait for window (ready) and test runner (ran) both to initialize
        var ready = false;
        var ran = false;
        window.webEventReceived.connect(function() { ready = true; maybeRun(); });
        run = function() { ran = true; maybeRun(); };

        window.setURL([
            'data:text/html;text,',
            '<body style="height:100%;width:100%;background:#eee;whitespace:pre;font-size:8px;">',
            '<pre id=output></pre><div style="height:2px;"></div>',
            '<body>',
            '<script>('+function(){
                window.addEventListener("DOMContentLoaded",function(){
                    setTimeout(function() {
                        EventBridge.scriptEventReceived.connect(function(msg){
                            output.innerHTML += msg;
                            window.scrollTo(0, 1e10);
                            document.body.scrollTop = 1e10;
                        });
                        EventBridge.emitWebEvent('ready');
                    }, 1000);
                });
            }+')();</script>',
        ].join('\n'));
        print = function() {
            var str = [].slice.call(arguments).join(' ');
            if (ready) {
                window.emitScriptEvent(str + '\n');
            } else {
                oldPrint('!ready', str);
            }
        };

        // Include testing library
        Script.include('../../libraries/jasmine/jasmine.js');
        Script.include('../../libraries/jasmine/hifi-boot.js');

        function maybeRun() {
            if (!ran || !ready) {
                return oldPrint('doRun -- waiting for both script and web window to become available');
            }
            if (!force) {
                // invoke Script.stop (after any async tests complete)
                jasmine.getEnv().addReporter({ jasmineDone: Script.stop });
            } else {
                jasmine.getEnv().addReporter({ jasmineDone: function() { print("JASMINE DONE"); } });
            }

            // Run the tests
            jasmine.getEnv().execute();
        };
    }
}
run();
