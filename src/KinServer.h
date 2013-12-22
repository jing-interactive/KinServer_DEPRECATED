#include "../_common/Kinect/KinectDevice.h"
#include "../_common/OpenCV/OpenCV.h"
#include "../_common/OpenCV/BlobTracker.h"
#include "../_common/MiniThread.h"
#include "../_common/MiniMutex.h"
#include "../_common/ofxOsc/ofxOsc.h"

struct KinectParam
{
	enum{
		PATT_INVALID = 0,
		PATT_HAND_TUIO = 1<<0,	//tuio
		PATT_HAND_GESTURE_DISTANCE = 1<<1,//left_pull/push && righ_pull/push
		PATT_HAND_GESTRUE_FINGER = 1<<2,//left_open/close  && right_open/close
		PATT_JOINT = 1<<3,	//osc joints
		PATT_BLOB = 1<<4,	//CamServer mode
		PATT_BLOB2 = 1<<5,//not implemented
	};

	KinectParam(cv::CommandLineParser & args);

	bool contains(int queryMode) const;

	bool depth;
	bool color;
	bool skeleton;

	bool face;

	int pattern;

	string client;
	int port_tuio;
	int port_osc;

	bool dump;//save log to file
};

#define MAX_AREA 100

struct KinServer : public KinectDevice, KinectDelegate
{
	bool osc_enabled;//default->false
	cv::Ptr<ofxOscSender> sender_osc;
	cv::Ptr<ofxOscSender> sender_tuio;

	std::vector<int> refs;

	KinectParam& _param;
	///
	int n_players;//0,1 ->1,2
	int primary_player;//only necessary for one player mode
	int n_hands;//0,1,2 ->1,2,3 two_hands/head

	//////////////////////////////////////////////////////////////////////////
	//hand related
	cv::Mat threshed_depth, finger_view;
	int thresh_low;
	int thresh_high;
	int blob_low;
	int blob_high;
	cv::Point3f _hand_pos[2];
	cv::Point3f _wrist_pos[2];
	float finger_dist[2];
	int fist_radius;
	int hand_body_thresh;

	MiniMutex mtx_depth_data_view; 

	//////////////////////////////////////////////////////////////////////////
	//blob related
	int z_near;
	int z_far;
	int min_area;
	int max_area;
	vector<vBlob> native_blobs;
	vector<float> z_of_blobs;
	vector<int> id_of_blobs;
	cv::Mat blob_view;
	int open_param;

	bool setup();

	~KinServer();

	KinServer(int device_id, KinectParam& param);

	bool loadFrom();

	bool saveTo();

	void updatePlayMode();

	char buf[100];
	ofxOscBundle bundle;
	ofxOscMessage alive;

protected: //KinectDelegate
	virtual void onDepthData(const cv::Mat& depth_u16c1, const cv::Mat& depth_u8c3, const cv::Mat& playerIdx_u8c1);

	virtual void onSkeletonEventBegin();

	//tuio messages are sent here
	void onSkeletonEventEnd();

	void onPlayerData(cv::Point3f skel_points[NUI_SKELETON_POSITION_COUNT], int playerIdx, bool isNewPlayer,NUI_SKELETON_DATA* rawData);

	virtual void onPlayerEnter(int playerIdx);

	virtual void onPlayerLeave(int playerIdx);

private:

	void setFaceMode(bool is);
	cv::Point3f jointTo320X240(const cv::Point3f& pt)
	{
		return cv::Point3f(pt.x*DEPTH_WIDTH, pt.y*DEPTH_HEIGHT,	pt.z*1000);
	}
	void _addJoint_Tuio(cv::Point3f* pts, int playerIdx);
	void _sendFinger_Osc(int playerIdx);
	void _sendJoint_Osc(cv::Point3f skel_points[NUI_SKELETON_POSITION_COUNT], int playerIdx);
	void _sendOrientation_Osc(const NUI_SKELETON_DATA* skel_data, int playerIdx);

	void _sendBlobOsc(int = 0);
	void _onFingerDepth( const cv::Mat& depth_u16c1, const cv::Mat& depth_u8c3, const cv::Mat& playerIdx_u8c1 );
	void _onBlobDepth( const cv::Mat& depth_u16c1, const cv::Mat& depth_u8c3, const cv::Mat& playerIdx_u8c1 );
	void _sendHandGesture_Osc(cv::Point3f* pts, int playerIdx );
};