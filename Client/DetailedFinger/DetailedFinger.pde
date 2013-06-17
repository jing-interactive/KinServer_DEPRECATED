import oscP5.*;
import netP5.*;

final int Width = 800;
final int Height = 600;

OscP5 oscP5;//OSC对象

class Hand
{
  float x, y;
  boolean pressed = false;
  void draw()
  {
    if (pressed)
    {
      ellipse(x, y,10,10);
    }
    else
    {
      ellipse(x, y,20,20);
    }
  }
}

Hand[] hands = new Hand[2];

void setup() {
  size(Width, Height);
  oscP5 = new OscP5(this, 3333);//3333是 KinectOsc 的端口号
  ellipseMode(CENTER);
  for (int i=0;i<2;i++)
   hands[i] = new Hand();
}

void draw()
{
  background(122);
  for (Hand h:hands)
    h.draw();
}

String[] addr = {
  "/left_hand_open", "/left_hand_close", 
  "/right_hand_open", "/right_hand_close"
};

void oscEvent(OscMessage msg) 
{
  for (int i=0;i<4;i++)
  {
    if (msg.checkAddrPattern(addr[i])) 
    {
      float x = width*msg.get(0).floatValue();
      float y = height*msg.get(1).floatValue();
      int idx = i/2;
      boolean pressed = (i%2 == 1);
      hands[idx].x = x;
      hands[idx].y = y;
      hands[idx].pressed = pressed;
      break;
    }
  }
}

