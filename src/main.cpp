#include "KinServer.h"

#define VERSION __DATE__

#define KEY_DOWN(vk_code) ::GetAsyncKeyState(vk_code) & 0x8000

inline bool isKeyToggle(int vk, bool* keyStatus = NULL){
	//
	static bool isPrevKeyDown[256];
	bool k = KEY_DOWN(vk);
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

#define PORT_RECV 8888

struct StartThread: public MiniThread
{
	StartThread()
	{
		printf("\nKinServer %s\n\nvinjn.z@gmail.com.\n\nhttp://weibo.com/vinjnmelanie\n\n", VERSION);
	}

	void threadedFunction()
	{
		for (int b=0;b<8;b++)
		{
			::Beep(sin(b/20.0f*3.14)*300,100);
		}
	}
};

bool showAllWindow = true;

const int width = 800;
const int height = 600;

bool loop = true;

int new_angle = 0;
bool motor_changed = true;
void onAngleChange(int pos)
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
const char* FINGER_WINDOW = "finger_view";
const char* BLOB_WINDOW = "blob_view";

void createMainWindow(const KinectParam& param) 
{
	if (param.color)
		cvNamedWindow(RGB_WINDOW);
	if (param.depth)
		cvNamedWindow(DEPTH_WINDOW);
	if (param.skeleton)
		cvNamedWindow(SKELETON_WINDOW);
	if (param.contains(KinectParam::PATT_HAND_GESTRUE_FINGER/* ||param.pattern == KinectParam::PATT_HAND*/))
		cvNamedWindow(FINGER_WINDOW);
	else if (param.contains(KinectParam::PATT_BLOB))
		cvNamedWindow(BLOB_WINDOW);
}

void destroyMainWindow(const KinectParam& param) 
{
	if (param.color)
		cvDestroyWindow(RGB_WINDOW);
	if (param.depth)
		cvDestroyWindow(DEPTH_WINDOW);
	if (param.skeleton)
		cvDestroyWindow(SKELETON_WINDOW);
	if (param.contains(KinectParam::PATT_HAND_GESTRUE_FINGER))
		cvDestroyWindow(FINGER_WINDOW);
	else if (param.contains(KinectParam::PATT_BLOB))
		cvDestroyWindow(BLOB_WINDOW);
}

void createParamWindow(const KinectParam& param, KinServer& device)
{
	cvNamedWindow(PARAM_WINDOW);
	cv::resizeWindow(PARAM_WINDOW, 400,400);
	cvCreateTrackbar("angle", PARAM_WINDOW, &new_angle, -NUI_CAMERA_ELEVATION_MINIMUM+NUI_CAMERA_ELEVATION_MAXIMUM, onAngleChange);
	if (param.contains(KinectParam::PATT_HAND_GESTRUE_FINGER))
	{
		cvCreateTrackbar("front_Z", PARAM_WINDOW, &device.thresh_low, 400);
		cvCreateTrackbar("after_Z", PARAM_WINDOW, &device.thresh_high, 400);
		cvCreateTrackbar("blob_low", PARAM_WINDOW, &device.blob_low, 1000);
		cvCreateTrackbar("blob_high", PARAM_WINDOW, &device.blob_high, 5000);
		cvCreateTrackbar("fist_R", PARAM_WINDOW, &device.fist_radius, 50);
	}
	else if (param.contains(KinectParam::PATT_HAND_TUIO))
	{
		cvCreateTrackbar2("n_players", PARAM_WINDOW, &device.n_players, 2, onGameModeChanged, &device);
		//cvCreateTrackbar2("n_hands", PARAM_WINDOW, &device.n_hands, 2, onGameModeChanged, &device);  
	}
	else if (param.contains(KinectParam::PATT_HAND_GESTURE_DISTANCE))
	{
		//assume arm is 80 cm long
		cvCreateTrackbar("hand_dist", PARAM_WINDOW, &device.hand_body_thresh, 800);
	}
	else if (param.contains(KinectParam::PATT_BLOB))
	{
		cvCreateTrackbar("min_area", PARAM_WINDOW, &device.min_area, MAX_AREA);
		cvCreateTrackbar("max_area", PARAM_WINDOW, &device.max_area, MAX_AREA);
		cvCreateTrackbar("open_param", PARAM_WINDOW, &device.open_param, 3);
	}
	else if (param.contains(KinectParam::PATT_BLOB2))
	{
		cvCreateTrackbar("z_near", PARAM_WINDOW, &device.z_near, Z_FAR);
		cvCreateTrackbar("z_far", PARAM_WINDOW, &device.z_far, Z_FAR);
		cvCreateTrackbar("min_area", PARAM_WINDOW, &device.min_area, MAX_AREA);
		cvCreateTrackbar("max_area", PARAM_WINDOW, &device.max_area, MAX_AREA);
	}
}

bool fruit_ninja = false;

void saveFrame( string filePath ) 
{
	throw std::exception("The method or operation is not implemented.");
}

int main(int argc, const char** argv)
{
	StartThread start_thread;
	start_thread.startThread(false, false);

	SetThreadName(-1, "main");

	const char *keys =
	{    
		"{help|h|false|display help info}"
		"{fruit_ninja||false|fruit_ninja mode is special}"
		"{device||0|the kinect device to use, default is 0}"
		"{client|c|localhost|the client's ip}"
		"{tuio||-1|the tuio port that client is listening at}"
		"{osc||7777|the osc port that client is listening at}"
		"{color||false|use color stream}"
		"{depth||true|use depth stream}" 
		"{skeleton||true|use skeleton stream}"
		"{pattern||hand|running pattern, available options are finger/hand_gesture/hand/body/blob/jointed_blob}"
		"{face|f|false|enable face detection mode}"
		"{minim||0|default mimized window}"
	};
	cv::CommandLineParser args(argc, argv, keys);
	if (args.get<bool>("help"))
	{
		args.printParams();
		return 0;
	}

	int device_id = args.get<int>("device"); 
	
	showAllWindow = !args.get<int>("minim"); 

	KinectParam the_param(args);
	KinServer device(device_id, the_param);

	if (!device.setup())
		return -1;

	fruit_ninja = args.get<bool>("fruit_ninja");
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

	cv::Ptr<ofxOscReceiver> recv = new ofxOscReceiver;
	recv->setup(PORT_RECV);
	printf("OSC Command server listening at %d\n", PORT_RECV);

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
		{	//Never change the camera angle more than once per second or
			//more than 15 times in any 20-second period
			int curr_angle = device.getDeviceAngle();
			if (curr_angle != new_angle)
				device.setDeviceAngle(new_angle);
			safe_time = timeGetTime();
			motor_changed = false;
		}
		int key = cv::waitKey(1);
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

		ofxOscMessage m;
		if (recv->getNextMessage(&m))
		{
			string addr = m.getAddress();
			if (addr == "/capture" && m.getNumArgs() == 1 && m.getArgType(0) == OFXOSC_TYPE_STRING)
			{
				string filePath = m.getArgAsString(0);
				saveFrame(filePath);
			}
		}

		if (showAllWindow)
		{
			if (the_param.color)
				cvShowImage(RGB_WINDOW, &(IplImage)device.getFrame(FRAME_COLOR_U8C3));
			if (the_param.depth)
				cvShowImage(DEPTH_WINDOW, &(IplImage)device.getFrame(FRAME_DEPTH_U8C3));
			if (the_param.skeleton)
				cvShowImage(SKELETON_WINDOW, &(IplImage)device.getFrame(FRAME_SKELETON_U8C3));
			if (the_param.contains(KinectParam::PATT_HAND_GESTRUE_FINGER))
			{
				ScopedLocker l(device.mtx_depth_data_view);
				cvShowImage(FINGER_WINDOW, &(IplImage)device.finger_view);
			}
			else
				if (the_param.contains(KinectParam::PATT_BLOB))
				{
					ScopedLocker l(device.mtx_depth_data_view);
					cvShowImage(BLOB_WINDOW, &(IplImage)device.blob_view);
				}
		}
	}
	return 0;
}
 