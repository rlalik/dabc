DABC = {};

DABC.version = "2.3.1";

DABC.mgr = 0;

DABC.dabc_tree = 0;   // variable to display hierarchy

DABC.load_root_js = 0; // 0 - not started, 1 - doing load, 2 - completed

/*
if (!Object.create) {
   Object.create = (function(){
       function F(){}

       return function(o){
           if (arguments.length != 1) {
               throw new Error('Object.create implementation only accepts one parameter.');
           }
           F.prototype = o
           return new F()
       }
   })()
}
*/

DABC.ntou4 = function(b, o) {
   // convert (read) four bytes of buffer b into a uint32_t
   var n  = (b.charCodeAt(o)   & 0xff);
       n += (b.charCodeAt(o+1) & 0xff) << 8;
       n += (b.charCodeAt(o+2) & 0xff) << 16;
       n += (b.charCodeAt(o+3) & 0xff) << 24;
   return n;
}

DABC.AssertRootPrerequisites = function() {
   if (DABC.load_root_js == 0) {
      console.log("start loading scripts");
      DABC.load_root_js = 1;
      loadScript('jsrootiosys/scripts/jquery.mousewheel.js', function() {
      loadScript('jsrootiosys/scripts/rawinflate.js', function() {
      loadScript('jsrootiosys/scripts/JSRootCore.js', function() {
      loadScript('jsrootiosys/scripts/three.min.js', function() {
      loadScript('jsrootiosys/fonts/helvetiker_regular.typeface.js', function() {
      loadScript('jsrootiosys/scripts/JSRootIOEvolution.js', function() {
      loadScript('jsrootiosys/scripts/JSRootD3ExpPainter.js', function() {
         DABC.load_root_js = 2;
         gStyle.OptimizeDraw = true;
         console.log("loading all scripts done");
      }) }) }) }) }) }) });
   }
   
   return (DABC.load_root_js == 2);
};


DABC.UnpackBinaryHeader = function(arg) {
   if ((arg==null) || (arg.length < 20)) return null;
   
   //console.log("Get header of length " + arg.length);
   
   var o = 0;
   var typ = DABC.ntou4(arg, o); o+=4;

   //console.log("Get header of typ " + typ);

   if (typ!=1) return null;
   
   var hdr = {};
   
   hdr['typ'] = typ;
   hdr['version'] = DABC.ntou4(arg, o); o+=4;
   hdr['master_version'] = DABC.ntou4(arg, o); o+=4;
   hdr['zipped'] = DABC.ntou4(arg, o); o+=4;
   hdr['payload'] = DABC.ntou4(arg, o); o+=4;
   
   hdr['rawdata'] = "";

   //console.log("Get header with payload " + hdr.payload);
   
   // if no payload specified, ignore
   if (hdr.payload == 0) return hdr;
   
   if (hdr.zipped == 0) {
      hdr['rawdata'] = arg.slice(o);
      if (hdr['rawdata'].length != hdr.payload)
         alert("mismatch between payload value and actual data length");
      return hdr;
   }

   var ziphdr = JSROOTIO.R__unzip_header(arg, o, true);
   
   if (!ziphdr) {
      alert("no zip header but it was specified!!!");
      return null;
   }
  
   var unzip_buf = JSROOTIO.R__unzip(ziphdr['srcsize'], arg, o, true);
   if (!unzip_buf) {
      alert("fail to unzip data");
      return false;
   } 
   
   hdr['rawdata'] = unzip_buf['unzipdata'];
   
   if (hdr['rawdata'].length != hdr.zipped)
      alert("mismatch between length of buffer before zip and actual data length");
   
   unzip_buf = null;
   return hdr;
}

DABC.nextXmlNode = function(node)
{
   while (node && (node.nodeType!=1)) node = node.nextSibling;
   return node;
}

DABC.TopXmlNode = function(xmldoc) 
{
   if (!xmldoc) return;
   
   return DABC.nextXmlNode(xmldoc.firstChild);
}


// ============= start of DrawElement ================================= 

DABC.DrawElement = function() {
   this.itemname = "";   // full item name in hierarhcy
   this.version = new Number(-1);    // check which version of element is drawn
   this.frameid = "";
   return this;
}

//method called when item is activated (clicked)
//each item can react by itself
DABC.DrawElement.prototype.ClickItem = function() { return; }

// method regularly called by the manager
// it is responsibility of item perform any action
DABC.DrawElement.prototype.RegularCheck = function() { return; }


DABC.DrawElement.prototype.CreateFrames = function(topid,cnt) {
   this.frameid = "dabc_dummy_" + cnt;

   var entryInfo = 
      "<div id='" +this.frameid + "'>" + 
      "<h2> CreateFrames for item " + this.itemname + " not implemented </h2>"+
      "</div>"; 
   $(topid).append(entryInfo);
}

DABC.DrawElement.prototype.IsDrawn = function() {
   if (!this.frameid) return false;
   if (!document.getElementById(this.frameid)) return false;
   return true;
}

DABC.DrawElement.prototype.Clear = function() {
   // one should remove here all running requests
   // delete objects
   // remove GUI
   
   if (this.frameid.length>0) {
      var elem = document.getElementById(this.frameid);
      if (elem!=null) elem.parentNode.removeChild(elem);
   }
   
   this.itemname = "";
   this.frameid = 0; 
   this.version = -1;
   this.frameid = "";
}


DABC.DrawElement.prototype.FullItemName = function() {
   // method should return absolute path of the item
   if ((this.itemname.length > 0) && (this.itemname[0] == '/')) return this.itemname;
   
   var curpath = DABC.mgr.GetCurrentPath();
   if (curpath.length == 0) curpath = document.location.pathname;
   
   return curpath + this.itemname; 
}

// ========= start of CommandDrawElement

DABC.CommandDrawElement = function() {
   DABC.DrawElement.call(this);
   this.req = null;
   this.xmlnode = null; // here is xml description of command, which should be first requested
   return this;
}

DABC.CommandDrawElement.prototype = Object.create( DABC.DrawElement.prototype );

DABC.CommandDrawElement.prototype.CreateFrames = function(topid,cnt) {
   this.frameid = "dabc_command_" + cnt;

   var entryInfo = "<div id='" + this.frameid + "'/>";
   
   $(topid).empty();
   $(topid).append(entryInfo);
   
   this.ShowCommand();
}

DABC.CommandDrawElement.prototype.NumArgs = function() {
   if (this.xmlnode==null) return 0;
   
   return this.xmlnode.getAttribute("numargs");
}

DABC.CommandDrawElement.prototype.ArgName = function(n) {
   if (n>=this.NumArgs()) return "";
   
   return this.xmlnode.getAttribute("arg"+n);
}

DABC.CommandDrawElement.prototype.ShowCommand = function() {
   
   var frame = $("#" + this.frameid);
   
   frame.empty();
   
   frame.append("<h3>" + this.FullItemName() + "</h3>");

   if (this.xmlnode==null) {
      frame.append("request command definition...<br>");
      return;
   } 
   
   var entryInfo = "<input type='button' title='Execute' value='Execute' onclick=\"DABC.mgr.ExecuteCommand('" + this.itemname + "')\"/><br>";

   for (var cnt=0;cnt<this.NumArgs();cnt++) {
      var argname = this.ArgName(cnt);
      var argid = this.frameid + "_arg" + cnt; 

      entryInfo += "Arg: " + argname + " "; 
      entryInfo += "<input id='" + argid + "' style='width:60px' value='3' argname = '" + argname + "'/>";    
      entryInfo += "<br>";
   }
   
   entryInfo += "<div id='" +this.frameid + "_res'/>";
   
   frame.append(entryInfo);

   for (var cnt=0;cnt<this.NumArgs();cnt++) {
      var argid = this.frameid + "_arg" + cnt;
      $("#"+argid).spinner({ min:1, max:100});
   }
}


