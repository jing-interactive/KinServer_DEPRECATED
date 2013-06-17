#include "../../_common/ofxOsc/ofxOsc.h"
#include "../../_common/MiniLog.h"
#include "KinectOsc.h"
#include "../../_common/ofxCvKalman/ofxCvKalman.h"

struct FaceThread : public MiniThread
{
	FaceThread(KinectOsc& ref):_ref(ref),MiniThread("FaceThread"){}
	void threadedFunction()
	{
		if (_haar.init("../../haarcascade_frontalface_default.xml"))
			K_MSG("faild to load haar cascade xml");
		else
		{
			while (isThreadRunning())
			{
				_ref.evt_face.wait();
				ScopedLocker lock(_ref.mtx_face);
				{
					IplImage ipl = _frame;
					_haar.find(&ipl, 1, false);
				}
			}
		}
	}

	cv::Mat _frame;
	KinectOsc& _ref;
	vHaarFinder _haar;
};

KinectParam::KinectParam(cv::CommandLineParser& args)
{
	color = args.get<bool>("color");
	depth = args.get<bool>("depth");
	skeleton = args.get<bool>("skeleton");

	bool face = args.get<bool>("face");
	if (face)
		color = true;

	dump = false;//default no file dumping
	
	client = args.get<string>("client");
	port_osc = args.get<int>("osc");
	port_tuio = args.get<int>("tuio");

	string patt = args.get<string>("pattern");
	printf("running with pattern: %s / ", patt.c_str());

	pattern = PATT_INVALID;

	if (port_tuio > 0)
		pattern |= PATT_HAND_TUIO;
	if (patt == "jointed_blob")
	{
		pattern |= PATT_HAND_GESTURE_DISTANCE;
		pattern |= PATT_BLOB;
		printf("protocol OSC");
		depth = true;
		//	color = false;
		skeleton = true;
	}
	else
	if (patt == "finger")
	{
		pattern |= PATT_HAND_GESTRUE_FINGER;
		printf("protocol OSC");
		depth = true;//special case~ cause I need it
		//		color = false;
		skeleton = true;
	}
	else if (patt == "hand_gesture")
	{
		pattern |= PATT_HAND_GESTURE_DISTANCE;
		printf("protocol OSC");
		depth = false;
		//		color = false;
		skeleton = true;			
	}
	else if (patt == "hand")
	{
		pattern |= PATT_HAND_TUIO;
		printf("protocol TUIO");
		depth = false;
		if (port_tuio == -1)
			port_tuio = 3333;
		//		color = false;
		skeleton = true;			
	}
	else if (patt == "blob")
	{
		pattern |= PATT_BLOB;
		printf("protocol OSC");
		depth = true;
		//	color = false;
		skeleton = false;//no need this
	}
	else if (patt == "body")
	{
		pattern |= PATT_JOINT;
		printf("protocol OSC");
		depth = true;
		//		color = false;
		skeleton = true;		
	}
	else
	{
		printf("no message out");
	}
	puts("\n");
}

bool KinectParam::contains( int queryMode ) const
{
	return pattern & queryMode;
}

bool KinectOsc::setup()
{
	return KinectDevice::setup(_param.color, _param.depth, _param.skeleton, this);
}

KinectOsc::~KinectOsc()
{
	saveTo();
}

KinectOsc::KinectOsc(int device_id, KinectParam& param):
	KinectDevice(device_id), _param(param)
{
	osc_enabled = false; 

	threshed_depth.create(cv::Size(DEPTH_WIDTH, DEPTH_HEIGHT), CV_8UC1);
	threshed_depth = CV_BLACK;
	thresh_low = 150;
	thresh_high = 40;
	blob_low = 300;
	blob_high = 2500;
	finger_view.create(cv::Size(DEPTH_WIDTH, DEPTH_HEIGHT), CV_8UC3);
	finger_view = cv::Scalar(0,0,0);

	fist_radius = 20;
	n_hands = 2;//default: two hands
	n_players = 1;//default: one player mode
	primary_player = -1;

	finger_dist[0] = finger_dist[1] = -1;
	hand_body_thresh = 400;//40 cm

	z_near = 1000;
	z_far = 3000;
	min_area = 1;
	max_area = 60;
	blob_view.create(cv::Size(DEPTH_WIDTH, DEPTH_HEIGHT), CV_8UC1);
	blob_view = CV_BLACK;
	open_param = 1;

	sender_osc = new ofxOscSender;
	sender_osc->setup(_param.client, _param.port_osc);
	printf("OSC\t[%s:%d]\n",_param.client.c_str(), _param.port_osc);

	int port_tuio = _param.port_tuio;
	if (port_tuio > 0)
	{
		sender_tuio = new ofxOscSender;
		sender_tuio->setup(_param.client, port_tuio);
		printf("TUIO\t[%s:%d]\n",_param.client.c_str(), port_tuio);
	}
	puts("\n");

	loadFrom();

	//TODO: if (_param.finger) ?
	if (_param.contains(KinectParam::PATT_HAND_TUIO))
	{
		updatePlayMode();
	}
}

