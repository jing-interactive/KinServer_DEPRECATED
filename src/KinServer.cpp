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
        pattern |= PATT_CAMSERVER;
        printf("protocol OSC");
        depth = true;
        //	color = false;
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
    else if (patt == "ccv")
    {
        pattern |= PATT_CCV;
        pattern |= PATT_CAMSERVER;
        printf("protocol TUIO (OSC will be added soon)");

        if (port_tuio == -1)
            port_tuio = 3333;

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
        printf("without network message");
    }
    puts(".\n");
}

bool KinectOption::contains( int queryMode ) const
{
    return (pattern & queryMode) != 0;
}

bool KinServer::setup()
{
    isBgDirty = false;
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

    threshed_depth = Mat1b(Size(DEPTH_WIDTH, DEPTH_HEIGHT), 0);
    depth_bg = Mat1w(Size(DEPTH_WIDTH, DEPTH_HEIGHT), 0);
    finger_view = Mat3b(Size(DEPTH_WIDTH, DEPTH_HEIGHT), Vec3b(255, 255, 255));

    n_hands = 2;//default: two hands
    n_players = 1;//default: one player mode
    primary_player = -1;

    z_threshold_mm = 50;
    min_area = 100;
    blob_view = Mat1b(Size(DEPTH_WIDTH, DEPTH_HEIGHT), 0);
    open_param = 1;

    x0 = 0;
    y0 = 0;
    x1 = DEPTH_WIDTH;
    y1 = DEPTH_HEIGHT;

    sender_osc = new ofxOscSender;
    sender_osc->setup(mOption.client, mOption.port_osc);
    printf("OSC\t[%s:%d]\n",mOption.client.c_str(), mOption.port_osc);

    int port_tuio = mOption.port_tuio;
    if (port_tuio > 0)
    {
        sender_tuio = new ofxOscSender;
        sender_tuio->setup(mOption.client, port_tuio);
        printf("TUIO\t[%s:%d]\n", mOption.client.c_str(), port_tuio);
    }
    puts("\n");

    loadFrom();

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
        READ_FS(z_threshold_mm);
        READ_FS(min_area);
        READ_FS(x0);
        READ_FS(y0);
        READ_FS(x1);
        READ_FS(y1);

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
        WRITE_FS(z_threshold_mm);
        WRITE_FS(min_area);
        WRITE_FS(x0);
        WRITE_FS(y0);
        WRITE_FS(x1);
        WRITE_FS(y1);

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
    if (mOption.pattern & KinectOption::PATT_HAND_TUIO)
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
    if (mOption.pattern & KinectOption::PATT_HAND_TUIO)
    {
        static int frameseq = 0;

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

void KinServer::onDepthData(const Mat1w& depth, const Mat3b& depthClr, const Mat1b& playerIdx)
{
    if (isBgDirty)
    {
        isBgDirty = false;
        depth_bg = depth.clone();
    }

    if (mOption.contains(KinectOption::PATT_CCV))
        _onBlobCCV(depth, depthClr, playerIdx);
    else if (mOption.contains(KinectOption::PATT_CAMSERVER))
        _onBlobCamServer(depth, depthClr, playerIdx);
}

void KinServer::onPlayerData(Point3f skel_points[NUI_SKELETON_POSITION_COUNT], int playerIdx, bool isNewPlayer, NUI_SKELETON_DATA* rawData)
{
    if (mOption.pattern & KinectOption::PATT_HAND_TUIO)
    {
        if ((n_players == 1 && primary_player == playerIdx) || n_players == 2)
            _addJoint_Tuio(skel_points, playerIdx);//just add, not send
    }

    if (mOption.pattern & KinectOption::PATT_JOINT)
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

void KinServer::updateBg()
{
    isBgDirty = true;
}
