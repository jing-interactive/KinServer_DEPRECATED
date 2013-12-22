#include "KinServer.h"

using namespace cv;

void KinServer::_onBlobDepth(const MatU16& depth, const MatRGB& depthClr, const MatU8& playerIdx)
{
#if 0
	threshed_depth = CV_BLACK;
	for (int y=0;y<DEPTH_HEIGHT;y++)
	{
		for (int x=0;x<DEPTH_WIDTH;x++)
		{
			ushort dep = depth_u16.at<ushort>(y,x); 
			uchar& out = threshed_depth.at<uchar>(y,x);
			if ( dep > z_near && dep < z_far)
			{ 
				out = 255; 
			}
		}
	}
	LOCK_START(mtx_depth_data_view);
	threshed_depth.copyTo(blob_view);
	const float scale = DEPTH_WIDTH*DEPTH_HEIGHT/MAX_AREA;
	vFindBlobs(&(IplImage)threshed_depth, native_blobs, min_area*scale, max_area*scale);
	center_of_blobs.clear();
	for (int i=0;i<native_blobs.size();i++)
	{
		if (native_blobs[i].isHole)
			continue;
		Point2f ct = native_blobs[i].center;
		ushort z = depth_u16.at<ushort>((int)ct.y, (int)ct.x);
		center_of_blobs.push_back(Point3f(ct.x/DEPTH_WIDTH,ct.y/DEPTH_HEIGHT,z));

		circle(blob_view, ct, 10, CV_GRAY, 4);
	}
	LOCK_END();
#else
	threshed_depth.setTo(CV_BLACK);
	for (int y=0;y<DEPTH_HEIGHT;y++)
	{
		for (int x=0;x<DEPTH_WIDTH;x++)
		{
			const Vec3b& clr = depthClr(y,x);
			uchar& out = threshed_depth(y,x);
			if ((clr[0] != clr[1]) || (clr[1] != clr[2]))
				out = 255;
		}
	}
	LOCK_START(mtx_depth_data_view);
	threshed_depth.copyTo(blob_view);
	const float scale = DEPTH_WIDTH*DEPTH_HEIGHT/MAX_AREA;
	if (open_param > 0)
		vOpen(threshed_depth, 1);
	vFindBlobs(threshed_depth, native_blobs, min_area*scale, max_area*scale);
	z_of_blobs.clear();
	id_of_blobs.clear();
	for (int i=0;i<native_blobs.size();i++)
	{
		// 			if (native_blobs[i].isHole)
		// 				continue;
		Point2f ct = native_blobs[i].center;
		ushort z = depth(ct.y, ct.x);
		z_of_blobs.push_back(z);
		int idx =  playerIdx(ct.y, ct.x) - 1;//TODO: make sure about the -1
		id_of_blobs.push_back(idx);

		circle(blob_view, ct, 10, CV_GRAY, 4);
	}
	LOCK_END();
#endif
	_sendBlobOsc();
	// 			Moments mmt = moments(threshed_depth, true);
	// 			obj.center.x = mmt.m10 / mmt.m00;
	// 			obj.center.y = mmt.m01 / mmt.m00; 
}

void KinServer::_sendBlobOsc(int/* = 0*/) 
{
	if (native_blobs.empty())
		return;

	{
		ofxOscMessage m;
		m.setAddress("/start");
		m.addIntArg(m_DeviceId);
		sender_osc->sendMessage(m);
	}

#define addFloatX(num) m.addFloatArg(num/(float)DEPTH_WIDTH)
#define addFloatY(num) m.addFloatArg(num/(float)DEPTH_HEIGHT)

	int n_blobs = native_blobs.size();
	for (int i = 0; i < n_blobs; i++)
	{
		float cz = z_of_blobs[i];
		if (cz < Z_NEAR || cz > Z_FAR)//skip illegal data
			continue;

		ofxOscMessage m;
		const vBlob& obj = native_blobs[i];
		int id = m_DeviceId*NUI_SKELETON_COUNT + id_of_blobs[i];
		float cx = obj.center.x;
		float cy = obj.center.y;		
		float w = obj.box.width;
		float h = obj.box.height;
		int nPts = obj.pts.size();

		if (obj.isHole)
			m.setAddress("/hole");
		else
			m.setAddress("/contour");

		m.addIntArg(id);                        //0
		m.addStringArg("/move");				//1
		addFloatX(cx);                          //2 -> cx
		addFloatY(cy);                          //3 -> cy
		addFloatX(w);							//4
		addFloatY(h);							//5 
		m.addFloatArg(cz);						//6 -> cz
		m.addIntArg(nPts);                      //7
		for (int k=0;k<nPts;k++)
		{
			addFloatX(obj.pts[k].x);
			addFloatY(obj.pts[k].y);
		}
		sender_osc->sendMessage( m );
	}
#undef addFloatX
#undef addFloatY
}
