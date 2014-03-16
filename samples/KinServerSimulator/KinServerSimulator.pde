import oscP5.*;
import netP5.*;

NetAddress myRemoteLocation;
OscP5 oscP5;
String info = "";

final int kListenPort = 5555;
final int kSendPort = 7001;

void setup() {
    size(400, 400);
    frameRate(25);

    oscP5 = new OscP5(this, kListenPort); 
    myRemoteLocation = new NetAddress("localhost", kSendPort);
}

void draw() {
    background(0); 
    text("My ip address is "+oscP5.ip(), 40, 40);
    text("Press 'a' to send /kinect heart beat", 40, 100);
    text("Press 'd' to send /kinect push gesture", 40, 140);
    text("Target : "+myRemoteLocation.address(), 40, 180);
    text(info, 10, 240);
    if (keyPressed)
    {
        OscMessage m = new OscMessage("/kinect");
        if (key == 'a')
        {
        }
        else if (key =='d')
        {
        }
        println("sent");
        oscP5.send(m, myRemoteLocation);
    }
}

void oscEvent(OscMessage m) {
    m.print();
    info = "### received "+m.addrPattern();
    String newip = m.address().substring(1);
    if (!myRemoteLocation.address().equals(newip))
    {
        // update ip
        println("new ip : "+newip);
        myRemoteLocation = new NetAddress(newip, kSendPort);
    }
}

