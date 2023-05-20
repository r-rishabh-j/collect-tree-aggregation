<?xml version="1.0" encoding="UTF-8"?>
<simconf>
  <project EXPORT="discard">[APPS_DIR]/mrm</project>
  <project EXPORT="discard">[APPS_DIR]/mspsim</project>
  <project EXPORT="discard">[APPS_DIR]/avrora</project>
  <project EXPORT="discard">[APPS_DIR]/serial_socket</project>
  <project EXPORT="discard">[APPS_DIR]/collect-view</project>
  <project EXPORT="discard">[APPS_DIR]/powertracker</project>
  <simulation>
    <title>My simulation</title>
    <randomseed>123456</randomseed>
    <motedelay_us>1000000</motedelay_us>
    <radiomedium>
      org.contikios.cooja.radiomediums.UDGM
      <transmitting_range>50.0</transmitting_range>
      <interference_range>100.0</interference_range>
      <success_ratio_tx>1.0</success_ratio_tx>
      <success_ratio_rx>1.0</success_ratio_rx>
    </radiomedium>
    <events>
      <logoutput>40000</logoutput>
    </events>
    <motetype>
      org.contikios.cooja.mspmote.SkyMoteType
      <identifier>sky1</identifier>
      <description>Sky Mote Type #sky1</description>
      <source EXPORT="discard">[CONTIKI_DIR]/code/example-collect-new.c</source>
      <commands EXPORT="discard">make example-collect-new.sky TARGET=sky</commands>
      <firmware EXPORT="copy">[CONTIKI_DIR]/code/example-collect-new.sky</firmware>
      <moteinterface>org.contikios.cooja.interfaces.Position</moteinterface>
      <moteinterface>org.contikios.cooja.interfaces.RimeAddress</moteinterface>
      <moteinterface>org.contikios.cooja.interfaces.IPAddress</moteinterface>
      <moteinterface>org.contikios.cooja.interfaces.Mote2MoteRelations</moteinterface>
      <moteinterface>org.contikios.cooja.interfaces.MoteAttributes</moteinterface>
      <moteinterface>org.contikios.cooja.mspmote.interfaces.MspClock</moteinterface>
      <moteinterface>org.contikios.cooja.mspmote.interfaces.MspMoteID</moteinterface>
      <moteinterface>org.contikios.cooja.mspmote.interfaces.SkyButton</moteinterface>
      <moteinterface>org.contikios.cooja.mspmote.interfaces.SkyFlash</moteinterface>
      <moteinterface>org.contikios.cooja.mspmote.interfaces.SkyCoffeeFilesystem</moteinterface>
      <moteinterface>org.contikios.cooja.mspmote.interfaces.Msp802154Radio</moteinterface>
      <moteinterface>org.contikios.cooja.mspmote.interfaces.MspSerial</moteinterface>
      <moteinterface>org.contikios.cooja.mspmote.interfaces.SkyLED</moteinterface>
      <moteinterface>org.contikios.cooja.mspmote.interfaces.MspDebugOutput</moteinterface>
      <moteinterface>org.contikios.cooja.mspmote.interfaces.SkyTemperature</moteinterface>
    </motetype>
    <mote>
      <breakpoints />
      <interface_config>
        org.contikios.cooja.interfaces.Position
        <x>72.40325419778446</x>
        <y>12.556168343429873</y>
        <z>0.0</z>
      </interface_config>
      <interface_config>
        org.contikios.cooja.mspmote.interfaces.MspClock
        <deviation>1.0</deviation>
      </interface_config>
      <interface_config>
        org.contikios.cooja.mspmote.interfaces.MspMoteID
        <id>1</id>
      </interface_config>
      <motetype_identifier>sky1</motetype_identifier>
    </mote>
    <mote>
      <breakpoints />
      <interface_config>
        org.contikios.cooja.interfaces.Position
        <x>14.463562210278692</x>
        <y>19.001991173022613</y>
        <z>0.0</z>
      </interface_config>
      <interface_config>
        org.contikios.cooja.mspmote.interfaces.MspClock
        <deviation>1.0</deviation>
      </interface_config>
      <interface_config>
        org.contikios.cooja.mspmote.interfaces.MspMoteID
        <id>2</id>
      </interface_config>
      <motetype_identifier>sky1</motetype_identifier>
    </mote>
    <mote>
      <breakpoints />
      <interface_config>
        org.contikios.cooja.interfaces.Position
        <x>84.46611698418921</x>
        <y>69.5324436298588</y>
        <z>0.0</z>
      </interface_config>
      <interface_config>
        org.contikios.cooja.mspmote.interfaces.MspClock
        <deviation>1.0</deviation>
      </interface_config>
      <interface_config>
        org.contikios.cooja.mspmote.interfaces.MspMoteID
        <id>3</id>
      </interface_config>
      <motetype_identifier>sky1</motetype_identifier>
    </mote>
    <mote>
      <breakpoints />
      <interface_config>
        org.contikios.cooja.interfaces.Position
        <x>37.964059135713725</x>
        <y>80.22074300617895</y>
        <z>0.0</z>
      </interface_config>
      <interface_config>
        org.contikios.cooja.mspmote.interfaces.MspClock
        <deviation>1.0</deviation>
      </interface_config>
      <interface_config>
        org.contikios.cooja.mspmote.interfaces.MspMoteID
        <id>4</id>
      </interface_config>
      <motetype_identifier>sky1</motetype_identifier>
    </mote>
    <mote>
      <breakpoints />
      <interface_config>
        org.contikios.cooja.interfaces.Position
        <x>0.40436929188942816</x>
        <y>33.14988191970954</y>
        <z>0.0</z>
      </interface_config>
      <interface_config>
        org.contikios.cooja.mspmote.interfaces.MspClock
        <deviation>1.0</deviation>
      </interface_config>
      <interface_config>
        org.contikios.cooja.mspmote.interfaces.MspMoteID
        <id>5</id>
      </interface_config>
      <motetype_identifier>sky1</motetype_identifier>
    </mote>
    <mote>
      <breakpoints />
      <interface_config>
        org.contikios.cooja.interfaces.Position
        <x>63.63204368735086</x>
        <y>86.05098808338913</y>
        <z>0.0</z>
      </interface_config>
      <interface_config>
        org.contikios.cooja.mspmote.interfaces.MspClock
        <deviation>1.0</deviation>
      </interface_config>
      <interface_config>
        org.contikios.cooja.mspmote.interfaces.MspMoteID
        <id>6</id>
      </interface_config>
      <motetype_identifier>sky1</motetype_identifier>
    </mote>
    <mote>
      <breakpoints />
      <interface_config>
        org.contikios.cooja.interfaces.Position
        <x>9.762392608715304</x>
        <y>60.06490720332074</y>
        <z>0.0</z>
      </interface_config>
      <interface_config>
        org.contikios.cooja.mspmote.interfaces.MspClock
        <deviation>1.0</deviation>
      </interface_config>
      <interface_config>
        org.contikios.cooja.mspmote.interfaces.MspMoteID
        <id>7</id>
      </interface_config>
      <motetype_identifier>sky1</motetype_identifier>
    </mote>
    <mote>
      <breakpoints />
      <interface_config>
        org.contikios.cooja.interfaces.Position
        <x>18.921812798794917</x>
        <y>-0.6230726491126228</y>
        <z>0.0</z>
      </interface_config>
      <interface_config>
        org.contikios.cooja.mspmote.interfaces.MspClock
        <deviation>1.0</deviation>
      </interface_config>
      <interface_config>
        org.contikios.cooja.mspmote.interfaces.MspMoteID
        <id>8</id>
      </interface_config>
      <motetype_identifier>sky1</motetype_identifier>
    </mote>
    <mote>
      <breakpoints />
      <interface_config>
        org.contikios.cooja.interfaces.Position
        <x>41.72187314734263</x>
        <y>36.37553613698724</y>
        <z>0.0</z>
      </interface_config>
      <interface_config>
        org.contikios.cooja.mspmote.interfaces.MspClock
        <deviation>1.0</deviation>
      </interface_config>
      <interface_config>
        org.contikios.cooja.mspmote.interfaces.MspMoteID
        <id>9</id>
      </interface_config>
      <motetype_identifier>sky1</motetype_identifier>
    </mote>
    <mote>
      <breakpoints />
      <interface_config>
        org.contikios.cooja.interfaces.Position
        <x>75.95139965341636</x>
        <y>41.66766829916874</y>
        <z>0.0</z>
      </interface_config>
      <interface_config>
        org.contikios.cooja.mspmote.interfaces.MspClock
        <deviation>1.0</deviation>
      </interface_config>
      <interface_config>
        org.contikios.cooja.mspmote.interfaces.MspMoteID
        <id>10</id>
      </interface_config>
      <motetype_identifier>sky1</motetype_identifier>
    </mote>
  </simulation>
  <plugin>
    org.contikios.cooja.plugins.SimControl
    <width>280</width>
    <z>0</z>
    <height>160</height>
    <location_x>400</location_x>
    <location_y>0</location_y>
  </plugin>
  <plugin>
    org.contikios.cooja.plugins.Visualizer
    <plugin_config>
      <moterelations>true</moterelations>
      <skin>org.contikios.cooja.plugins.skins.IDVisualizerSkin</skin>
      <skin>org.contikios.cooja.plugins.skins.GridVisualizerSkin</skin>
      <skin>org.contikios.cooja.plugins.skins.TrafficVisualizerSkin</skin>
      <skin>org.contikios.cooja.plugins.skins.UDGMVisualizerSkin</skin>
      <viewport>2.4916481465104963 0.0 0.0 2.4916481465104963 111.73525602713364 40.529750538575584</viewport>
    </plugin_config>
    <width>400</width>
    <z>3</z>
    <height>400</height>
    <location_x>1</location_x>
    <location_y>1</location_y>
  </plugin>
  <plugin>
    org.contikios.cooja.plugins.LogListener
    <plugin_config>
      <filter>Sink</filter>
      <formatted_time />
      <coloring />
    </plugin_config>
    <width>1308</width>
    <z>1</z>
    <height>506</height>
    <location_x>400</location_x>
    <location_y>160</location_y>
  </plugin>
  <plugin>
    org.contikios.cooja.plugins.TimeLine
    <plugin_config>
      <mote>0</mote>
      <mote>1</mote>
      <mote>2</mote>
      <mote>3</mote>
      <mote>4</mote>
      <mote>5</mote>
      <mote>6</mote>
      <mote>7</mote>
      <mote>8</mote>
      <mote>9</mote>
      <showRadioRXTX />
      <showRadioHW />
      <showLEDs />
      <zoomfactor>500.0</zoomfactor>
    </plugin_config>
    <width>1708</width>
    <z>5</z>
    <height>166</height>
    <location_x>0</location_x>
    <location_y>798</location_y>
  </plugin>
  <plugin>
    org.contikios.cooja.plugins.Notes
    <plugin_config>
      <notes>Enter notes here</notes>
      <decorations>true</decorations>
    </plugin_config>
    <width>1028</width>
    <z>4</z>
    <height>160</height>
    <location_x>680</location_x>
    <location_y>0</location_y>
  </plugin>
  <plugin>
    org.contikios.cooja.plugins.ScriptRunner
    <plugin_config>
      <script>TIMEOUT(400000);&#xD;