bool KinectOsc::loadFrom()
{
	cv::FileStorage fs("KinConfig.xml", cv::FileStorage::READ);
	if (!fs.isOpened())
	{
		printf("KinConfig.xml doesn't exist, KinServer starts with default values.\n");
		return false;
	}
	else
	{
 		READ_FS(n_players);
 		READ_FS(n_hands);
		READ_FS(open_param);
		READ_FS(hand_body_thresh);
		READ_FS(thresh_low);
		READ_FS(thresh_high);
		READ_FS(blob_low);
		READ_FS(blob_high);
		READ_FS(fist_radius);
		READ_FS(z_near);
		READ_FS(z_far);
		READ_FS(min_area);
		READ_FS(max_area);

		printf("KinConfig.xml loaded.\n");
		return true;
	}
}

bool KinectOsc::saveTo()
{
	cv::FileStorage fs("KinConfig.xml", cv::FileStorage::WRITE);
	if (!fs.isOpened())
	{
		printf("failed to open KinConfig.xml for writing.\n");
		return false;
	}
	else
	{
 		WRITE_FS(n_players);
 		WRITE_FS(n_hands);
		WRITE_FS(open_param);
		WRITE_FS(hand_body_thresh); 
		WRITE_FS(thresh_low);
		WRITE_FS(thresh_high);
		WRITE_FS(blob_low);
		WRITE_FS(blob_high);
		WRITE_FS(fist_radius);
		WRITE_FS(z_near);
		WRITE_FS(z_far);
		WRITE_FS(min_area);
		WRITE_FS(max_area);

		printf("KinConfig.xml saved.\n");
		return true;
	}
}

void KinectOsc::updatePlayMode()
{
	refs.clear();
	if (n_hands == 1)
		refs.push_back(NUI_SKELETON_POSITION_HAND_RIGHT);
	else if (n_hands == 2)
	{
		refs.push_back(NUI_SKELETON_POSITION_HAND_LEFT);
		refs.push_back(NUI_SKELETON_POSITION_HAND_RIGHT);
	}
// 	else if (n_hands == 3)
// 	{
// 		refs.push_back(NUI_SKELETON_POSITION_HAND_LEFT);
// 		refs.push_back(NUI_SKELETON_POSITION_HAND_RIGHT);
// 		refs.push_back(NUI_SKELETON_POSITION_HEAD);   
// 	}
	primary_player = -1;
}

void KinectOsc::onRgbData(const cv::Mat& rgb)
{
	if (!_param.face)
		return;
	ScopedLocker lock(mtx_face);
	{
		rgb.copyTo(thread_face->_frame);
	}
	evt_face.set();		
}
 
void KinectOsc::setFaceMode(bool is)
{
	_param.face = is;
	thread_face.release();
	if (_param.face)
	{
		thread_face = new FaceThread(*this);
	}
}

void KinectOsc::onSkeletonEventBegin()
{
	if (_param.pattern&KinectParam::PATT_HAND_TUIO)
	{
		bundle.clear();
		alive.clear();
		alive.setAddress("/tuio/2Dcur");
		alive.addStringArg("alive");
	}
}

//tuio messages are sent here
void KinectOsc::onSkeletonEventEnd()
{
	static int frameseq = 0;
	if (_param.pattern&KinectParam::PATT_HAND_TUIO)
	{
		frameseq++;
		// Send fseq message
		ofxOscMessage fseq;
		fseq.setAddress( "/tuio/2Dcur" );
		fseq.addStringArg( "fseq" );
		fseq.addIntArg(frameseq);

		bundle.addMessage( alive );	 //add message to bundle
		bundle.addMessage( fseq );	 //add message to bundle
		sender_tuio->sendBundle( bundle ); //send bundle
	}
}

void KinectOsc::onDepthData(const cv::Mat& depth_u16c1, const cv::Mat& depth_u8c3, const cv::Mat& playerIdx_u8c1)
{
	if (_param.contains(KinectParam::PATT_HAND_GESTRUE_FINGER))
		_onFingerDepth(depth_u16c1, depth_u8c3, playerIdx_u8c1);
	else if (_param.contains(KinectParam::PATT_BLOB))
		_onBlobDepth(depth_u16c1, depth_u8c3, playerIdx_u8c1);
}