DABC.CommandDrawElement.prototype.Clear = function() {
   
   DABC.DrawElement.prototype.Clear.call(this);
   
   if (this.req) this.req.abort(); 
   this.req = null;          
}

DABC.CommandDrawElement.prototype.ClickItem = function() {
}

DABC.CommandDrawElement.prototype.RegularCheck = function() {
}

DABC.CommandDrawElement.prototype.RequestCommand = function() {
   if (this.req) return;

   var url = this.itemname + "get.xml";

   this.req = DABC.mgr.NewHttpRequest(url, true, false, this);

   this.req.send(null);
}

DABC.CommandDrawElement.prototype.InvokeCommand = function() {
   if (this.req) return;
   
   var resdiv = $("#" + this.frameid + "_res");
   if (resdiv) resdiv.empty();
   
   var url = this.itemname + "execute";

   for (var cnt=0;cnt<this.NumArgs();cnt++) {
      var argid = this.frameid + "_arg" + cnt;
      
      var arginp = $("#"+argid);
      
      if (cnt==0) url+="?"; else url+="&";
      
      url += arginp.attr("argname");
      url += "=";
      url += arginp.spinner("value");
   }
   
   this.req = DABC.mgr.NewHttpRequest(url, true, false, this);

   this.req.send(null);
}

DABC.CommandDrawElement.prototype.RequestCallback = function(arg) {
   // in any case, request pointer will be reseted
   // delete this.req;
   this.req = null;
   
   if (arg==null) {
      console.log("no xml response from server");
      return;
   }
   
   var top = DABC.TopXmlNode(arg);
   var cmdnode = DABC.nextXmlNode(top.firstChild);
   
   if (this.xmlnode==null) {
      // we are waiting for the command itself
      this.xmlnode = cmdnode;
      this.ShowCommand();
      return;
   }
   
   var resdiv = $("#" + this.frameid + "_res");
   if (resdiv) {
      resdiv.empty();
      
      if (cmdnode)
         resdiv.append("<h5>Get reply res=" + cmdnode.getAttribute("_Result_") + "</h5>");
      else   
         resdiv.append("<h5>Get reply without command?</h5>");
   }
}


//========== start of HistoryDrawElement

DABC.HistoryDrawElement = function(_clname) {
   DABC.DrawElement.call(this);

   this.clname = _clname;
   this.req = null;           // request to get raw data
   this.xmlnode = null;       // xml node with current values 
   this.xmlhistory = null;    // array with previous history
   this.xmllimit = 0;         // maximum number of history entries
   this.force = true;
   
   return this;
}

DABC.HistoryDrawElement.prototype = Object.create( DABC.DrawElement.prototype );

DABC.HistoryDrawElement.prototype.EnableHistory = function(hlimit) {
   this.xmllimit = hlimit;
}

DABC.HistoryDrawElement.prototype.isHistory = function() {
   return this.xmllimit > 0;
}


DABC.HistoryDrawElement.prototype.Clear = function() {
   
   DABC.DrawElement.prototype.Clear.call(this);
   
   this.xmlnode = null;       // xml node with current values 
   this.xmlhistory = null;    // array with previous history
   this.xmllimit = 100;       // maximum number of history entries  
   if (this.req) this.req.abort(); 
   this.req = null;          // this is current request
   this.force = true;
}

DABC.HistoryDrawElement.prototype.CreateFrames = function(topid, id) {
}

DABC.HistoryDrawElement.prototype.ClickItem = function() {
   if (this.req != null) return; 

   if (!this.IsDrawn()) 
      this.CreateFrames(DABC.mgr.NextCell(), DABC.mgr.cnt++);
   this.force = true;
   
   this.RegularCheck();
}

DABC.HistoryDrawElement.prototype.RegularCheck = function() {

   // no need to do something when req not completed
   if (this.req!=null) return;
 
   // do update when monitoring enabled
   if ((this.version >= 0) && !this.force) {
      var chkbox = document.getElementById("monitoring");
      if (!chkbox || !chkbox.checked) return;
   }
        
   var url = this.itemname + "get.xml";
   var separ = "?";

   // console.log("GetHistory current version = " + this.version);
   
   if (this.version>0) { url += separ + "version=" + this.version; separ = "&"; } 
   if (this.xmllimit>0) url += separ + "history=" + this.xmllimit;
   this.req = DABC.mgr.NewHttpRequest(url, true, false, this);

   this.req.send(null);

   this.force = false;
}

DABC.HistoryDrawElement.prototype.ExtractField = function(name, kind, node) {
   
   if (!node) node = this.xmlnode;    
   if (!node) return;

   var val = node.getAttribute(name);
   if (!val) return;
   
   if (kind=="number") return Number(val); 
   if (kind=="time") {
      //return Number(val);
      var d  = new Date(val);
      return d.getTime() / 1000.;
   }
   
   return val;
}

DABC.HistoryDrawElement.prototype.ExtractSeries = function(name, kind) {

   // xml node must have attribute, which will be extracted
   var val = this.ExtractField(name, kind, this.xmlnode);
   if (!val) return;
   
   var arr = new Array();
   arr.push(val);
   
   if (this.xmlhistory) 
      for (var n=this.xmlhistory.length-1;n>=0;n--) {
         var newval = this.ExtractField(name, kind, this.xmlhistory[n]);
         if (newval) val = newval;
         arr.push(val);
      }

   arr.reverse();
   return arr;
}


DABC.HistoryDrawElement.prototype.RequestCallback = function(arg) {
   // in any case, request pointer will be reseted
   // delete this.req;
   this.req = null;
   
   if (arg==null) {
      console.log("no xml response from server");
      return;
   }
   
   var top = DABC.TopXmlNode(arg);

//   console.log("Get request callback " + top.getAttribute("dabc:version") + "  or " + top.getAttribute("version"));
   
   var new_version = Number(top.getAttribute("dabc:version"));
   
   // top.getAttribute("itemname")  // one could rechek itemname

   console.log("Get history request callback version = " + new_version);

   var modified = (this.version != new_version);

   this.version = new_version;

   // this is xml node with all fields
   this.xmlnode = DABC.nextXmlNode(top.firstChild);

   if (this.xmlnode == null) {
      console.log("Did not found node with item attributes");
      return;
   }
   
   // this is node <history> 
   var historynode = DABC.nextXmlNode(this.xmlnode.firstChild);
   
   if ((historynode!=null) && (this.xmllimit>0)) {
  
      // gap indicates that we could not get full history relative to provided version number
      var gap = historynode.getAttribute("gap");
      
      // this is first node of kind <h value="abc" time="aaa"> 
      var hnode = DABC.nextXmlNode(historynode.firstChild);
   
      var arr = new Array
      while (hnode!=null) {
         arr.push(hnode);
         hnode = DABC.nextXmlNode(hnode.nextSibling);
      }

      // join both arrays with history entries
      if ((this.xmlhistory == null) || (arr.length >= this.xmllimit) || (gap!=null)) {
         this.xmlhistory = arr;
      } else
         if (arr.length>0) {
            modified = true;
            var total = this.xmlhistory.length + arr.length; 
            if (total > this.xmllimit) 
               this.xmlhistory.splice(0, total - this.xmllimit);

            this.xmlhistory = this.xmlhistory.concat(arr);
         }

      console.log("History length = " + this.xmlhistory.length);
   }
   
   if (modified) this.DrawHistoryElement();
}


