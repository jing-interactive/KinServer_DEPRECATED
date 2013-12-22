#include "KinServer.h"

using namespace cv;

void KinServer::_sendDepthStream(const MatU16& depth, const MatRGB& depthClr, const MatU8& playerIdx)
{
	//for (int y=0;y<DEPTH_HEIGHT;y++)
	//{
	//	for (int x=0;x<DEPTH_WIDTH;x++)
	//	{
	//		ushort dep = depth.at<ushort>(y,x); 
	//		uchar& out = threshed_depth.at<uchar>(y,x);
	//		if ( dep > z_near && dep < z_far)
	//		{ 
	//			out = 255; 
	//		}
	//	}
	//}
	//LOCK_START(mtx_depth_data_view);
	//threshed_depth.copyTo(blob_view);
	//const float scale = DEPTH_WIDTH*DEPTH_HEIGHT/MAX_AREA;
	//vFindBlobs(&(IplImage)threshed_depth, native_blobs, min_area*scale, max_area*scale);
	//center_of_blobs.clear();
	//for (int i=0;i<native_blobs.size();i++)
	//{
	//	if (native_blobs[i].isHole)
	//		continue;
	//	Point2f ct = native_blobs[i].center;
	//	ushort z = depth_u16.at<ushort>((int)ct.y, (int)ct.x);
	//	center_of_blobs.push_back(Point3f(ct.x/DEPTH_WIDTH,ct.y/DEPTH_HEIGHT,z));

	//	circle(blob_view, ct, 10, CV_GRAY, 4);
	//}
	//LOCK_END();
}
