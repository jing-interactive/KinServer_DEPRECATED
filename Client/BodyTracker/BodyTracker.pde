import oscP5.*;
import netP5.*;

final int Width = 800;
final int Height = 600;

final float FOCAL_LENGTH = 285.63f;
OscP5 oscP5;//OSC对象

final int n_joints = 20;
class Player
{
  boolean visible = false;
  int idx = 0;
  PVector joints[] = new PVector[n_joints];
  void reset()
  {
    visible = false;
  }
  void setup(OscMessage msg)
  {
    visible = true;
    for (int i=0;i<n_joints;i++)
    {
      String m = msg.get(2+i).stringValue();
      float[] num = float(splitTokens(m, ","));
      joints[i] = new PVector(num[0], num[1], num[2]);
    }
  }

  void draw()
  {
    if (!visible)
      return;
    fill(0);
    for (PVector j:joints)
    {
      float sz = 20;//(5-j.z)*4;
      ellipse(j.x*Width, j.y*Height, sz, sz);
    }
  }
}
final int n_players = 6;
Player[] players = new Player[n_players];
ArrayList blobList = new ArrayList();

void setup() {
  size(Width, Height);
  oscP5 = new OscP5(this, 7777);
  for (int i=0;i<n_players; i++)
  {
    players[i] = new Player();
    players[i].idx = i;
  }
}

void draw()
{
  background(122);

  for (Player p: players)
  {
    p.draw();
  }
}

void oscEvent(OscMessage msg) 
{
  if (msg.checkAddrPattern("/kinect")) 
  {
//    for (Player p: players)
//      p.reset();
     println(msg);
    int id = msg.get(0).intValue();
    println("player: "+id);
    players[id].setup(msg);
  }
}
