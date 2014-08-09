#include "KinServer.h"

#define VERSION __DATE__

using namespace cv;

#define KEY_DOWN(vk_code) ::GetAsyncKeyState(vk_code) & 0x8000

static bool isKeyToggle(int vk, bool* keyStatus = NULL)
{
    //
    static bool isPrevKeyDown[256];
    BOOL k = KEY_DOWN(vk);
    if (k && !isPrevKeyDown[vk]){
        isPrevKeyDown[vk] = true;
        if (keyStatus != NULL)
            *keyStatus =! *keyStatus;//key status needs update
        return true;
    }
    if (!k){
        isPrevKeyDown[vk] = false;
    }
    return false;
}

bool showAllWindow = true;

const int width = 800;
const int height = 600;

bool loop = true;

int new_angle = 0;
bool motor_changed = true;
void onAngleChange(int pos, void* userdata)
{
    motor_changed = true;
}

void onGameModeChanged(int pos, void* userdata)
{
    if (userdata)
    {
        KinServer* dev = static_cast<KinServer*>(userdata);
        dev->updatePlayMode();
    }
}

const char* DEPTH_WINDOW = "depth_view";
const char* PARAM_WINDOW = "param_panel";
const char* RGB_WINDOW = "color_view";
const char* SKELETON_WINDOW ="skeleton_view";
const char* BLOB_WINDOW = "blob_view";
const char* BG_WINDOW = "bg_view";

void createMainWindow(const KinectOption& param) 
{
    if (param.color)
        namedWindow(RGB_WINDOW);
    if (param.depth)
        namedWindow(DEPTH_WINDOW);
    if (param.skeleton)
        namedWindow(SKELETON_WINDOW);

    if (param.contains(KinectOption::PATT_CAMSERVER))
        namedWindow(BLOB_WINDOW);
    if (param.contains(KinectOption::PATT_CCV))
        namedWindow(BG_WINDOW);
}

void destroyMainWindow(const KinectOption& param) 
{
    if (param.color)
        destroyWindow(RGB_WINDOW);
    if (param.depth)
        destroyWindow(DEPTH_WINDOW);
    if (param.skeleton)
        destroyWindow(SKELETON_WINDOW);
    if (param.contains(KinectOption::PATT_CAMSERVER))
        destroyWindow(BLOB_WINDOW);
    if (param.contains(KinectOption::PATT_CCV))
        destroyWindow(BG_WINDOW);
}

void createParamWindow(const KinectOption& param, KinServer& device)
{
    namedWindow(PARAM_WINDOW);
    resizeWindow(PARAM_WINDOW, 400,400);
    createTrackbar("angle", PARAM_WINDOW, &new_angle, -NUI_CAMERA_ELEVATION_MINIMUM+NUI_CAMERA_ELEVATION_MAXIMUM, onAngleChange);

    if (param.contains(KinectOption::PATT_HAND_TUIO))
    {
        createTrackbar("n_players", PARAM_WINDOW, &device.n_players, 2, onGameModeChanged, &device);
    }
    else if (param.contains(KinectOption::PATT_CAMSERVER))
    {
        createTrackbar("distance_mm", PARAM_WINDOW, &device.z_threshold_mm, 100);
        createTrackbar("min_area", PARAM_WINDOW, &device.min_area, 200);
        createTrackbar("blur", PARAM_WINDOW, &device.open_param, 3);
    }
}

bool fruit_ninja = false;

int main(int argc, const char** argv)
{
    printf("\nKinServer %s\n\nvinjn.z@gmail.com.\n\nhttp://vinjn.github.io/\n\n", VERSION);

    SetThreadName(-1, "main");

    const char *keys =
    {    
        "{help h usage ?|false      |display help info}"
        "{fruit_ninja   |false      |fruit_ninja mode is special}"
        "{device        |0          |the kinect device to use, default is 0}"
        "{client        |localhost  |the client's ip}"
        "{tuio          |-1         |the tuio port that client is listening at}"
        "{osc           |7777       |the osc port that client is listening at}"
        "{color         |false      |use color stream}"
        "{depth         |true       |use depth stream}" 
        "{skeleton      |true       |use skeleton stream}"
        "{pattern       |hand       |running pattern, available options are hand/body/ccv/cam_server/jointed_blob}"
        "{minim         |0          |default mimized window}"
    };
    CommandLineParser args(argc, argv, keys);
    if (args.has("help"))
    {
        args.printMessage();
        return 0;
    }

    int device_id = args.get<int>("device"); 

    showAllWindow = !args.get<int>("minim"); 

    KinectOption the_param(args);
    KinServer device(device_id, the_param);

    if (!device.setup())
        return -1;

    fruit_ninja = args.has("fruit_ninja");
    if (fruit_ninja)
    {
        device.osc_enabled = false;
        printf("\n\nFRUIT NINJA MODE!\nPress F4 after entering game to start osc message.\nPress ESC to pause osc message.\n\n");
    }

    new_angle = device.getDeviceAngle();

    if (showAllWindow)
        createMainWindow(the_param);
    createParamWindow(the_param, device);

    int safe_time = timeGetTime();
    int start_time = timeGetTime();
    bool depth_bg_initialized = false;

    while (loop)
    {
        if (fruit_ninja)
        {
            if (isKeyToggle(VK_F4))
            {
                device.osc_enabled = true;
                printf("OSC ON\n");
                ::Beep(1000,100);
            }
            else if (isKeyToggle(VK_ESCAPE))
            {
                device.osc_enabled = false;
                printf("OSC OFF\n");
            }
        }

        if (timeGetTime() - safe_time > 2000 && motor_changed)
        {	
            //Never change the camera angle more than once per second or
            //more than 15 times in any 20-second period
            int curr_angle = device.getDeviceAngle();
            if (curr_angle != new_angle)
                device.setDeviceAngle(new_angle);
            safe_time = timeGetTime();
            motor_changed = false;
        }

        int key = waitKey(1);
        if (key == VK_ESCAPE)
            loop = false;
        if (key == VK_RETURN)
        {
            the_param.dump = !the_param.dump;
            printf("DUMP MODE: %s\n", the_param.dump ? "ON" : "OFF");
        }

        if (key == VK_SPACE)
        {
            showAllWindow = !showAllWindow;
            destroyMainWindow(the_param);
            if (showAllWindow)
            {
                createMainWindow(the_param);
            }
        }

        if ((!depth_bg_initialized && timeGetTime() - start_time > 2000 && device.checkNewFrame(FRAME_DEPTH_U16))
            || key == 'b')
        {
            depth_bg_initialized = true;
            device.updateBg();
        }

        if (showAllWindow)
        {
            if (the_param.color)
                imshow(RGB_WINDOW, device.getFrame(FRAME_COLOR_U8C3));
            if (the_param.depth)
                imshow(DEPTH_WINDOW, device.getFrame(FRAME_DEPTH_U16));
            if (the_param.skeleton)
                imshow(SKELETON_WINDOW, device.getFrame(FRAME_SKELETON_U8C3));

            if (the_param.contains(KinectOption::PATT_CAMSERVER))
            {
                ScopedLocker l(device.mtx_depth_data_view);
                imshow(BLOB_WINDOW, device.blob_view);
            }
            if (the_param.contains(KinectOption::PATT_CCV))
                imshow(BG_WINDOW, device.depth_bg);
        }
    }
    return 0;
}
