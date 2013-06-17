#include "KinectOsc.h"
#include "../../_common/ofxCvKalman/ofxCvKalman.h"

#ifdef DEMO
struct DemoLimiter
{
	bool valid;
	DemoLimiter()
	{
		time_t rawtime;
		time ( &rawtime );
		tm* timeinfo = localtime ( &rawtime );
		if (timeinfo->tm_mon == 2 && timeinfo->tm_mday < 21)
		{
			puts("::::\n");
			valid = true;
		}
		else
		{
			puts("....\n");
			valid = false;
		}		
	}

}demo_limiter;
#endif

void KinectOsc::_sendFinger_Osc(int playerIdx) 
{
	static char* addresses[] = {"/left","/right"};
	string action;

	for (int i=0;i<2;i++)
	{
		ofxOscMessage m;
		if (finger_dist[i] > fist_radius*0.1 || finger_dist[i] < FLT_EPSILON)//negative is also open!
			action = "open";
		else
			action = "close";
		m.setAddress(addresses[i]);		
		m.addStringArg(action);			//0
		m.addFloatArg(_hand_pos[i].x);	//1
		m.addFloatArg(_hand_pos[i].y);	//2
		m.addIntArg(m_DeviceId);		//3
		m.addIntArg(playerIdx);			//4
		sender_osc->sendMessage(m);
	}
}

template<typename T>
T clamp(T x, T min=0, T max=1)	
{
	return ( x < min ) ? min : ( ( x > max ) ? max : x );
}

template<typename T>
T lmap(T val, T inMin, T inMax, T outMin, T outMax)
{
	return outMin + (outMax - outMin) * ((val - inMin) / (inMax - inMin));
}

void KinectOsc::_sendHandGesture_Osc(cv::Point3f* pts, int playerIdx )
{
#ifdef DEMO
	if (!demo_limiter.valid)
		return;
#endif
	static char* addresses[] = {"/left","/right"}; 
	int hand_ids[] = {NUI_SKELETON_POSITION_HAND_LEFT, NUI_SKELETON_POSITION_HAND_RIGHT};
	string action;

	cv::Point3f center(pts[NUI_SKELETON_POSITION_HIP_CENTER].x, (pts[NUI_SKELETON_POSITION_HIP_CENTER].y+pts[NUI_SKELETON_POSITION_HEAD].y)/2,
		pts[NUI_SKELETON_POSITION_SHOULDER_CENTER].z);
	int center_ids[] = {NUI_SKELETON_POSITION_SHOULDER_CENTER, NUI_SKELETON_POSITION_SHOULDER_CENTER};
 
	float arm_extend = 1.3f*(pts[NUI_SKELETON_POSITION_HIP_CENTER].y - pts[NUI_SKELETON_POSITION_SHOULDER_CENTER].y); 

	for (int i=0;i<2;i++)
	{
		int hand_idx = hand_ids[i];
		float dist_hand_to_body = 1000*(center.z - pts[hand_idx].z);

		ofxOscMessage m;
		if (dist_hand_to_body > hand_body_thresh)//if hand is far from body
			action = "push";//then it's push action
		else
			action = "pull";
		m.setAddress(addresses[i]);
		m.addStringArg(action);		//0
#ifdef DEMO
		float mapped_x = lmap(kalman_filters[i*2+0]->value(), center.x - arm_extend, center.x + arm_extend, 0.0f, 1.0f);
		float mapped_y = lmap(kalman_filters[i*2+1]->value(), center.y - arm_extend, center.y + arm_extend, 0.0f, 1.0f);
		m.addFloatArg(clamp(mapped_x));
		m.addFloatArg(clamp(mapped_y));
#else
		m.addFloatArg(kalman_filters[i*2+0]->value());	//1
		m.addFloatArg(kalman_filters[i*2+1]->value());	//2
#endif
		m.addIntArg(m_DeviceId);		//3
		m.addIntArg(playerIdx);			//4
		sender_osc->sendMessage(m);
	}
}