DABC.HistoryDrawElement.prototype.DrawHistoryElement = function()
{
   // should be implemented in derived class
   alert("HistoryDrawElement.DrawHistoryElement not implemented for item " + this.itemname);
}

// ======== end of DrawElement ======================

// ======== start of GaugeDrawElement ======================

DABC.GaugeDrawElement = function() {
   DABC.HistoryDrawElement.call(this, "gauge");
   this.gauge = 0;
}

// TODO: check how it works in different older browsers
DABC.GaugeDrawElement.prototype = Object.create( DABC.HistoryDrawElement.prototype );

DABC.GaugeDrawElement.prototype.CreateFrames = function(topid, id) {

   this.frameid = "dabc_gauge_" + id;
   this.min = 0;
   this.max = 1;
   this.gauge = null;
   
//   var entryInfo = "<div id='"+this.frameid+ "' class='200x160px'> </div> \n";
   var entryInfo = "<div id='"+this.frameid+ "'/>";
   $(topid).append(entryInfo);
}

DABC.GaugeDrawElement.prototype.DrawHistoryElement = function() {
   
   val = this.ExtractField("value", "number");

   if (val > this.max) {
      if (this.gauge!=null) {
         this.gauge = null;
         $("#" + this.frameid).empty();
      }
      this.max = 10;
      var cnt = 0;
      while (val > this.max) 
         this.max *= (((cnt++ % 3) == 1) ? 2.5 : 2);
   }
   
   if (this.gauge==null) {
      this.gauge = new JustGage({
         id: this.frameid, 
         value: val,
         min: this.min,
         max: this.max,
         title: this.FullItemName()
      });
   } else {
      this.gauge.refresh(val);
   }
}

// ======== end of GaugeDrawElement ======================

//======== start of ImageDrawElement ======================

DABC.ImageDrawElement = function() {
   DABC.DrawElement.call(this);
}

// TODO: check how it works in different older browsers
DABC.ImageDrawElement.prototype = Object.create( DABC.DrawElement.prototype );

DABC.ImageDrawElement.prototype.CreateFrames = function(topid, id) {

   this.frameid = "dabc_image_" + id;
   
   var width = $(topid).width();
   
   var url = this.itemname + "image.png";
//   var entryInfo = "<div id='"+this.frameid+ "' class='200x160px'> </div> \n";
   var entryInfo = 
      "<div id='"+this.frameid+ "'>" +
      "<img src='" + url + "' alt='some text' width='" + width + "'>" + 
      "</div>";
   $(topid).append(entryInfo);
}


// ======== end of ImageDrawElement ======================


//======== start of LogDrawElement ======================

DABC.LogDrawElement = function() {
   DABC.HistoryDrawElement.call(this,"log");
}

DABC.LogDrawElement.prototype = Object.create( DABC.HistoryDrawElement.prototype );

DABC.LogDrawElement.prototype.CreateFrames = function(topid, id) {
   this.frameid = "dabc_log_" + id;
   var entryInfo;
   if (this.isHistory()) {
      // var w = $(topid).width();
      var h = $(topid).height();
      var maxhstyle = "";
      if (h>10) maxhstyle = "; max-height:" + h + "px"; 
      entryInfo = "<div id='" + this.frameid + "' style='overflow:auto; font-family:monospace" + maxhstyle + "'/>";
   } else {
      entryInfo = "<div id='"+this.frameid+ "'/>";
   }
   $(topid).append(entryInfo);
}

DABC.LogDrawElement.prototype.DrawHistoryElement = function() {
   var element = $("#" + this.frameid);
   element.empty();

   if (this.isHistory()) {
      var txt = this.ExtractSeries("value","string");
      for (var i=0;i<txt.length;i++)
         element.append("<PRE>"+txt[i]+"</PRE>");
   } else {
      var val = this.ExtractField("value");
      element.append(this.FullItemName() + "<br>");
      element.append("<h5>"+val +"</h5>");
   }
}

// ========= start of GenericDrawElement ===========================


DABC.GenericDrawElement = function() {
   DABC.HistoryDrawElement.call(this,"generic");
   this.recheck = false;   // indicate that we want to redraw 
}

DABC.GenericDrawElement.prototype = Object.create( DABC.HistoryDrawElement.prototype );

DABC.GenericDrawElement.prototype.CreateFrames = function(topid, id) {
   this.frameid = "dabc_generic_" + id;
   var entryInfo;
//   if (this.isHistory()) {
//      var w = $(topid).width();
//      var h = $(topid).height();
//      entryInfo = "<div id='" + this.frameid + "' style='overflow:auto; font-family:monospace; max-height:" + h + "px'/>";
//   } else {
      entryInfo = "<div id='"+this.frameid+ "'/>";
//   }
   $(topid).append(entryInfo);
}

DABC.GenericDrawElement.prototype.DrawHistoryElement = function() {
   if (this.recheck) {
      console.log("here one need to draw element with real style " + this.FullItemName());
      this.recheck = false;
      
      if (this.xmlnode.getAttribute("dabc:kind")) {
         var itemname = this.itemname;
         var xmlnode = this.xmlnode;
         DABC.mgr.ClearWindow();
         DABC.mgr.DisplayItem(itemname, xmlnode);
         return;
      }
   }
   
   var element = $("#" + this.frameid);
   element.empty();
   element.append(this.FullItemName() + "<br>");
   for (var i=0; i< this.xmlnode.attributes.length; i++) {
      var attr = this.xmlnode.attributes.item(i);
      element.append("<h5><PRE>" + attr.name + " = " + attr.value + "</PRE></h5>");
   }

//   if (this.isHistory()) {
//      var txt = this.ExtractSeries("value","string");
//      for (var i=0;i<txt.length;i++)
//         element.append("<PRE>"+txt[i]+"</PRE>");
//   } else {
//      var val = this.ExtractField("value");
//      element.append(this.itemname + "<br>");
//      element.append("<h5>"+val +"</h5>");
//   }
}


//======== start of HierarchyDrawElement =============================

DABC.HierarchyDrawElement = function() {
   DABC.DrawElement.call(this);
   this.xmldoc = 0;
   this.ready = false;
   this.req = 0;             // this is current request
}

// TODO: check how it works in different older browsers
DABC.HierarchyDrawElement.prototype = Object.create( DABC.DrawElement.prototype );

DABC.HierarchyDrawElement.prototype.CreateFrames = function(topid, id) {
   this.frameid = topid;
}

DABC.HierarchyDrawElement.prototype.RegularCheck = function() {
   if (this.ready || this.req) return;
   
   var url = "h.xml";
   
   // this.cnt++;
   // url += "&cnt="+this.cnt;
   // if (this.version>0) url += "&ver=" + this.version;

   // console.log(" Create xml request");
   
   this.req = DABC.mgr.NewHttpRequest(url, true, false, this);

   this.req.send(null);
}

