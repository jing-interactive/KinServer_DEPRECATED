//forwarding depth points
//cause it's the most simple form

import oscP5.*;
import netP5.*;

OscP5 oscP5;
NetAddress to_client;
final int n_joints = 20;
final int Width = 400;
final int Height = 400;

class Vector4
{
  float x, y, z, w;
  Vector4(float x, float y, float z, float w)
  {
    this.x = x;
    this.y = y;
    this.z = z;
    this.w = w;
  }
}

class Skeleton
{
  int player_idx = -1;
  PVector joints[] = new PVector[n_joints];
  PVector joints_depth[] = new PVector[n_joints];
  Vector4 orients[] = new Vector4[n_joints];
  void draw()
  {
    fill(0);
    for (PVector j:joints_depth)
    {
      float sz = 20;//(5-j.z)*4;
      println(j);
      ellipse(j.x*Width, j.y*Height, sz, sz);
    }
  }
}

Skeleton[] skeleton_list = null;
int n_skeletons = -1;

void setup()
{
  size(Width, Height);
  oscP5 = new OscP5(this, 7777);
  String[] lines;
  try {
    lines = loadStrings("record.txt");
  }
  catch (Exception e)
  {
    lines = loadStrings("../bin/record.txt");
  }
  //  println(lines);
  n_skeletons = lines.length/43;
  skeleton_list = new Skeleton[n_skeletons];
  for (int i=0;i<n_skeletons;i++)
  {
    skeleton_list[i] = new Skeleton();
    skeleton_list[i].player_idx = int(lines[i*(n_joints+1)]);
    for (int j=0;j<n_joints;j++)
    {
      //joint
      String line = lines[i*43+2+j];
      float[] num = float(splitTokens(line, ","));
      skeleton_list[i].joints[j] = new PVector(num[0], num[1], num[2]);
      skeleton_list[i].joints_depth[j] = NuiTransformSkeletonToDepthImageF(skeleton_list[i].joints[j]);

      //orient
      line = lines[i*43+23+j];
      num = float(splitTokens(line, ","));
      skeleton_list[i].orients[j] = new Vector4(num[0], num[1], num[2], num[3]);
    }
  }
  to_client = new NetAddress("localhost", 3333);
  frameRate(60);
}

float idx=0;

void draw()
{
  background(122);
  if (keyPressed)
  {
    idx = (idx+1);
  }
  else
  {
    idx+= 0.03;
  }
  if (idx >= n_skeletons)
    idx = 0;

  //joint
  OscMessage m = new OscMessage("/kinect");
  m.add(0);//device_id
  m.add(1);//player_idx
  for (PVector j:skeleton_list[(int)idx].joints_depth)
  {
    m.add(j.x+","+j.y+","+j.z+",1.0");
  }
  oscP5.send(m, to_client);

  //orient
  m = new OscMessage("/orient");
  m.add(0);//device_id
  m.add(1);//player_idx
  for (Vector4 j:skeleton_list[(int)idx].orients)
  {
    m.add(j.x+","+j.y+","+j.z+","+j.w);
  }
  oscP5.send(m, to_client);

  skeleton_list[(int)idx].draw();
}


PVector NuiTransformSkeletonToDepthImageF( PVector vPoint)
{    
  final float NUI_CAMERA_SKELETON_TO_DEPTH_IMAGE_MULTIPLIER_320x240 = 285.63f;
  //
  // Requires a valid depth value.
  //
  PVector result = new PVector();

  if (vPoint.z > 0.0001)
  {
    //
    // Center of depth sensor is at (0,0,0) in skeleton space, and
    // and (160,120) in depth image coordinates.  Note that positive Y
    // is up in skeleton space and down in image coordinates.
    //        
    result.x = ( 0.5f + vPoint.x * NUI_CAMERA_SKELETON_TO_DEPTH_IMAGE_MULTIPLIER_320x240 / ( vPoint.z * 320.0f ) );
    result.y = ( 0.5f - vPoint.y * NUI_CAMERA_SKELETON_TO_DEPTH_IMAGE_MULTIPLIER_320x240 / ( vPoint.z * 240.0f ) );

    result.z = vPoint.z;
  }

  return result;
}