void KinectOsc::_onFingerDepth(const cv::Mat& depth_u16c1, const cv::Mat& depth_u8c3, const cv::Mat& playerIdx_u8c1)
{
	cv::Point3f hands[2];
	hands[0] = 0.5f*(jointTo320X240(_hand_pos[0]) + jointTo320X240(_wrist_pos[0]));
	hands[1] = 0.5f*(jointTo320X240(_hand_pos[1]) + jointTo320X240(_wrist_pos[1]));

	const int  SZ = 40;

	threshed_depth = cv::Scalar(0); 

	for (int i=0;i<2;i++)
	{
		int x0 = std::max<int>(hands[i].x-SZ,0);
		int y0 = std::max<int>(hands[i].y-SZ,0);
		int x1 = std::min<int>(x0+SZ*2, DEPTH_WIDTH);
		int y1 = std::min<int>(y0+SZ*2, DEPTH_HEIGHT);

		for (int y=y0;y<y1;y++)
			for (int x=x0;x<x1;x++)
			{
				ushort dep = depth_u16c1.at<ushort>(y,x);
				uchar idx = playerIdx_u8c1.at<uchar>(y,x) -1;//TODO: the SAME THING -> -1
				uchar& out = threshed_depth.at<uchar>(y,x);
				if (idx != -1)
				{
					if (dep < hands[i].z+thresh_high && dep > hands[i].z-thresh_low)
					{
						out = 255;
					}
				}
			}
	}
	vOpen(threshed_depth,1);
	vector<vBlob> blobs;
	vector<vector<vDefect>> defects;
	vBlob* two_blobs[2];
	vector<vDefect>* two_defects[2];

	vFindBlobs(threshed_depth,blobs,defects,blob_low,blob_high);

	int n_blobs = blobs.size();
	if (n_blobs == 0)
		return;
#if 1
	for (int i=0;i<2;i++)
	{
		two_blobs[i] = NULL;
		int nn_id = -1;
		int nn_dist = INT_MAX;
		for (int k=0;k<n_blobs;k++)
		{
			vBlob& b = blobs[k];
			if (pointPolygonTest(b.pts, Point2f(hands[i].x, hands[i].y), false) > 0)
			{
				two_blobs[i] = &b;
				two_defects[i] = &defects[k];
				break;
			}
		}
	}
#else
	if (blobs.size() != 2)
		return;

	for (int i=0;i<2;i++)
	{
		two_blobs[i] = &blobs[i];
		two_defects[i] = &defects[i];
	}
#endif
	CvPoint info_pos[] = {{30,30}, {200,30}};

	vector<vBlob> finger_blobs;
	LOCK_START(mtx_depth_data_view);
	finger_view = CV_BLACK;
	for (int i=0;i<2;i++)
	{
		finger_dist[i] = 0;
		if (two_blobs[i] != NULL)
		{
			//	vFillPoly(&(IplImage)hand_view, two_blobs[i]->pts, vDefaultColor(i));
			cv::RotatedRect box = two_blobs[i]->rotBox;
			box.size.width *= 0.5;
			box.size.height *= 0.5;
			cv::ellipse(finger_view, box, CV_RED, 1);
			int n_pts = two_defects[i]->size();

			for (int k=0;k<n_pts;k++)
			{
				finger_dist[i] += two_defects[i]->at(k).depth;
				two_defects[i]->at(k).draw(finger_view, CV_GREEN);
				vBlob b;
				b.center = two_defects[i]->at(k).startPoint;
				finger_blobs.push_back(b);
			}
			finger_dist[i] /= n_pts;
		}
		char info[100];
		if (finger_dist[i] > FLT_EPSILON)
		{
			sprintf(info,"%.1f", finger_dist[i]);
			vDrawText(finger_view, info_pos[i].x,info_pos[i].y, info);
		}
	}

	for (int k=0;k<2;k++)
	{
		cv::circle(finger_view,Point(hands[k].x, hands[k].y), 3, CV_RED);
	}
	for (int k=0;k<n_blobs;k++)
	{
		vLinePoly(&(IplImage)finger_view, blobs[k].pts);
	}
	LOCK_END()
#ifdef USING_FINGER_TRACKER
		LOCK_START(mtx_finger_trakcer);
	finger_trakcer.trackBlobs(finger_blobs);
	LOCK_END()
#endif
}

void KinectOsc::_addJoint_Tuio(cv::Point3f* pts, int playerIdx ) 
{
	for (int i=0;i<refs.size();i++)
	{	//Set Message
		int id = refs[i];
		//skip outside joints
		if (pts[id].x <= 0 || pts[id].x >= 1.0f
			|| pts[id].y <= 0|| pts[id].y >= 1.0f)
			continue;

		int tuio_id = playerIdx*NUI_SKELETON_POSITION_COUNT+id; 

		ofxOscMessage set;
		set.setAddress( "/tuio/2Dcur" );
		set.addStringArg("set");
		set.addIntArg(tuio_id);				// id
		set.addFloatArg(pts[id].x);	// x
		set.addFloatArg(pts[id].y);	// y
		set.addFloatArg(pts[id].x - m_PrevPoints[playerIdx][id].x);			// dX
		set.addFloatArg(pts[id].y - m_PrevPoints[playerIdx][id].y);			// dY
		set.addFloatArg(0);		// m
		bundle.addMessage( set );							// add message to bundle
		alive.addIntArg(tuio_id);				// add blob to list of ALL active IDs
	}
}

#ifdef USING_FINGER_TRACKER
void KinectOsc::_addFingerTuio(int playerIdx) 
{
	if (!osc_enabled)
		return;

	LOCK_START(mtx_finger_trakcer);
	const std::vector<vTrackedBlob>& blobs = finger_trakcer.trackedBlobs;
	const int n_blobs = blobs.size();
	for (int i=0;i<n_blobs;i++)
	{	//Set Message
		const vTrackedBlob& b = blobs[i];
		//skip outside joints
		if (b.center.x < 0 || b.center.x > DEPTH_WIDTH
			|| b.center.y < 0|| b.center.y > DEPTH_HEIGHT)
			continue;

		int tuio_id = b.id;

		ofxOscMessage set;
		set.setAddress( "/tuio/2Dcur" );
		set.addStringArg("set");
		set.addIntArg(tuio_id);				// id
		set.addFloatArg(b.center.x/DEPTH_WIDTH);	// x
		set.addFloatArg(b.center.y/DEPTH_HEIGHT);	// y
		set.addFloatArg(b.velocity.x/DEPTH_WIDTH);			// dX
		set.addFloatArg(b.velocity.y/DEPTH_HEIGHT);			// dY
		set.addFloatArg(0);		// m
		bundle.addMessage( set );							// add message to bundle
		alive.addIntArg(tuio_id);				// add blob to list of ALL active IDs
	}
	LOCK_END()
}
#endif 