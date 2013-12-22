#include "../_common/ofxOsc/ofxOsc.h"
#include "../_common/MiniLog.h"
#include "KinServer.h"

using namespace cv;

KinectOption::KinectOption(CommandLineParser& args)
{
	color = args.has("color");
	depth = args.has("depth");
	skeleton = args.has("skeleton");

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
		pattern |= PATT_CAMSERVER;
		printf("protocol OSC");
		depth = true;
		//	color = false;
		skeleton = true;
	}
	else if (patt == "finger")
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
	else if (patt == "cam_server")
	{
		pattern |= PATT_CAMSERVER;
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
    else if (patt == "depth_stream")
    {
        pattern |= PATT_DEPTH_STREAM;
        printf("protocol OSC");
        depth = true;
        //		color = false;
        skeleton = false;		
    }
	else
	{
		printf("without network message");
	}
	puts(".\n");
}

bool KinectOption::contains( int queryMode ) const
{
	return pattern & queryMode;
}

bool KinServer::setup()
{
	return KinectDevice::setup(mOption.color, mOption.depth, mOption.skeleton, this);
}

KinServer::~KinServer()
{
	saveTo();
}

KinServer::KinServer(int device_id, KinectOption& param):
	KinectDevice(device_id), mOption(param)
{
	osc_enabled = false; 

	threshed_depth = MatU8(Size(DEPTH_WIDTH, DEPTH_HEIGHT), 0);
	thresh_low = 150;
	thresh_high = 40;
	blob_low = 300;
	blob_high = 2500;
	finger_view = MatRGB(Size(DEPTH_WIDTH, DEPTH_HEIGHT), Vec3b(255, 255, 255));

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
	blob_view.create(Size(DEPTH_WIDTH, DEPTH_HEIGHT), CV_8UC1);
	blob_view = CV_BLACK;
	open_param = 1;

	sender_osc = new ofxOscSender;
	sender_osc->setup(mOption.client, mOption.port_osc);
	printf("OSC\t[%s:%d]\n",mOption.client.c_str(), mOption.port_osc);

	int port_tuio = mOption.port_tuio;
	if (port_tuio > 0)
	{
		sender_tuio = new ofxOscSender;
		sender_tuio->setup(mOption.client, port_tuio);
		printf("TUIO\t[%s:%d]\n",mOption.client.c_str(), port_tuio);
	}
	puts("\n");

	loadFrom();

	//TODO: if (_param.finger) ?
	if (mOption.contains(KinectOption::PATT_HAND_TUIO))
	{
		updatePlayMode();
	}
}

bool KinServer::loadFrom()
{
	FileStorage fs("KinConfig.xml", FileStorage::READ);
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

bool KinServer::saveTo()
{
	FileStorage fs("KinConfig.xml", FileStorage::WRITE);
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

void KinServer::updatePlayMode()
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

void KinServer::onSkeletonEventBegin()
{
	if (mOption.pattern&KinectOption::PATT_HAND_TUIO)
	{
		bundle.clear();
		alive.clear();
		alive.setAddress("/tuio/2Dcur");
		alive.addStringArg("alive");
	}
}

//tuio messages are sent here
void KinServer::onSkeletonEventEnd()
{
	static int frameseq = 0;
	if (mOption.pattern&KinectOption::PATT_HAND_TUIO)
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

void KinServer::onDepthData(const MatU16& depth, const MatRGB& depthClr, const MatU8& playerIdx)
{
	if (mOption.contains(KinectOption::PATT_HAND_GESTRUE_FINGER))
		_onFingerDepth(depth, depthClr, playerIdx);
	else if (mOption.contains(KinectOption::PATT_CAMSERVER))
        _onBlobDepth(depth, depthClr, playerIdx);
    else if (mOption.contains(KinectOption::PATT_DEPTH_STREAM))
        _sendDepthStream(depth, depthClr, playerIdx);
}

void KinServer::onPlayerData(Point3f skel_points[NUI_SKELETON_POSITION_COUNT], int playerIdx, bool isNewPlayer, NUI_SKELETON_DATA* rawData)
{
	_hand_pos[0] = skel_points[NUI_SKELETON_POSITION_HAND_LEFT];
	_hand_pos[1] = skel_points[NUI_SKELETON_POSITION_HAND_RIGHT];
	_wrist_pos[0] = skel_points[NUI_SKELETON_POSITION_WRIST_LEFT];
	_wrist_pos[1] = skel_points[NUI_SKELETON_POSITION_WRIST_RIGHT];

	if (mOption.pattern & KinectOption::PATT_HAND_TUIO)
	{
		if ((n_players == 1 && primary_player == playerIdx) || n_players == 2)
			_addJoint_Tuio(skel_points, playerIdx);//just add, not send
	}

	int patt = mOption.pattern;
	if (patt & KinectOption::PATT_HAND_GESTURE_DISTANCE)
	{
		_sendHandGesture_Osc(skel_points, playerIdx);//distance based stable hand info
	}
	if (patt & KinectOption::PATT_HAND_GESTRUE_FINGER)
	{
		_sendFinger_Osc(playerIdx);//detailed hand info
	}
	if (patt & KinectOption::PATT_JOINT)
	{
		_sendJoint_Osc(skel_points, playerIdx);
		_sendOrientation_Osc(rawData, playerIdx);
	}
}
 
void KinServer::_sendJoint_Osc(Point3f skel_points[NUI_SKELETON_POSITION_COUNT], int playerIdx ) 
{
	ofxOscMessage m;
	m.setAddress("/kinect");
	m.addIntArg(m_DeviceId);
	m.addIntArg(playerIdx);

	if (mOption.dump)
	{
		MiniLog("\n/kinect, %d\n",playerIdx);
	}

	for (int i = 0; i < NUI_SKELETON_POSITION_COUNT; i++)
	{
		//for osc msg
		sprintf(buf, "%.3f,%.3f,%.3f,%.3f", skel_points[i].x, skel_points[i].y, skel_points[i].z, m_Confidences[i]);
		m.addStringArg(buf);

		//for dumping
		if (mOption.dump)
			MiniLog("%.3f,%.3f,%.3f,%.3f\n", skel_points[i].x, skel_points[i].y, skel_points[i].z, m_Confidences[i]);
	}
	sender_osc->sendMessage(m);
}

void KinServer::_sendOrientation_Osc(const NUI_SKELETON_DATA* skel_data, int playerIdx ) 
{
	ofxOscMessage m;
	m.setAddress("/orient");
	m.addIntArg(m_DeviceId);
	m.addIntArg(playerIdx);

	if (mOption.dump)
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
		if (mOption.dump)
			MiniLog("%.3f,%.3f,%.3f,%.3f\n", quaternion.x, quaternion.y, quaternion.z, quaternion.w);
	}
	sender_osc->sendMessage(m);
}

void KinServer::onPlayerEnter( int playerIdx )
{
	// update primary_player
	if (primary_player == -1)
		primary_player = playerIdx;
}

void KinServer::onPlayerLeave( int playerIdx )
{
	// update primary_player
	if (primary_player == playerIdx)
		primary_player = -1;
}