DABC.HierarchyDrawElement.prototype.createNode = function(nodeid, parentid, node, fullname) 
{
   node = DABC.nextXmlNode(node);

   while (node) {

      // console.log(" Work with node " + node.nodeName);
      
      var kind = node.getAttribute("dabc:kind");
      // var value = node.getAttribute("value");
      
      var html = "";

      var nodename = node.nodeName;
      var nodefullname  = "";
      
      if (parentid>=0) 
         nodefullname = fullname + node.nodeName + "/";
      
      var nodeimg = "";
      var node2img = "";
      
      var scan_inside = true;
      
      if (kind) {
         
         if (kind == "ROOT.Session") nodeimg = source_dir+'img/globe.gif'; else
         if (kind == "DABC.Application") nodeimg = 'httpsys/img/dabcicon.png'; else
         if (kind == "DABC.Command") { nodeimg = 'httpsys/img/dabcicon.png'; scan_inside = false; } else
         if (kind == "image.png") nodeimg = 'httpsys/img/dabcicon.png'; else
         if (kind == "GO4.Analysis") nodeimg = 'go4sys/icons/go4logo2_small.png'; else
         if (kind.match(/\bROOT.TH1/)) { nodeimg = source_dir+'img/histo.png'; scan_inside = false; } else
         if (kind.match(/\bROOT.TH2/)) { nodeimg = source_dir+'img/histo2d.png'; scan_inside = false; } else  
         if (kind.match(/\bROOT.TH3/)) { nodeimg = source_dir+'img/histo3d.png'; scan_inside = false; } else
         if (kind == "ROOT.TCanvas") nodeimg = source_dir+'img/canvas.png'; else
         if (kind == "ROOT.TProfile") nodeimg = source_dir+'img/profile.png'; else
         if (kind.match(/\bROOT.TGraph/)) nodeimg = source_dir+'img/graph.png'; else
         if (kind == "ROOT.TTree") nodeimg = source_dir+'img/tree.png'; else
         if (kind == "ROOT.TFolder") { nodeimg = source_dir+'img/folder.gif'; node2img = source_dir+'img/folderopen.gif'; }  else
         if (kind == "ROOT.TNtuple") nodeimg = source_dir+'img/tree_t.png';   else
         if (kind == "ROOT.TBranch") nodeimg = source_dir+'img/branch.png';   else
         if (kind.match(/\bROOT.TLeaf/)) nodeimg = source_dir+'img/leaf.png'; else
         if ((kind == "ROOT.TList") && (node.nodeName == "StreamerInfo")) nodeimg = source_dir+'img/question.gif'; 
      }

      if (!node.hasChildNodes() || !scan_inside) {
         html = "javascript: DABC.mgr.display('"+nodefullname+"');";
      } else {
         html = nodefullname;
         if (html == "") html = ".."; 
      }   
      
      if (node2img == "") node2img = nodeimg;
      
      DABC.dabc_tree.add(nodeid, parentid, nodename, html, nodename, "", nodeimg, node2img);

      if (scan_inside)
         nodeid = this.createNode(nodeid+1, nodeid, node.firstChild, nodefullname);
      else
         nodeid = nodeid + 1;
      
      node = DABC.nextXmlNode(node.nextSibling);
   }
   
   return nodeid;
}

DABC.HierarchyDrawElement.prototype.GetCurrentPath = function() {
   if (!this.xmldoc) return;
   var lvl0 = DABC.nextXmlNode(this.xmldoc.firstChild);
   if (!lvl0) return;
   return lvl0.getAttribute("path");
}

DABC.HierarchyDrawElement.prototype.TopNode = function() 
{
   if (!this.xmldoc) return;
   
   var lvl1 = DABC.nextXmlNode(this.xmldoc.firstChild);
   
   if (lvl1) return DABC.nextXmlNode(lvl1.firstChild);
}


DABC.HierarchyDrawElement.prototype.FindNode = function(fullname, top) {

   if (fullname.length==0) return top;
   
   if (!top) top = this.TopNode();
   var pos = fullname.indexOf("/");
   if (pos<0) return;
   
   var localname = fullname.substr(0, pos);  
   var child = DABC.nextXmlNode(top.firstChild);
   
   // console.log("Serching for localname " + localname);

   while (child) {
      if (child.nodeName == localname) break;
      child = DABC.nextXmlNode(child.nextSibling);
   }
   
   if (!child) return;
   
   return this.FindNode(fullname.substr(pos+1), child);
}


DABC.HierarchyDrawElement.prototype.RequestCallback = function(arg) {
   this.req = 0;

   if (arg==null) { this.ready = false; return; }

   // console.log(" Get XML request callback "+ver);

   this.xmldoc = arg;
   
   // console.log(" xml doc is there");
   
   var top = this.TopNode();
   
   if (!top) { 
      // console.log("XML top node not found");
      return;
   }
   
//   this.createNode(0, -1, top.firstChild, "");
   
   
   DABC.dabc_tree = 0;
   DABC.dabc_tree = new dTree('DABC.dabc_tree');
   DABC.dabc_tree.config.useCookies = false;

   
   this.createNode(0, -1, top, "");

   var content = "<p><a href='javascript: DABC.dabc_tree.openAll();'>open all</a> | <a href='javascript: DABC.dabc_tree.closeAll();'>close all</a> | <a href='javascript: DABC.mgr.ReloadTree();'>reload</a> | <a href='javascript: DABC.mgr.ClearWindow();'>clear</a> </p>";
   content += DABC.dabc_tree;
   $("#" + this.frameid).html(content);
   
//   DABC.dabc_tree.openAll();
   
   this.ready = true;
}


DABC.HierarchyDrawElement.prototype.Clear = function() {
   
   DABC.DrawElement.prototype.Clear.call(this);
   
   this.xmldoc = null;
   this.ready = false;
   if (this.req != null) this.req.abort();
   this.req = null;
}


// ======== end of HierarchyDrawElement ======================

// ================ start of FesaDrawElement

DABC.FesaDrawElement = function(_clname) {
   DABC.DrawElement.call(this);
   this.clname = _clname;     // FESA class name
   this.data = null;          // raw data
   this.root_obj = null;      // root object, required for draw
   this.root_painter = null;  // root object, required for draw
   this.req = null;           // request to get raw data
}

DABC.FesaDrawElement.prototype = Object.create( DABC.DrawElement.prototype );

DABC.FesaDrawElement.prototype.Clear = function() {
   
   DABC.DrawElement.prototype.Clear.call(this);

   this.clname = "";         // ROOT class name
   this.root_obj = null;     // object itself, for streamer info is file instance
   this.root_painter = null;
   this.data = null;          // raw data
   if (this.req) this.req.abort(); 
   this.req = null;          // this is current request
   this.force = true;
}

DABC.FesaDrawElement.prototype.CreateFrames = function(topid, id) {
   this.frameid = "dabcobj" + id;
   
   var entryInfo = "<div id='" + this.frameid + "'/>";
   $(topid).append(entryInfo);
   
   var render_to = "#" + this.frameid;
   var element = $(render_to);
      
   var fillcolor = 'white';
   var factor = 0.66666;
      
   d3.select(render_to).style("background-color", fillcolor);
   d3.select(render_to).style("width", "100%");

   var w = element.width(), h = w * factor; 

   this.vis = d3.select(render_to)
                   .append("svg")
                   .attr("width", w)
                   .attr("height", h)
                   .style("background-color", fillcolor);
      
   this.vis.append("svg:title").text(this.FullItemName());
}

DABC.FesaDrawElement.prototype.ClickItem = function() {
   if (this.req != null) return; 

   if (!this.IsDrawn()) 
      this.CreateFrames(DABC.mgr.NextCell(), DABC.mgr.cnt++);
   this.force = true;
   
   this.RegularCheck();
}

