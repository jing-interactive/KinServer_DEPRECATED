#include "KinServer.h"

#define VERSION __DATE__

using namespace cv;

#define KEY_DOWN(vk_code) ::GetAsyncKeyState(vk_code) & 0x8000

int selectedCornerId= -1;
Ptr<KinServer> devicePtr;

void onCCVMouse(int event, int x, int y, int flags, void* param)
{
    const int kBoundary = 2000;
    if (x < 0 || x > kBoundary) x = 0;
    if (y < 0 || y > kBoundary) y = 0;

    if (event == EVENT_LBUTTONUP || !(flags & EVENT_FLAG_LBUTTON))
    {
        selectedCornerId = -1;
        return;
    }

    if (event == EVENT_LBUTTONDOWN)
    {
        selectedCornerId = -1;

        for (int i=0;i<KinServer::CORNER_COUNT;i++)
        {
            Point* pt = &devicePtr->corners[i];
            const int kThreshold = 15;
            if (abs(pt->x - x) < kThreshold && abs(pt->y - y) < kThreshold)
            {
                selectedCornerId = i;
                break;
            }
        }
        return;
    }

    if (event == EVENT_MOUSEMOVE && (flags & EVENT_FLAG_LBUTTON))
    {
        if (selectedCornerId != -1)
        {
            if (selectedCornerId < KinServer::CORNER_OUT_LT)
            {
                x = max<int>(x, devicePtr->depthOrigin.x);
                y = max<int>(y, devicePtr->depthOrigin.y);
                x = min<int>(x, devicePtr->depthOrigin.x + DEPTH_WIDTH);
                y = min<int>(y, devicePtr->depthOrigin.y + DEPTH_HEIGHT);
            }
            devicePtr->corners[selectedCornerId] = Point(x, y);
        }
        return;
    }
}

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

const char* DEPTH_WINDOW = "depth";
const char* PARAM_WINDOW = "param_panel";
const char* RGB_WINDOW = "color";
const char* SKELETON_WINDOW ="skeleton";
const char* BLOB_WINDOW = "tracking";
const char* CCV_BG_WINDOW = "CCV_back";
const char* CCV_WINDOW = "CCV";

void createMainWindow(const KinectOption& param) 
{
    if (param.contains(KinectOption::PATT_CCV))
    {
        namedWindow(CCV_WINDOW);
        namedWindow(CCV_BG_WINDOW);
        namedWindow(DEPTH_WINDOW);
        return;
    }
    if (param.color)
        namedWindow(RGB_WINDOW);
    if (param.skeleton)
        namedWindow(SKELETON_WINDOW);
    if (param.depth)
        namedWindow(DEPTH_WINDOW);
    if (param.contains(KinectOption::PATT_CAMSERVER))
        namedWindow(BLOB_WINDOW);
}

void destroyMainWindow(const KinectOption& param) 
{
    if (param.contains(KinectOption::PATT_CCV))
    {
        destroyWindow(CCV_WINDOW);
        destroyWindow(CCV_BG_WINDOW);
        destroyWindow(DEPTH_WINDOW);
        return;
    }
    if (param.color)
        destroyWindow(RGB_WINDOW);
    if (param.depth)
        destroyWindow(DEPTH_WINDOW);
    if (param.skeleton)
        destroyWindow(SKELETON_WINDOW);
    if (param.contains(KinectOption::PATT_CAMSERVER))
        destroyWindow(BLOB_WINDOW);
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
        createTrackbar("distance", PARAM_WINDOW, &device.z_threshold_mm, 100);
        createTrackbar("size", PARAM_WINDOW, &device.min_area, 200);
        createTrackbar("blur", PARAM_WINDOW, &device.open_param, 3);
    }
}

static bool fruit_ninja = false;

static void drawBoundingBox(Mat frame, int lt, int rb, Scalar clrRect, Scalar clrCircle)
{
    const int kThickness = 2;
    rectangle(frame, devicePtr->corners[lt], 
        devicePtr->corners[rb], clrRect, kThickness);
    circle(frame, devicePtr->corners[lt], 10, clrCircle, kThickness);
    circle(frame, devicePtr->corners[rb], 10, clrCircle, kThickness);
}

static void drawBlobId(Mat frame)
{
    const vBlobTracker& blobTracker = devicePtr->blobTracker;
    for (int i=0;i<blobTracker.trackedBlobs.size();i++)
    {	
        const vTrackedBlob& blob = blobTracker.trackedBlobs[i];
        Point center(blob.center.x + devicePtr->depthOrigin.x, blob.center.y + devicePtr->depthOrigin.y);
        // TODO: roi skip outside joints
        if (center.x <= devicePtr->corners[KinServer::CORNER_DEPTH_LT].x
            || center.x >= devicePtr->corners[KinServer::CORNER_DEPTH_RB].x
            || center.y <= devicePtr->corners[KinServer::CORNER_DEPTH_LT].y
            || center.y >= devicePtr->corners[KinServer::CORNER_DEPTH_RB].y
            )
            continue;

        circle(frame, center, 10, CV_WHITE);
        char info[100];
        sprintf(info, "# %d", blob.id);
        vDrawText(frame, center.x + 5, center.y + 5, info);
    }
}

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
    devicePtr = new KinServer(device_id, the_param);
    KinServer& device = *devicePtr;

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
    setMouseCallback(CCV_WINDOW, onCCVMouse);

    int safe_time = timeGetTime();
    int start_time = timeGetTime();
    bool depth_bg_initialized = false;

    Mat3b ccvFrame(DEPTH_HEIGHT*1.5, DEPTH_WIDTH*1.5);

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
            if (the_param.contains(KinectOption::PATT_CCV))
            {
                ccvFrame.setTo(Scalar::all(195));
                {
                    ScopedLocker l(device.mtx_depth_data_view);
                    vFastCopyImageTo(device.blob_view, ccvFrame, Rect(device.depthOrigin, Size(DEPTH_WIDTH, DEPTH_HEIGHT)));
                }
                drawBoundingBox(ccvFrame, KinServer::CORNER_DEPTH_LT, KinServer::CORNER_DEPTH_RB, rgbColor(144, 80, 81), rgbColor(0, 255, 0));
                drawBoundingBox(ccvFrame, KinServer::CORNER_OUT_LT, KinServer::CORNER_OUT_RB, rgbColor(24, 115, 0), rgbColor(0, 104, 183));
                if (selectedCornerId != -1) 
                {
                    circle(ccvFrame, device.corners[selectedCornerId], 15, CV_RED);
                }

                drawBlobId(ccvFrame);

                imshow(CCV_WINDOW, ccvFrame);
                imshow(CCV_BG_WINDOW, device.depth_bg);
                imshow(DEPTH_WINDOW, device.getFrame(FRAME_DEPTH_U16));
            }
            else
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
            }
        }
    }
    return 0;
}
