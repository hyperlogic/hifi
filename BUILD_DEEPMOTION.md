Download the deep motion libs from [here](https://drive.google.com/open?id=1ZdJzPLZFVmjB3uswm0woCGN3T8u1axc8)
Unzip the deepmotion libs/dll into local folder on your pc.
Then when running cmake add the -D flag to specify this location.

`cmake .. -G "Visual Studio 15 Win64" -DDEEPMOTION_DLL_PATH=c:/deepmotion`

Deepmotion only works with the schoolboy avatar, to activate switch to this avatar:
https://s3-us-west-1.amazonaws.com/hifi-content/Avatars/deepmotion/SchoolBoy_Rig_Lambert_JointOrientation_Rename.fst

Use the scripts included with trackers.zip to manipulate each point of tracking via "create mode" or using the mouse in desktop mode.