void KinectOsc::onPlayerData(cv::Point3f skel_points[NUI_SKELETON_POSITION_COUNT], int playerIdx, bool isNewPlayer, NUI_SKELETON_DATA* rawData)
{
	_hand_pos[0] = skel_points[NUI_SKELETON_POSITION_HAND_LEFT];
	_hand_pos[1] = skel_points[NUI_SKELETON_POSITION_HAND_RIGHT];
	_wrist_pos[0] = skel_points[NUI_SKELETON_POSITION_WRIST_LEFT];
	_wrist_pos[1] = skel_points[NUI_SKELETON_POSITION_WRIST_RIGHT];

	float values[] = {_hand_pos[0].x, _hand_pos[0].y, _hand_pos[1].x, _hand_pos[1].y};

	if (isNewPlayer)
	{
		for (int i=0;i<4;i++)
		{
			kalman_filters[i] = new ofxCvKalman(values[i]);
		}
	}
	else
	{
		for (int i=0;i<4;i++)
		{
			kalman_filters[i]->update(values[i]);
		}
	}

	if (_param.pattern & KinectParam::PATT_HAND_TUIO)
	{
		if ((n_players == 1 && primary_player == playerIdx) || n_players == 2)
			_addJoint_Tuio(skel_points, playerIdx);//just add, not send
	}

	int patt = _param.pattern;
	if (patt & KinectParam::PATT_HAND_GESTURE_DISTANCE)
	{
		_sendHandGesture_Osc(skel_points, playerIdx);//distance based stable hand info
	}
	if (patt & KinectParam::PATT_HAND_GESTRUE_FINGER)
	{
		_sendFinger_Osc(playerIdx);//detailed hand info
	}
	if (patt & KinectParam::PATT_JOINT)
	{
		_sendJoint_Osc(skel_points, playerIdx);
		_sendOrientation_Osc(rawData, playerIdx);
	}
}
 
void KinectOsc::_sendJoint_Osc(cv::Point3f skel_points[NUI_SKELETON_POSITION_COUNT], int playerIdx ) 
{
	ofxOscMessage m;
	m.setAddress("/kinect");
	m.addIntArg(m_DeviceId);
	m.addIntArg(playerIdx);

	if (_param.dump)
	{
		MiniLog("\n/kinect, %d\n",playerIdx);
	}

	for (int i = 0; i < NUI_SKELETON_POSITION_COUNT; i++)
	{
		//for osc msg
		sprintf(buf, "%.3f,%.3f,%.3f,%.3f", skel_points[i].x, skel_points[i].y, skel_points[i].z, m_Confidences[i]);
		m.addStringArg(buf);

		//for dumping
		if (_param.dump)
			MiniLog("%.3f,%.3f,%.3f,%.3f\n", skel_points[i].x, skel_points[i].y, skel_points[i].z, m_Confidences[i]);
	}
	sender_osc->sendMessage(m);
}

void KinectOsc::_sendOrientation_Osc(const NUI_SKELETON_DATA* skel_data, int playerIdx ) 
{
	ofxOscMessage m;
	m.setAddress("/orient");
	m.addIntArg(m_DeviceId);
	m.addIntArg(playerIdx);

	if (_param.dump)
	{
		MiniLog("\n/orient, %d\n",playerIdx);
	}
	NUI_SKELETON_BONE_ORIENTATION bone_orients[NUI_SKELETON_POSITION_COUNT];
	NuiSkeletonCalculateBoneOrientations(skel_data, bone_orients);
	for (int i = 0; i < NUI_SKELETON_POSITION_COUNT; i++)
	{
		//for osc msg
		Vector4 quaternion = bone_orients->hierarchicalRotation.rotationQuaternion;//getJointOrientation(skel_points_raw, static_cast<NUI_SKELETON_POSITION_INDEX>(i));
		sprintf(buf, "%.3f,%.3f,%.3f,%.3f", quaternion.x, quaternion.y, quaternion.z, quaternion.w);
		m.addStringArg(buf);
		//for dumping
		if (_param.dump)
			MiniLog("%.3f,%.3f,%.3f,%.3f\n", quaternion.x, quaternion.y, quaternion.z, quaternion.w);
	}
	sender_osc->sendMessage(m);
}

void KinectOsc::onPlayerEnter( int playerIdx )
{
	// update primary_player
	if (primary_player == -1)
		primary_player = playerIdx;
}

void KinectOsc::onPlayerLeave( int playerIdx )
{
	// update primary_player
	if (primary_player == playerIdx)
		primary_player = -1;
}