allm = sim.getMotes();&#xD;
startTime=time;&#xD;
log.log("Starttime:"+startTime+"\n");&#xD;
var ref = ["Hello", "Hello", "Hello", "Hello"];&#xD;
var last = [];&#xD;
var ctr = 0;&#xD;
var arr = [];&#xD;
var range = [45];   // radius of every event&#xD;
var count = allm.length;&#xD;
&#xD;
var s_time = [];&#xD;
var e_time = [];&#xD;
var summ = [];&#xD;
var count_elem = [];&#xD;
while (count &gt; 0) {&#xD;
    if (msg.contains("Starting")) {&#xD;
        // log.log("New Mote Found \n");&#xD;
        arr[id - 1] = mote;&#xD;
        // log.log("Nodes " + id + "at : " + arr[id - 1].getInterfaces().getPosition().getXCoordinate() + " , " + arr[id - 1].getInterfaces().getPosition().getYCoordinate() + "\n");&#xD;
        // msg = "Co-ord at : " + arr[id - 1].getInterfaces().getPosition().getXCoordinate() + " " + arr[id - 1].getInterfaces().getPosition().getYCoordinate();&#xD;
        // write(mote,msg);&#xD;
        count = count - 1;&#xD;
    }&#xD;
    YIELD();&#xD;
}&#xD;
// log.log("******\n");&#xD;
// for (var i = 0; i &lt; allm.length; i++) {&#xD;
//     log.log("Nodes " + i + 1 + "at : " + arr[i].getInterfaces().getPosition().getXCoordinate() + " , " + arr[i].getInterfaces().getPosition().getYCoordinate() + "\n");&#xD;
&#xD;
// }&#xD;
// log.log("******\n");&#xD;
var cc = 20;&#xD;
var xi = -10;&#xD;
var xj = 0;&#xD;
&#xD;
send_times = []&#xD;
&#xD;
while (cc &gt; 0) {&#xD;
    var refer_msg = msg;&#xD;
    cc = cc - 1;&#xD;
    var x_size = 100;  // max horizontal length&#xD;
    var y_size = 100;	// max vertical length&#xD;
    // var k = Math.floor((Math.random() * ref.length));&#xD;
    var radius = range[Math.floor((Math.random() * range.length))];&#xD;
&#xD;
    msg = "ID:" + ctr; // generating a message&#xD;
&#xD;
&#xD;
    var p = 5000000;&#xD;
    while (p &gt; 0) {&#xD;
        p = p - 1;&#xD;
    }&#xD;
    var flag = 0;&#xD;
    //WAIT_UNTIL (msg.equals(ref[k]));&#xD;
    // var x = Math.floor((Math.random() * x_size));  // &#xD;
    // var y = Math.floor((Math.random() * y_size));  // generate event at x,y &#xD;
    // x = 20;   // in case you want to fix the location of event&#xD;
    // y = 20;   &#xD;
    var x = xi;&#xD;
	var y = xj;&#xD;
&#xD;
	xj=xj+30;&#xD;
&#xD;
	if(xj&gt;90)&#xD;
	{&#xD;
		xj=0;&#xD;
		xi=xi+30;		&#xD;
	}&#xD;
&#xD;
    // log.log("Event : " + msg + " was generated on " + time + " at ( " + x + " , " + y + " ) \n");&#xD;
    s_time[ctr] = time;  // stores the start time&#xD;
    summ[ctr] = 0;&#xD;
    count_elem[ctr] = 0;&#xD;
    send_times[ctr]=time;&#xD;
    for (var i = 0; i &lt; allm.length; i++) {&#xD;
        mote = arr[i];&#xD;
        var d_x = mote.getInterfaces().getPosition().getXCoordinate();&#xD;
        var d_y = mote.getInterfaces().getPosition().getYCoordinate();&#xD;
        var dist = Math.sqrt((d_x - x) * (d_x - x) + (d_y - y) * (d_y - y));&#xD;
        if (dist &lt;= radius) {&#xD;
            write(mote, msg);&#xD;
            // log.log("Event : " + msg + " with radius : " + radius + " sensed by mote Number : " + i + "'\n");&#xD;
            flag = 1;&#xD;
        }&#xD;
    }&#xD;
    // if (flag == 0) {&#xD;
    //     log.log("Event : " + msg + " with radius : " + radius + " was out of range from sensors'\n");&#xD;
    // }&#xD;
    ctr++;&#xD;
    YIELD();&#xD;
}&#xD;
&#xD;
receive_times = []&#xD;
log.log(send_times);&#xD;
log.log("\n");&#xD;
&#xD;
while (1 &gt; 0) {&#xD;
    var refer_msg = msg;&#xD;
    if (refer_msg.contains("Sink got message")) {&#xD;
        var a = refer_msg.split("=&gt;")[1]&#xD;
        if (a == undefined) {&#xD;
            continue;&#xD;
        }&#xD;
        a = a.split(":")[1]&#xD;
        if (a == undefined) {&#xD;
            continue;&#xD;
        }&#xD;
        a = a.split("|")[0];&#xD;
        delta = (time-send_times[a]);&#xD;
        log.log("T=&gt;EventID:" + a + " received at:" + delta+"\n");&#xD;
        if (receive_times[a] == undefined) {&#xD;
            receive_times[a] = [];&#xD;
        }&#xD;
        receive_times[a].push(time);		&#xD;
    }&#xD;
    YIELD();&#xD;
}</script>
      <active>true</active>
    </plugin_config>
    <width>600</width>
    <z>2</z>
    <height>600</height>
    <location_x>8</location_x>
    <location_y>401</location_y>
  </plugin>
</simconf>