DABC.FesaDrawElement.prototype.RegularCheck = function() {

   if (!DABC.AssertRootPrerequisites()) return;
   
   // no need to do something when req not completed
   if (this.req!=null) return;
 
   // do update when monitoring enabled
   if ((this.version >= 0) && !this.force) {
      var chkbox = document.getElementById("monitoring");
      if (!chkbox || !chkbox.checked) return;
   }
        
   var url = this.itemname + "get.bin";
   
   if (this.version>0) url += "?version=" + this.version;

   this.req = DABC.mgr.NewHttpRequest(url, true, true, this);

   this.req.send(null);

   this.force = false;
}


DABC.FesaDrawElement.prototype.RequestCallback = function(arg) {
   // in any case, request pointer will be reseted
   // delete this.req;
   this.req = null;
   
   // console.log("Get response from server " + arg.length);
   
   var hdr = DABC.UnpackBinaryHeader(arg);
   
   if (hdr == null) {
      alert("cannot extract binary header");
      return;
   }
   
   if (this.version == hdr.version) {
      console.log("Same version of beam profile " + ver);
      return;
   }
   
   if (hdr.rawdata == null) {
      alert("no data for beamprofile when expected");
      return;
   } 

   this.data = hdr.rawdata;
   this.version = hdr.version;

   console.log("Data length is " + this.data.length);
   
   this.vis.select("title").text(this.FullItemName() + 
         "\nversion = " + this.version + ", size = " + this.data.length);

   if (!this.ReconstructObject()) return;
   
   if (this.root_painter != null) {
      this.root_painter.RedrawFrame();
   } else {
      this.root_painter = JSROOTPainter.drawObjectInFrame(this.vis, this.root_obj);
      
      if (this.root_painter == -1) this.root_painter = null;
   }
}


DABC.FesaDrawElement.prototype.ReconstructObject = function()
{
   if (this.clname != "2D") return false;
   
   if (this.root_obj == null) {
      this.root_obj = JSROOTPainter.Create2DHisto(16, 16);
      this.root_obj['fName']  = "BeamProfile";
      this.root_obj['fTitle'] = "Beam profile from FESA";
      this.root_obj['fOption'] = "col";
//      console.log("Create histogram");
   }
   
   if ((this.data==null) || (this.data.length != 16*16*4)) {
      alert("no proper data for beam profile");
      return false;
   }
   
   var o = 0;
   for (var iy=0;iy<16;iy++)
      for (var ix=0;ix<16;ix++) {
         var value = DABC.ntou4(this.data, o); o+=4;
         var bin = this.root_obj.getBin(ix+1, iy+1);
         this.root_obj.setBinContent(bin, value);
//         if (iy==5) console.log("Set content " + value);
      }
   
   return true;
}


//========== start of RateHistoryDrawElement

DABC.RateHistoryDrawElement = function() {
   DABC.HistoryDrawElement.call(this, "rate");
   this.root_painter = null;  // root painter, required for draw
   this.EnableHistory(100);
}

DABC.RateHistoryDrawElement.prototype = Object.create( DABC.HistoryDrawElement.prototype );

DABC.RateHistoryDrawElement.prototype.Clear = function() {
   
   DABC.HistoryDrawElement.prototype.Clear.call(this);
   this.root_painter = null;  // root painter, required for draw
}

DABC.RateHistoryDrawElement.prototype.CreateFrames = function(topid, id) {

   DABC.AssertRootPrerequisites();
   
   this.frameid = "dabcobj" + id;
   
   var entryInfo = "<div id='" + this.frameid + "'/>";
   $(topid).append(entryInfo);
   
   var render_to = "#" + this.frameid;
   var element = $(render_to);
      
   var fillcolor = 'white';
   var factor = 0.66666;
      
   d3.select(render_to).style("background-color", fillcolor);
   d3.select(render_to).style("width", "100%");

   var w = element.width(), h = w * factor; 

   this.vis = d3.select(render_to)
                   .append("svg")
                   .attr("width", w)
                   .attr("height", h)
                   .style("background-color", fillcolor);
      
   this.vis.append("svg:title").text(this.itemname);
}


DABC.RateHistoryDrawElement.prototype.DrawHistoryElement = function() {

   if (!DABC.AssertRootPrerequisites()) return;
   
   this.vis.select("title").text(this.itemname + 
         "\nversion = " + this.version + ", history = " + this.xmlhistory.length);
   
//   console.log("Extract series");
   
   var x = this.ExtractSeries("time", "time");
   var y = this.ExtractSeries("value", "number");
   
//   if (x && y) console.log("Arrays length = " + x.length + "  " + y.length);

   // here we should create TGraph object

   var gr = JSROOTPainter.CreateTGraph();
   
   gr['fX'] = x;
   gr['fY'] = y;
   gr['fNpoints'] = x.length;
   
   JSROOTPainter.AdjustTGraphRanges(gr);

   gr['fHistogram']['fTitle'] = this.FullItemName();
   if (gr['fHistogram']['fYaxis']['fXmin']>0)
      gr['fHistogram']['fYaxis']['fXmin'] = 0;
   else
      gr['fHistogram']['fYaxis']['fXmin'] *= 1.2;

   gr['fHistogram']['fYaxis']['fXmax'] *= 1.2;
   
   gr['fHistogram']['fXaxis']['fTimeDisplay'] = true;
   gr['fHistogram']['fXaxis']['fTimeFormat'] = "";
   // gStyle['TimeOffset'] = 0; // DABC uses UTC time, starting from 1/1/1970
   gr['fHistogram']['fXaxis']['fTimeFormat'] = "%H:%M:%S%F0"; // %FJanuary 1, 1970 00:00:00
   
   if (this.root_painter && this.root_painter.UpdateObject(gr)) {
      this.root_painter.RedrawFrame();
   } else {
      this.root_painter = JSROOTPainter.drawObjectInFrame(this.vis, gr);
      
      if (this.root_painter == -1) this.root_painter = null;
   }
}

// ======== start of RootDrawElement ======================

DABC.RootDrawElement = function(_clname) {
   DABC.DrawElement.call(this);

   this.clname = _clname;    // ROOT class name
   this.obj = null;          // object itself, for streamer info is file instance
   this.sinfo = null;        // used to refer to the streamer info record
   this.req = null;          // this is current request
   this.first_draw = true;   // one should enable flag only when all ROOT scripts are loaded
   this.painter = null;      // pointer on painter, can be used for update
   
   this.raw_data = null;    // raw data kept in the item when object cannot be reconstructed immediately
   this.raw_data_version = 0;   // verison of object in the raw data, will be copied into object when completely reconstructed
   this.raw_data_size = 0;      // size of raw data, can be displayed
   this.need_master_version = 0; // this is version, required for the master item (streamer info)
   
   this.StateEnum = {
         stInit        : 0,
         stWaitRequest : 1,
         stWaitSinfo   : 2,
         stReady       : 3
   };
   
   this.state = this.StateEnum.stInit;   
}

DABC.RootDrawElement.prototype = Object.create( DABC.DrawElement.prototype );

DABC.RootDrawElement.prototype.Clear = function() {
   
   DABC.DrawElement.prototype.Clear.call(this);

   this.clname = "";         // ROOT class name
   this.obj = null;          // object itself, for streamer info is file instance
   this.sinfo = null;        // used to refer to the streamer info record
   if (this.req) this.req.abort(); 
   this.req = null;          // this is current request
   this.first_draw = true;   // one should enable flag only when all ROOT scripts are loaded
   this.painter = null;      // pointer on painter, can be used for update
}



