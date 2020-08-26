/**
 * MoteinoRead
 * 
 * Read data from the serial port and display
 * Moteino GBM data
 *
 * example input string:   
 * 7745 ID4 -32 G04 09  
 * time[s] ID# RSSI Ident value
 *
 * (C) Michael Blank, 20 Jul 2020
 * 
 */


import processing.serial.*;
import java.util.*;
import processing.net.*; 

Client c; 
String input;
String hexString[];

int RECT_H=120;
int DELTA = 35;
int Y_OCC=35;
int Y_ID=15;
int Y_RSSI=60;
int Y_LAST_REC=210;
int BOTTOM = 240;
color RED = color(255, 0, 0);
color GREEN = color(0, 255, 0);
color GRAY = color(180);
String status="";
int statusTime=0;
int VANISHED_TIME = 500*1000;  // after this number 
// of milliseconds, a GBM is deleted from list


Serial myPort;  // Create object from Serial class
int val;      // Data received from the serial port
ArrayList<GBM> gbms = new ArrayList<GBM>();
String portName = "/dev/ttyACM0";

void setup() 
{
    size(400, 240);  // y=BOTTOM
    String serPorts[] = Serial.list();
    if (Arrays.asList(serPorts).contains("/dev/ttyACM1")) {
        // in the unlikely case that the Arduino is not on ACM0
        portName = "/dev/ttyACM1";
    }
    myPort = new Serial(this, portName, 9600);
    colorMode(RGB, 255, 255, 255);
    ellipseMode(RADIUS);
    frameRate(5);
    surface.setResizable(true);
    
    c = new Client(this, "localhost", 1234); 
}

void draw()
{
    while (myPort.available() > 0) {

        String inBuffer = trim(myPort.readStringUntil('\n'));   
        if (inBuffer != null) {
            println(inBuffer);
            String[] msg =split(inBuffer, ' ');
            evalMsg(msg);
        }
    }
    background(255);             // Set background to white
    fill(0, 0, 0);  // black
    text("ID", 10, Y_ID);
    text("occ", 10, Y_OCC+5);
    text("age", 10, Y_LAST_REC);
    text("rssi", 10, Y_RSSI+60);

    for (int i=0; i < gbms.size(); i++) {
        // draw toti state
        drawOccupancyCircle(i);

        // visualize rssi
        drawRssiRect(i);
        fill(0, 0, 0);
        text(gbms.get(i).ID+"", 45+i*DELTA, Y_ID);
        text((millis() - gbms.get(i).recTime)/1000 +"", 45+i*DELTA, Y_LAST_REC);
    }

    if (gbms.size() == 0) {
        fill(0, 0, 0);
        text("waiting for first serial data...", 50, 50);
    }
    fill(0, 0, 0);
    if (statusTime > millis() ) {
        text(status, 10, BOTTOM-10);
    } else {
        text("--", 10, BOTTOM-10);
    }
    int vanish = checkAges();
    if (vanish !=-1) gbms.remove(vanish);
}

void mousePressed() {
    for (int i=0; i< gbms.size(); i++) {
        if ((mouseX >= 40+i*DELTA)
            && (mouseX <= 40+i*DELTA +20)) {
            fill(0, 0, 0);
            status="GBM#"+gbms.get(i).ID+ "  version="+ gbms.get(i).version;
            statusTime=millis()+10000;
            //modal!! JOptionPane.showMessageDialog(null,"GBM#"+gbms.get(i).ID+ "  "+ gbms.get(i).version);
        }
    }
}

