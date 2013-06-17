'''
win32 mouse key emulator
read kinect impormation from KinectOsc.exe via osc protocol
press F4 to start emulation
press ESC to stop emulation
'''

from simpleOSC import *
import win32api, win32con

print "start zen_mouse"

initOSCServer('127.0.0.1', 7777)

start = False

def key_down(vk_code):
	return win32api.GetAsyncKeyState(vk_code) & 0x8000
	
# define a message-handler function for the server to call.
def printing_handler(addr, tags, data, source):
    left_hand_id = 7
    right_hand_id = 11
    #print "---"
    print "received new osc msg from %s" % getUrlStr(source)
#    print "with addr : %s" % addr
#    print "typetags : %s" % tags
#    print "---"
    #print data
    
    left_hand = data[1+left_hand_id].split(',')
    
    right_hand = data[1+right_hand_id].split(',')
    
    x = float(right_hand[0])
    y = float(right_hand[1])
    # x = (x-0.5)*1.2+0.5;
    # y = (y-0.5)*1.2+0.5;
    
    cx = int(x*65536)#win32api.GetSystemMetrics(0))
    cy = int(y*65536)#win32api.GetSystemMetrics(1))
    #print cx,cy
	
    if start == True:
        #win32api.SetCursorPos((cx,cy))
        win32api.mouse_event(win32con.MOUSEEVENTF_LEFTDOWN|win32con.MOUSEEVENTF_MOVE|win32con.MOUSEEVENTF_ABSOLUTE,cx,cy)
        #win32api.mouse_event(win32con.MOUSEEVENTF_LEFTDOWN,0,0)
        #win32api.mouse_event(win32con.MOUSEEVENTF_LEFTUP,0,0)

setOSCHandler("/kinect", printing_handler) # adding our function

# just checking which handlers we have added. not really needed
reportOSCHandlers()

VK_ESCAPE = 0x1B
VK_F4 = 0x73


try :
    while 1 :
        if key_down(VK_F4):# and key_down(VK_F4):
            global start
            start = True
            print "start"
        if key_down(VK_ESCAPE):# and key_down(VK_F4):
            global start
            start = False
            print "Stop"
        time.sleep(1)

except KeyboardInterrupt :
    print "\nClosing OSCServer."
    print "Waiting for Server-thread to finish"
    closeOSC()
    print "Done"
        
