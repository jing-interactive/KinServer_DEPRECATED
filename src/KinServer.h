#include "../_common/Kinect/KinectDevice.h"
#include "../_common/OpenCV/OpenCV.h"
#include "../_common/OpenCV/BlobTracker.h"
#include "../_common/MiniThread.h"
#include "../_common/MiniMutex.h"
#include "../_common/ofxOsc/ofxOsc.h"

struct KinectOption
{
    enum{
        PATT_INVALID = 0,
        PATT_HAND_TUIO = 1<<0,	            // tuio
        PATT_JOINT = 1<<1,	                // osc joints
        PATT_CAMSERVER = 1<<2,              // CamServer mode
        PATT_DEPTH_STREAM = 1<<3,           // steaming depth data
        PATT_CCV = 1<<4,                    // Core community vision
    };

    KinectOption(cv::CommandLineParser & args);

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

class KinServer : public KinectDevice, KinectDelegate
{
public:
    bool osc_enabled;//default->false
    cv::Ptr<ofxOscSender> sender_osc;
    cv::Ptr<ofxOscSender> sender_tuio;

    std::vector<int> refs;

    KinectOption& mOption;
    ///
    int n_players;//0,1 ->1,2
    int primary_player;//only necessary for one player mode
    int n_hands;//0,1,2 ->1,2,3 two_hands/head

    //////////////////////////////////////////////////////////////////////////
    //hand related
    MatU8 threshed_depth;
    MatRGB finger_view;

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

    KinServer(int device_id, KinectOption& param);

    bool loadFrom();

    bool saveTo();

    void updatePlayMode();

    char buf[100];
    ofxOscBundle bundle;
    ofxOscMessage alive;

protected: //KinectDelegate
    virtual void onDepthData(const MatU16& depth, const MatRGB& depthClr, const MatU8& playerIdx);

    virtual void onSkeletonEventBegin();

    //tuio messages are sent here
    void onSkeletonEventEnd();

    void onPlayerData(cv::Point3f skel_points[NUI_SKELETON_POSITION_COUNT], int playerIdx, bool isNewPlayer,NUI_SKELETON_DATA* rawData);

    virtual void onPlayerEnter(int playerIdx);

    virtual void onPlayerLeave(int playerIdx);

private:

    void _addJoint_Tuio(cv::Point3f* pts, int playerIdx);
    void _sendJoint_Osc(cv::Point3f skel_points[NUI_SKELETON_POSITION_COUNT], int playerIdx);
    void _sendOrientation_Osc(const NUI_SKELETON_DATA* skel_data, int playerIdx);

    void _sendBlobOsc(int = 0);
    void _onBlobDepth( const MatU16& depth, const MatRGB& depthClr, const MatU8& playerIdx );
    void _sendHandGesture_Osc(cv::Point3f* pts, int playerIdx );

    void _sendDepthStream( const MatU16& depth, const MatRGB& depthClr, const MatU8& playerIdx );
};