DABC.RootDrawElement.prototype.CreateFrames = function(topid, id) {
   this.frameid = "dabcobj" + id;
   
   this.first_draw = true;
   
   var entryInfo = ""; 
   if (this.sinfo) {
      entryInfo += "<div id='" + this.frameid + "'/>";
   } else {
      entryInfo += "<h5><a> Streamer Infos </a>&nbsp; </h5>";
      entryInfo += "<div style='overflow:auto'><h6>Streamer Infos</h6><span id='" + this.frameid +"' class='dtree'></span></div>";
   }
   //entryInfo+="</div>";
   $(topid).append(entryInfo);
   
   if (this.sinfo) {
      var render_to = "#" + this.frameid;
      var element = $(render_to);
      
      var fillcolor = 'white';
      var factor = 0.66666;
      
      d3.select(render_to).style("background-color", fillcolor);
      d3.select(render_to).style("width", "100%");

      var w = element.width(), h = w * factor; 

      this.vis = d3.select(render_to)
                   .append("svg")
                   .attr("width", w)
                   .attr("height", h)
                   .style("background-color", fillcolor);
      
      this.vis.append("svg:title").text(this.itemname);
      
//      console.log("create visual pane of width " + w + "  height " + h)
   }
   
//   $(topid).data('any', 10);
//   console.log("something = " + $(topid).data('any'));
   
}

DABC.RootDrawElement.prototype.ClickItem = function() {
   if (this.state != this.StateEnum.stReady) return; 

   // TODO: TCanvas update do not work in JSRootIO
   if (this.clname == "TCanvas") return;

   if (!this.IsDrawn()) 
      this.CreateFrames(DABC.mgr.NextCell(), DABC.mgr.cnt++);
      
   this.state = this.StateEnum.stInit;
   this.RegularCheck();
}

// force item to get newest version of the object
DABC.RootDrawElement.prototype.Update = function() {
   if (this.state != this.StateEnum.stReady) return;
   this.state = this.StateEnum.stInit;
   this.RegularCheck();
}

DABC.RootDrawElement.prototype.HasVersion = function(ver) {
   return this.obj && (this.version >= ver);
}

// if frame created and exists
DABC.RootDrawElement.prototype.DrawObject = function() {
   if (this.obj == null) return;

   if (this.sinfo) {
   
      if (this.vis!=null)
        this.vis.select("title").text(this.FullItemName() + 
                                      "\nversion = " + this.version + ", size = " + this.raw_data_size);
      
      if (this.painter != null) {
         this.painter.RedrawFrame();
      } else {
//         if (gStyle) gStyle.AutoStat = true;
//                else console.log("no gStyle");

         this.painter = JSROOTPainter.drawObjectInFrame(this.vis, this.obj);
         
         if (this.painter == -1) this.painter = null;

         // if (this.painter)  console.log("painter is created");
      }
   } else {
      gFile = this.obj;
      JSROOTPainter.displayStreamerInfos(this.obj.fStreamerInfo.fStreamerInfos, "#" + this.frameid);
      gFile = 0;
   }
   
   this.first_draw = false;
}

DABC.RootDrawElement.prototype.ReconstructRootObject = function() {
   if (!this.raw_data) {
      this.state = this.StateEnum.stInit;
      return;
   }

   var obj = {};
   
   obj['_typename'] = 'JSROOTIO.' + this.clname;

//   console.log("Calling JSROOTIO function");

   gFile = this.sinfo.obj;

   if (this.clname == "TCanvas") {
      obj = JSROOTIO.ReadTCanvas(this.raw_data, 0);
      if (obj && obj['fPrimitives']) {
         if (obj['fName'] == "") obj['fName'] = "anyname";
      }
   } else 
   if (JSROOTIO.GetStreamer(this.clname)) {
      JSROOTIO.GetStreamer(this.clname).Stream(obj, this.raw_data, 0);
      JSROOTCore.addMethods(obj);

   } else {
      console.log("!!!! streamer not found !!!!!!!" + this.clname);
   }
   
   gFile = null;

   if (this.painter && this.painter.UpdateObject(obj)) {
      // if painter accepted object update, we need later just redraw frame
      obj = null;
   } else { 
      this.obj = obj;
      this.painter = null;
   }
   
   this.state = this.StateEnum.stReady;
   this.version = this.raw_data_version;
   
   this.raw_data = null;
   this.raw_data_version = 0;
   
   this.DrawObject();
}

DABC.RootDrawElement.prototype.RequestCallback = function(arg) {
   // in any case, request pointer will be reseted
   // delete this.req;
   this.req = null;
   
   if (this.state != this.StateEnum.stWaitRequest) {
      alert("item not in wait request state");
      this.state = this.StateEnum.stInit;
      return;
   }
   
   var hdr = DABC.UnpackBinaryHeader(arg);
   
   if (hdr==null) {
      console.log(" RootDrawElement get error " + this.itemname + "  reload list");
      this.state = this.StateEnum.stInit;
      // most probably, objects structure is changed, therefore reload it
      DABC.mgr.ReloadTree();
      return;
   }

   // if we got same version, do nothing - we are happy!!!
   if ((hdr.version > 0) && (this.version == hdr.version)) {
      this.state = this.StateEnum.stReady;
      console.log(" Get same version " + hdr.version + " of object " + this.itemname);
      if (this.first_draw) this.DrawObject();
      return;
   } 
   
   // console.log(" RootDrawElement get callback " + this.itemname + " sz " + arg.length + "  this.version = " + this.version + "  newversion = " + ver);

   if (!this.sinfo) {
      
      delete this.obj; 
      
      // we are doing sreamer infos
      if (gFile) console.log(" gFile already exists???"); 
      this.obj = new JSROOTIO.RootFile;

      gFile = this.obj;
      this.obj.fStreamerInfo.ExtractStreamerInfo(hdr.rawdata);
      this.version = hdr.version;
      this.state = this.StateEnum.stReady;
      this.raw_data = null;
      // this indicates that object was clicked and want to be drawn
      this.DrawObject();
         
      gFile = null;

      if (!this.obj) alert("Cannot reconstruct streamer infos!!!");

      // with streamer info one could try to update complex fields
      DABC.mgr.UpdateComplexFields();

      return;
   } 

   this.raw_data_version = hdr.version;
   this.raw_data = hdr.rawdata;
   this.raw_data_size = hdr.rawdata.length;
   this.need_master_version = hdr.master_version;
   
   if (this.sinfo && !this.sinfo.HasVersion(this.need_master_version)) {
      // console.log(" Streamer info is required of vers " + this.need_master_version);
      this.state = this.StateEnum.stWaitSinfo;
      this.sinfo.Update();
      return;
   }
   
   this.ReconstructRootObject();
}

