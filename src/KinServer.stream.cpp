#include "KinServer.h"
#include <sstream>

using namespace cv;

void KinServer::_sendDepthStream(const MatU16& depth, const MatRGB& depthClr, const MatU8& playerIdx)
{
    ofxOscMessage msg;
    msg.setAddress("/depth_stream");

    // bounding box
    int start[DEPTH_HEIGHT];
    int end[DEPTH_HEIGHT];
    std::fill(start,    start + DEPTH_HEIGHT,   DEPTH_WIDTH);
    std::fill(end,      end + DEPTH_HEIGHT,     0);

	for (int y=0;y<DEPTH_HEIGHT;y++)
	{
		for (int x=0;x<DEPTH_WIDTH;x++)
		{
            if (playerIdx(y, x) > 0)
            {
                start[y] = min<int>(start[y], x);
                end[y] = max<int>(end[y], x);
            }
		}
	}

    for (int y=0;y<DEPTH_HEIGHT;y++)
    {
        std::stringstream ss;
        ss << start[y] << ' ' << end[y] << ' ';
        for (int x=start[y];x<end[y];x++)
        {
            ushort val16 = depth(y, x);
            uchar val8 = val16 >> 4;
            ss << val8;
        }
        msg.addStringArg(ss.str());
    }
    sender_osc->sendMessage(msg);
}