void evalMsg(String[] m) {
    int id = int(m[1].charAt(2)) - int('0');  // 1 character only !! TODO
    int index = getGBM(id);
    char type = m[3].charAt(0);  // g(bm) message or v(ersion) message

    if (index == -1) {   // first time this GBM sends a message
        if (type == 'G') {
            gbms.add(new GBM(id, parseInt(m[2]), parseInt(m[4]) ) );
            sendUpdateToLoconet(id,parseInt(m[4]));
        } else {
            gbms.add(new GBM(id, parseInt(m[2]), m[3]) );
        }
        Collections.sort(gbms);
        return;
    }

    // update message
    GBM upd = gbms.get(index);
    switch(type) {
    case 'G':
        upd.rssi = parseInt(m[2]);
        upd.occupied =  parseInt(m[4]);
        upd.recTime = millis();
        //println(upd.toString());
        if (c != null) sendUpdateToLoconet(upd.ID,upd.occupied);
        break;
    case 'V':
        upd.rssi = parseInt(m[2]);
        upd.version = m[3];
    }
}


void drawRssiRect(int ind) {
    int rs = gbms.get(ind).rssi;
    fill(200, 200, 200);
    rect(40+ind*DELTA, Y_RSSI, 20, RECT_H);
    if ((millis() - gbms.get(ind).recTime ) > 50000) {
        fill(GRAY);
    } else {
        if (rs >= -60) {
            fill(100, 255, 100);  // green
        } else {
            if (rs >= -70) {
                fill(255, 255, 100);  // yellowish
            } else {
                fill(255, 100, 100);  // red
            }
        }
    }

    // scale RSSI=-20 ... RSSI=-80 to a range of 1...0
    int h=RECT_H*(80+rs)/60; 
    if (h <0) h=1;
    if (h>RECT_H) h= RECT_H;
    rect(40+ind*DELTA, Y_RSSI+(RECT_H-h), 20, h);
    fill(0, 0, 0);
    text(rs, 40+ind*DELTA, Y_RSSI+(RECT_H-h)+10);
}

void drawOccupancyCircle(int ind) {

    if (gbms.get(ind).occupied == 0) {
        fill(GREEN);
    } else {
        fill(RED);
    }

    ellipse(50+ind*DELTA, Y_OCC, 10, 10);
}

void sendUpdateToLoconet(int id, int occ) {
    String msg = "B2 ";
    int id2 = (id-1)/2;
    if (id2 < 15) msg += '0';
    msg += Integer.toHexString(id2) + ' ';
    int idBit1 = id & 0x1;
    int val = (4+2*(1 - idBit1)+occ)*16;
    msg += Integer.toHexString(val);
    msg = addChecksumToLNString(msg);
    println(msg); 
    c.write("SEND "+ msg + '\n');
}

int checkAges() {
    for (int i=0; i < gbms.size(); i++) {
        if ((millis() - gbms.get(i).recTime) > VANISHED_TIME) {
            return i;
        }
    }
    return -1; // none found
}

String addChecksumToLNString(String s) {

       String hexS[] = s.split(" ");
       int checksum = 0;
       for (int i=0; i < 3; i++) {
            int d = Integer.parseInt(hexS[i], 16);
            checksum = checksum ^ d;
        }
        checksum = checksum ^ 0xff;
        return s + " " + twoCharFromInt(checksum);

}

String twoCharFromInt(int a) {
    String s = Integer.toHexString(a);
    if (s.length() == 1) s = '0'+s;
    return s.toUpperCase();
}

class GBM implements Comparable<GBM> {
    int ID;
    int rssi;
    int occupied;
    int recTime;
    String version="V?";

    GBM (int id, int r, int o) {
        ID = id;
        rssi = r;
        occupied = o;
        recTime = millis();
    }

    GBM (int id, int r, String v) {
        ID = id;
        rssi = r;
        version = v;
        recTime = millis();
    }

    String toString() {
        return "ID="+ID+" rssi="+rssi;
    }


    public int compareTo(GBM g) {
        if (g.ID > ID) {
            return -1;
        } else {
            return 1;
        }
    }
}

int getGBM(int id) {
    for (int i=0; i< gbms.size(); i++) {
        if (gbms.get(i).ID == id) {
            return i;
        }
    }
    return -1;
}
