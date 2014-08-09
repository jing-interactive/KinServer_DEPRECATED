#include "KinServer.h"
#include "../_common/OpenCV/ofxCvKalman.h"

using namespace cv;

void KinServer::_addJoint_Tuio(Point3f* pts, int playerIdx ) 
{
	for (int i=0;i<refs.size();i++)
	{	
        //Set Message
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
