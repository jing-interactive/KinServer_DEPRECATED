import oscP5.*;

OscP5 oscP5;//OSC对象

class vBlob
{
  public vBlob(float _cx, float _cy, float _w, float _h)
  {
    cx = _cx;
    cy = _cy;
    w = _w;
    h = _h;
    //对新添加的成员进行初始化
    isHole = false;
    points = new ArrayList();
  }
  float cx,cy;//中心点的坐标
  float w,h;//宽度和高度

  public void draw()//进行绘制
  {
    if (points.size() > 0)//CamServer开启了detail模式后points会包含轮廓点，因此它的size会大于零
      drawContour();
    else//否则仅仅是显示一个白色的包围框
    {
      fill(255);
      rect( cx, cy, w, h);
    }
  }

  //++++++++++++++++第四课新添加的
  boolean isHole;//是否是洞
  ArrayList points;//保存轮廓点

  public void drawContour()
  {
    if (isHole)//如果是洞，使用不同的颜色
      fill(255,0,0);
    else
      fill(255,255,255);
    beginShape();
    for (int j=0; j<points.size();j++ )
    {
      PVector pos = (PVector)points.get(j);
      vertex(pos.x, pos.y);
    }
    endShape(CLOSE); 
  }
}

ArrayList blobList = new ArrayList();

void setup() {
  size(640, 480);
  oscP5 = new OscP5(this,7777);
}

void draw()
{
  background(122);

  for (int i=0;i<blobList.size();i++)
  {
    vBlob obj = (vBlob)blobList.get(i);
    obj.draw();
  }
}

void oscEvent(OscMessage msg) 
{
  if(msg.checkAddrPattern("/start")) 
  {// 收到 /start 意味着一次更新的开始
    blobList.clear();//将blobList清空，因为我们要把全新的数据塞进去了
  }
  else
  {
    boolean isContour = msg.checkAddrPattern("/contour");
    boolean isHole = msg.checkAddrPattern("/hole");
    // 这三者的含义上节课都有讲哦

    if (isContour || isHole)
    {
      float cx = width*msg.get(2).floatValue();
      float cy = height*msg.get(3).floatValue();
      float w = width*msg.get(4).floatValue();
      float h = height*msg.get(5).floatValue();
      vBlob obj = new vBlob(cx, cy, w, h);

      //++++++++++++++++第四课新添加的
      if (isContour || isHole)
      {
        int idx_npts = 7;//顶点个数处于消息的第7位，见第三课中的消息定义
        int nPts =  msg.get(idx_npts).intValue();
        for (int i=0; i < nPts; i++)//依次读取每个坐标(x, y)，添加到obj的points数组中。
        { 
          float px = width*msg.get(idx_npts+1+i*2).floatValue();
          float py = height*msg.get(idx_npts+2+i*2).floatValue();
          obj.points.add(new PVector(px,py));
        }
        obj.isHole = isHole;
      }
      blobList.add(obj);
    }
  }
}