DABC.RootDrawElement.prototype.RegularCheck = function() {

   if (!DABC.AssertRootPrerequisites()) return;
   
   switch (this.state) {
     case this.StateEnum.stInit: break;
     case this.StateEnum.stWaitRequest: return;
     case this.StateEnum.stWaitSinfo: { 
        // console.log(" item " + this.itemname+ " requires streamer info ver " + this.need_master_version  +  "  available is = " + this.sinfo.version);

        if (this.sinfo.HasVersion(this.need_master_version)) {
           this.ReconstructRootObject();
        } else {
           // console.log(" version is not ok");
        }
        return;
     }
     case this.StateEnum.stReady: {
        // if item ready, verify that we want to send request again
        if (!this.IsDrawn()) return;
        var chkbox = document.getElementById("monitoring");
        if (!chkbox || !chkbox.checked) return;
        
        // TODO: TCanvas update do not work in JSRootIO
        if (this.clname == "TCanvas") return;
        
        break;
     }
     default: return;
   }
   
   // console.log(" checking request for " + this.itemname + (this.sinfo.ready ? " true" : " false"));

   var url = this.itemname + "get.bin";
   
   if (this.version>0) url += "?version=" + this.version;

   this.req = DABC.mgr.NewHttpRequest(url, true, true, this);

//   console.log(" Send request " + url);

   this.req.send(null);
   
   this.state = this.StateEnum.stWaitRequest;
}


// ======== end of RootDrawElement ======================



// ============= start of DABC.Manager =============== 

DABC.Manager = function(with_tree) {
   this.cnt = new Number(0);      // counter to create new element 
   this.arr = new Array();        // array of DrawElement
   this.with_tree = with_tree;
   
   if (this.with_tree) {
      DABC.dabc_tree = new dTree('DABC.dabc_tree');
      DABC.dabc_tree.config.useCookies = false;
      this.CreateTable(3,3);
   }
   
   return this;
}

DABC.Manager.prototype.CreateTable = function(divx, divy)
{
   var tablecontents = "";
   var cnt = 0;
   
   var precx = Math.floor(100/divx);
   var precy = Math.floor(100/divy);
   
   tablecontents = "<table width='100%' height='100%'>";
   for (var i = 0; i < divy; i ++)
   {
      tablecontents += "<tr height='"+precy+"%'>";
      for (var j = 0; j < divx; j ++) {
         tablecontents += "<td id='draw_place_"+ cnt + "' width='" + precx 
                       + "%' height='"+precy+"%'>" + i + "," + j + "</td>";
         cnt++;
      }
      tablecontents += "</tr>";
   }
   tablecontents += "</table>";
   $("#dabc_draw").empty();
   document.getElementById("dabc_draw").innerHTML = tablecontents;


   this.table_counter = 0;
   this.table_number = divx*divy;
}

DABC.Manager.prototype.NextCell = function()
{
   var id = "#dabc_draw";
   if (this.with_tree) {
      var id = "#draw_place_" + this.table_counter;
      this.table_counter++;
      if (this.table_counter>=this.table_number) this.table_counter = 0;
   }
   $(id).empty();
   return id;
}


DABC.Manager.prototype.FindItem = function(_item) {
   for (var i in this.arr) {
      if (this.arr[i].itemname == _item) return this.arr[i];
   }
}

DABC.Manager.prototype.empty = function() {
   return this.arr.length == 0;
}

// this is used for text request 
DABC.Manager.prototype.NewRequest = function() {
   var req;
   // For Safari, Firefox, and other non-MS browsers
   if (window.XMLHttpRequest) {
      try {
         req = new XMLHttpRequest();
      } catch (e) {
         req = false;
      }
   } else if (window.ActiveXObject) {
      // For Internet Explorer on Windows
      try {
         req = new ActiveXObject("Msxml2.XMLHTTP");
      } catch (e) {
         try {
            req = new ActiveXObject("Microsoft.XMLHTTP");
         } catch (e) {
            req = false;
         }
      }
   }

   return req;
}


DABC.Manager.prototype.NewHttpRequest = function(url, async, isbin, item) {
   var xhr = new XMLHttpRequest();
   
   xhr.dabc_item = item;
   xhr.isbin = isbin;

//   if (typeof ActiveXObject == "function") {
   if (window.ActiveXObject) {
      // console.log(" Create IE request");
      
      xhr.onreadystatechange = function() {
         // console.log(" Ready IE request");
         if (this.readyState != 4) return;
         if (!this.dabc_item || (this.dabc_item==0)) return;
         
         if (this.status == 200 || this.status == 206) {
            if (this.isbin) {
               var filecontent = new String("");
               var array = new VBArray(this.responseBody).toArray();
               for (var i = 0; i < array.length; i++) {
                  filecontent = filecontent + String.fromCharCode(array[i]);
               }
               
               // console.log(" IE response ver = " + ver);

               this.dabc_item.RequestCallback(filecontent);
               delete filecontent;
               filecontent = null;
            } else {
               this.dabc_item.RequestCallback(this.responseXML);
            }
         } else {
            this.dabc_item.RequestCallback(null);
         } 
         this.dabc_item = null;
      }
      
      xhr.open('POST', url, async);
      
   } else {
      
      xhr.onreadystatechange = function() {
         //console.log(" Response request "+this.readyState);

         if (this.readyState != 4) return;
         if (!this.dabc_item || (this.dabc_item==0)) return;
         
         if (this.status == 0 || this.status == 200 || this.status == 206) {
            if (this.isbin) {
               var HasArrayBuffer = ('ArrayBuffer' in window && 'Uint8Array' in window);
               var Buf;
               if (HasArrayBuffer && 'mozResponse' in this) {
                  Buf = this.mozResponse;
               } else if (HasArrayBuffer && this.mozResponseArrayBuffer) {
                  Buf = this.mozResponseArrayBuffer;
               } else if ('responseType' in this) {
                  Buf = this.response;
               } else {
                  Buf = this.responseText;
                  HasArrayBuffer = false;
               }
               if (HasArrayBuffer) {
                  var filecontent = new String("");
                  var bLen = Buf.byteLength;
                  var u8Arr = new Uint8Array(Buf, 0, bLen);
                  for (var i = 0; i < u8Arr.length; i++) {
                     filecontent = filecontent + String.fromCharCode(u8Arr[i]);
                  }
                  delete u8Arr;
               } else {
                  var filecontent = Buf;
               }
               this.dabc_item.RequestCallback(filecontent);

               delete filecontent;
               filecontent = null;
            } else {
               this.dabc_item.RequestCallback(this.responseXML);
            }
         } else {
            this.dabc_item.RequestCallback(null);
         }
         this.dabc_item = 0;
      }

      xhr.open('POST', url, async);
      
      if (xhr.isbin) {
         var HasArrayBuffer = ('ArrayBuffer' in window && 'Uint8Array' in window);
         if (HasArrayBuffer && 'mozResponseType' in xhr) {
            xhr.mozResponseType = 'arraybuffer';
         } else if (HasArrayBuffer && 'responseType' in xhr) {
            xhr.responseType = 'arraybuffer';
         } else {
            //XHR binary charset opt by Marcus Granado 2006 [http://mgran.blogspot.com]
            xhr.overrideMimeType("text/plain; charset=x-user-defined");
         }
      }
   }
   return xhr;
}


DABC.Manager.prototype.UpdateComplexFields = function() {
   if (this.empty()) return;

   for (var i in this.arr) 
     this.arr[i].RegularCheck();
}

DABC.Manager.prototype.UpdateAll = function() {
   this.UpdateComplexFields();
}

DABC.Manager.prototype.DisplayItem = function(itemname, xmlnode)
{
   if (!xmlnode) xmlnode = this.FindXmlNode(itemname);
   if (!xmlnode) {
      console.log("cannot find xml node " + itemname);
      return;
   } 
   
   var kind = xmlnode.getAttribute("dabc:kind");
   var history = xmlnode.getAttribute("dabc:history");
   if (!kind) kind = "";

   var elem;
   
   if (kind == "image.png") {
      elem = new DABC.ImageDrawElement();
      elem.itemname = itemname;
      elem.CreateFrames(this.NextCell(), this.cnt++);
      this.arr.push(elem);
      return;
   }
   
   if (kind == "DABC.Command") {
      elem = new DABC.CommandDrawElement();
      elem.itemname = itemname;
      elem.CreateFrames(this.NextCell(), this.cnt++);
      elem.RequestCommand();
      this.arr.push(elem);
      return;
   }

   // ratemeter
   if (kind == "rate") { 
      if ((history == null) || !document.getElementById("show_history").checked) {
         elem = new DABC.GaugeDrawElement();
         elem.itemname = itemname;
         elem.CreateFrames(this.NextCell(), this.cnt++);
         this.arr.push(elem);
         return;
      } else {
         elem = new DABC.RateHistoryDrawElement();
         elem.itemname = itemname;
         elem.CreateFrames(this.NextCell(), this.cnt++);
         this.arr.push(elem);
         return;
      }
   }
   
   if (kind == "log") {
      elem = new DABC.LogDrawElement();
      elem.itemname = itemname;

      if ((history != null) && document.getElementById("show_history").checked) 
         elem.EnableHistory(100);
      
      elem.CreateFrames(this.NextCell(), this.cnt++);
      this.arr.push(elem);
      return;
   }
   
   if (kind.indexOf("FESA.") == 0) {
      elem = new DABC.FesaDrawElement(kind.substr(5));
      elem.itemname = itemname;
      elem.CreateFrames(this.NextCell(), this.cnt++);
      this.arr.push(elem);
      elem.RegularCheck();
      return;
   }

   if (kind.indexOf("ROOT.") == 0) {
      // procesing of ROOT classes 
      var sinfoname = this.FindMasterName(itemname, xmlnode);
      var sinfo = this.FindItem(sinfoname);

      if (sinfoname && !sinfo) {
         sinfo = new DABC.RootDrawElement(kind.substr(5));
         sinfo.itemname = sinfoname;
         this.arr.push(sinfo);
         // mark sinfo item as ready - it will not be requested until is not really required
         sinfo.state = sinfo.StateEnum.stReady;
      }

      elem = new DABC.RootDrawElement(kind.substr(5));
      elem.itemname = itemname;
      elem.sinfo = sinfo;
      elem.CreateFrames(this.NextCell(), this.cnt++);
      this.arr.push(elem);

      elem.RegularCheck();
      return;
   }
   
   // create generic draw element, which just shows all attributes 
   elem = new DABC.GenericDrawElement();
   elem.itemname = itemname;
   elem.CreateFrames(this.NextCell(), this.cnt++);
   this.arr.push(elem);
   return elem;
}

DABC.Manager.prototype.display = function(itemname) {
//   console.log(" display click "+itemname);

//   if (!itemname) return;

//   console.log(" display click "+itemname);

/*   
   console.log(" display click "+itemname);
   if (JSROOT.elements) console.log(" elements are there ");
   var topid = JSROOT.elements.generateNewFrame("report");
   var elem = new JSROOT.DrawElement();
   JSROOT.elements.initElement(topid, elem, true);
   elem.makeCollapsible();
   elem.setInfo("Any Element");
   elem.appendText("<br>somethiung to see");
   elem.appendText("<br>somethiung to see");
   console.log(" source "+JSROOT.source_dir );
*/  
   
   var xmlnode = this.FindXmlNode(itemname);
   if (!xmlnode) {
      console.log(" cannot find xml node " + itemname);
      return;
   }

//   console.log(" find kind of node "+itemname + " " + kind);
   
   var elem = this.FindItem(itemname);

   if (elem) {
      elem.ClickItem();
      return;
   }
   
   this.DisplayItem(itemname, xmlnode);
}

DABC.Manager.prototype.ExecuteCommand = function(itemname)
{
   var elem = this.FindItem(itemname);
   if (elem) elem.InvokeCommand();
}



DABC.Manager.prototype.DisplayGeneric = function(itemname, recheck)
{
   var elem = new DABC.GenericDrawElement();
   elem.itemname = itemname;
   elem.CreateFrames(this.NextCell(), this.cnt++);
   if (recheck) elem.recheck = true;
   this.arr.push(elem);
}


DABC.Manager.prototype.DisplayHiearchy = function(holder) {
   var elem = this.FindItem("ObjectsTree");
   
   if (elem) return;

   // console.log(" start2");

   elem = new DABC.HierarchyDrawElement();
   
   elem.itemname = "ObjectsTree";

   elem.CreateFrames(holder, this.cnt++);
   
   this.arr.push(elem);
   
   elem.RegularCheck();
}

DABC.Manager.prototype.ReloadTree = function() 
{
   var elem = this.FindItem("ObjectsTree");
   if (!elem) return;
   
   elem.ready = false;
   elem.RegularCheck();
}

DABC.Manager.prototype.ClearWindow = function()
{
   $("#dialog").empty();
   
   var elem = null;
   
   if (this.with_tree) {
      elem = this.FindItem("ObjectsTree");
   }

   for (var i=0;i<this.arr.length;i++) {
      if (this.arr[i] == elem) continue;
      this.arr[i].Clear();
   }
   
   this.arr.splice(0, this.arr.length);

   if (!this.with_tree) return;
   
   var num = $("#grid_spinner").spinner( "value" );

   this.arr.push(elem);
   
   this.CreateTable(num,num);
   
   elem.ready = false;
   elem.RegularCheck();
}


DABC.Manager.prototype.FindXmlNode = function(itemname) {
   var elem = this.FindItem("ObjectsTree");
   if (!elem) return;
   
   return elem.FindNode(itemname);
}

DABC.Manager.prototype.GetCurrentPath = function() {
   var elem = this.FindItem("ObjectsTree");
   if (elem) return elem.GetCurrentPath();
   return "";
}


DABC.Manager.prototype.ReloadSingleElement = function()
{
   if (this.arr.length == 0) return;

   var itemname = this.arr[this.arr.length-1].itemname;
   
   this.ClearWindow();
   
   this.DisplayGeneric(itemname, true);
}


/** \brief Method finds element in structure, which must be loaded before item itself can be loaded
 *   In case of ROOT objects it is StreamerInfo */

DABC.Manager.prototype.FindMasterName = function(itemname, itemnode) {
   if (!itemnode) return;
   
   var master = itemnode.getAttribute("dabc:master");
   if (!master) return;
   
   var newname = itemname;
   var currpath = this.GetCurrentPath();
   
   if ((newname.length==0) && (currpath.length==0)) {
      newname = document.location.pathname;
      console.log("find master for path " + newname);
   }
   
   // console.log("item = " + itemname + "  master = " + master);

   while (newname) {
      var separ = newname.lastIndexOf("/", newname.length - 2);
      
      if (separ<=0) {
         
         if (currpath.length>0) {
            // if itemname too short, try to apply global path 
            newname = currpath + newname;
            currpath = "";
            continue;
         }
         
         alert("Name " + itemname + " is not long enough for master " + itemnode.getAttribute("dabc:master"));
         return;
      }
      newname = newname.substr(0, separ+1);
      
      if (master.indexOf("../")<0) break;
      master = master.substr(3);
   }
   
   return newname + master + "/";
}



// ============= end of DABC.Manager =============== 
