(function(){

   if (typeof DABC == "object") {
      var e1 = new Error("DABC is already defined");
      e1.source = "dabc.js";
      throw e1;
   }

   if (typeof JSROOT != "object") {
      var e1 = new Error("dabc.js requires JSROOT to be already loaded");
      e1.source = "dabc.js";
      throw e1;
   }
   
   JSROOT.gStyle.DragAndDrop = true;

   DABC = {};

   DABC.version = "2.9.1";
   
   DABC.source_dir = function(){
      var scripts = document.getElementsByTagName('script');

      for (var n in scripts) {
         if (scripts[n]['type'] != 'text/javascript') continue;

         var src = scripts[n]['src'];
         if ((src == null) || (src.length == 0)) continue;

         var pos = src.indexOf("dabc.js");
         if (pos<0) continue;
         if (src.indexOf("JSRootCore.")>0) continue;
         
         JSROOT.console("Set DABC.source_dir to " + src.substr(0, pos));
         return src.substr(0, pos);
      }
      return "";
   }(); 

   
   DABC.InvokeCommand = function(itemname, args) {
      var url = itemname + "/execute";
      if (args!=null) url += "?" + args; 
      
      JSROOT.NewHttpRequest(url,"object").send();
   }
   
   
   // method for custom HADAQ-specific GUI, later could be moved into hadaq.js script 
   
   DABC.HadaqDAQControl = function(hpainter, itemname) {
      var mdi = hpainter.GetDisplay();
      if (!mdi) return null;

      var frame = mdi.FindFrame(itemname, true);
      if (!frame) return null;
      
      var divid = d3.select(frame).attr('id');
      
      var item = hpainter.Find(itemname);
      var calarr = [];
      if (item) {
         for (var n in item._parent._childs) {
            var name = item._parent._childs[n]._name;
            
            if ((name.indexOf("TRB")==0) && (name.indexOf("TdcCal")>0)) {
               var fullname = hpainter.itemFullName(item._parent._childs[n]);
               calarr.push(fullname);               
            }
         }
      }
      
      var html = "<fieldset>" +
      		     "<legend>DAQ</legend>" +
      		     "<button class='hadaq_startfile'>Start file</button>" +
                 "<button class='hadaq_stopfile'>Stop file</button>" +
                 "<button class='hadaq_restartfile'>Restart file</button>" +
                 '<input class="hadaq_filename" type="text" name="filename" value="file.hld" style="margin-top:5px;"/><br/>' +
                 "<label class='hadaq_rate'>Rate: __undefind__</label><br/>"+
                 "<label class='hadaq_info'>Info: __undefind__</label>"+
      		     "</fieldset>" +
      		     "<fieldset>" +
      		     "<legend>Calibration</legend>";
      for (var n in calarr)
         html += "<div class='hadaq_calibr' style='padding:2px;margin:2px'>" + calarr[n] + "</div>";
      html+="</fieldset>";
      
      d3.select(frame).html(html);
      
      $(frame).find(".hadaq_startfile").button().click(function() { 
         DABC.InvokeCommand(itemname+"/StartHldFile", "filename="+$(frame).find('.hadaq_filename').val()+"&maxsize=2000");
      });
      $(frame).find(".hadaq_stopfile").button().click(function() { 
         DABC.InvokeCommand(itemname+"/StopHldFile");
      });
      $(frame).find(".hadaq_restartfile").button().click(function() { 
         DABC.InvokeCommand(itemname+"/RestartHldFile");
      });
      
      var inforeq = null;
      
      function UpdateDaqStatus(res) {
         if (res==null) return;
         var rate = "";
         for (var n in res._childs) {
            var item = res._childs[n], lbl = "", units = "";
            
            if (item._name=='HadaqInfo')
               $(frame).find('.hadaq_info').text("Info: " + item.value);                  
            if (item._name=='HadaqData') { lbl = "Rate: "; units = " " + item.units+"/s"; }
            if (item._name=='HadaqEvents') { lbl = "Ev: "; units = " Hz"; }
            if (item._name=='HadaqLostEvents') { lbl = "Lost:"; units = " Hz"; }
            if (lbl) {
               if (rate.length>0) rate += " ";
               rate += lbl + item.value + units;
            }
         }
         
         $(frame).find('.hadaq_rate').css("font-size","120%").text(rate);                  
      }
      
      var handler = setInterval(function() {
         if ($("#"+divid+" .hadaq_info").length==0) {
            // if main element disapper (reset), stop handler 
            clearInterval(handler);
            return;
         }
         
         if (inforeq) return;
         
         var url = "multiget.json?items=[";
         url+="'" + itemname + "'";
         for (var n in calarr)
            url+= ",'" + calarr[n]+"/Status" + "'";
         url+="]";
         //url+="]&compact=3";
         
         function makehname(prefix, code, name) {
            var str = code.toString(16).toUpperCase();
            while (str.length<4) str = "0"+str;
            return "/"+prefix+"_"+str+"/"+prefix+"_"+str+"_"+name;
         }

         function get_status_color(status) {
            if (status.indexOf('Ready')==0) return 'green';
            if (status.indexOf('File')==0) return 'blue';
            return 'red';
         }
         
         inforeq = JSROOT.NewHttpRequest(url, "object", function(res) {
            inforeq = null;
            if (!res) return;
            UpdateDaqStatus(res[0].result);
            res.shift();
            DABC.UpdateTRBStatus($(frame).find('.hadaq_calibr'), res, hpainter, true);
         });
         inforeq.send(null);
      }, 2000);
   }
   
   DABC.UpdateTRBStatus = function(holder, res, hpainter, multiget) {
      
      function makehname(prefix, code, name) {
         var str = code.toString(16).toUpperCase();
         while (str.length<4) str = "0"+str;
         str = prefix+"_"+str + "_" + name;
         var hitem = null;
         hpainter.ForEach(function(item) { if ((item._name == str) && (hitem==null)) hitem = item; });
         if (hitem) return hpainter.itemFullName(hitem);
         console.log("did not found histogram " + str);
         return str;
      }

      function get_status_color(status) {
         if (status.indexOf('Ready')==0) return 'green';
         if (status.indexOf('File')==0) return 'blue';
         return 'red';
      }
      
      holder.each(function(index) {
         if (!res) return;
         var info = multiget ? res[index] : res;
         // when doing multiget, return object stored as result field
         if (('result' in info) && multiget) info = info.result;
         if (!info) return;
         
         if ($(this).children().length == 0) {
            var code = "<div style='float:left'>";
            code += "<button hist='" + makehname("TRB", info.trb, "MsgPerTDC") + "' >"+info.trb.toString(16)+"</button>";
            for (var j in info.tdc)
               code+="<button class='tdc_btn' tdc='" + info.tdc[j] + "' hist='" + makehname("TDC", info.tdc[j], "Channels") + "'>"+info.tdc[j].toString(16)+"</button>";
         
            code += "</div>"; 
            code += "<div class='hadaq_progress'></div>";
            $(this).html(code);
            $(this).find("button").button().click(function(){ 
               var drawframe = hpainter.GetDisplay().FindFrame("tdccalibr_drawing", true);
               $(drawframe).empty();
               hpainter.display($(this).attr('hist'),"divid:"+$(drawframe).attr('id'));         
            });
            $(this).find(".hadaq_progress").progressbar({ value: info.progress });
         }
         
         $(this).css('background-color', get_status_color(info.value));
         $(this).attr('title',"TRB:" + info.trb.toString(16) + " State: " + info.value + " Time:" + info.time);
         
         $(this).find(".hadaq_progress")
            .attr("title", "progress: " + info.progress + "%")
            .progressbar("option", "value", info.progress);
         
         for (var j in info.tdc) {
            $(this).find(".tdc_btn[tdc='"+info.tdc[j]+"']")
                   .css('color', get_status_color(info.tdc_status[j]))
                   .css('background', 'white')
                   .attr("title", "TDC" + info.tdc[j].toString(16) + " " + info.tdc_status[j] + " Progress:" + info.tdc_progr[j]);
         }
      });
   }
   
   
   DABC.StreamControl = function(hpainter, itemname) {
      var mdi = hpainter.GetDisplay();
      if (!mdi) return null;

      var frame = mdi.FindFrame(itemname, true);
      if (!frame) return null;
      
      var divid = d3.select(frame).attr('id');
      
      var item = hpainter.Find(itemname + "/Status");
      var calarr = [];
      if (item) {
         for (var n in item._childs) 
            calarr.push(hpainter.itemFullName(item._childs[n]));
      }
      
      var html = "<fieldset>" +
                 "<legend>Stream</legend>" +
                 "<button class='store_startfile' title='Start storage of ROOT file'>Start file</button>" +
                 "<button class='store_stopfile' title='Stop storage of ROOT file'>Stop file</button>" +
                 '<input class="store_filename" type="text" name="filename" value="file.root" style="margin-top:5px;"/><br/>' +
                 "<label class='stream_rate'>Rate: __undefind__</label><br/>"+
                 "<label class='stream_info'>Info: __undefind__</label>"+
                 "</fieldset>" +
                 "<fieldset>" +
                 "<legend>Calibration</legend>";
      for (var n in calarr) 
         html += "<div class='stream_tdc_calibr' style='padding:2px;margin:2px'>" + calarr[n] + "</div>";
      html+="</fieldset>";
      
      d3.select(frame).html(html);
      
      $(frame).find(".store_filename").prop("title", "Name of ROOT file to store.\nOne can specify store kind and maxsize with args like:\n file.root&kind=2&maxsize=2000");
      
      $(frame).find(".store_startfile")
         .button()
         .click(function() { 
            DABC.InvokeCommand(itemname+"/Control/StartRootFile", "fname="+$(frame).find('.store_filename').val());
         });
      $(frame).find(".store_stopfile")
         .button()
         .click(function() { DABC.InvokeCommand(itemname+"/Control/StopRootFile"); });
      
      var inforeq = null;
      
      function UpdateStreamStatus(res) {
         if (res==null) return;
         $(frame).find('.stream_rate').css("font-size","120%").text("Rate: " + res.EventsRate + " ev/s");                  
         $(frame).find('.stream_info').text("Info: " + res.StoreInfo);                  
      }
      
      var handler = setInterval(function() {
         if ($("#"+divid+" .stream_info").length==0) {
            // if main element disapper (reset), stop handler 
            clearInterval(handler);
            return;
         }
         
         if (inforeq) return;
         
         inforeq = JSROOT.NewHttpRequest(itemname + "/Status/get.json", "object", function(res) {
            inforeq = null;
            if (res==null) return;
            UpdateStreamStatus(res);
            DABC.UpdateTRBStatus($(frame).find('.stream_tdc_calibr'), res._childs, hpainter, false);
         });
         inforeq.send(null);
      }, 2000);
   }
   
   
   // =========================================== BNET ============================================
   
   DABC.CompareArrays = function(arr1, arr2) {
      if (!arr1 || !arr2) return arr1 == arr2;
      if (arr1.length != arr2.length) return false;
      for (var k=0;k<arr1.length;++k)
         if (arr1[k] != arr2[k]) return false;
      return true;
   }
   
   DABC.BnetPainter = function(hpainter, mdi, itemname) {
      JSROOT.TBasePainter.call(this);
      
      this.hpainter = hpainter;
      this.mdi = mdi;
      this.itemname = itemname;
      
      this.frame = this.mdi.FindFrame(itemname, true);
      if (!this.frame) return;
      
      this.InputItems = [];
      this.BuilderItems = [];
      this.CalibrHub = 0; 
      this.CalibrItem = "";
      this.InputNodes = [];
      this.BuilderNodes = [];
      this.InputInfo = [];
      this.BuilderInfo = [];
      this.CalibrInfo = null;
      this.inforeq = null;
   } 
   
   DABC.BnetPainter.prototype = Object.create(JSROOT.TBasePainter.prototype);
   
   DABC.BnetPainter.prototype.Cleanup = function(arg) {
      console.log('CLEANUP DABC PAINTER');
      
      JSROOT.TBasePainter.prototype.Cleanup.call(this, arg);
      
      if (this.main_timer) {
         clearInterval(this.main_timer);
         delete this.main_timer;
      }
      
      delete this.hpainter;
      delete this.mdi;
      delete this.frame;
   }
   
   DABC.BnetPainter.prototype.active = function() {
      return this.mdi && this.frame;
   }
   
   DABC.BnetPainter.prototype.wrap = function(txt, sz) {
      while (txt.length<sz) txt += " ";
      if (txt.length > sz) txt = txt.substr(0, sz);
      return txt;
   }
   
   DABC.BnetPainter.prototype.MakeLabel = function(attr, txt, sz) {
      var lbl = "<label";
      if (attr) lbl += " " + attr;
      lbl +=">";
      if (txt === undefined) txt = "<undef>"; else
      if (txt === null) txt = "null"; else
      if (typeof txt != 'string') txt = txt.toString();
      lbl += this.wrap(txt,sz);
      lbl += "</label>";
      return lbl;
   }
   
   DABC.BnetPainter.prototype.RefreshHTML = function() {
      
      console.log('DOING RefreshHTML');
      
      var html = "<div style='overflow:hidden;max-height:100%;max-width:100%'>";
      
      html += "<fieldset style='margin:5px'>" +
              "<legend>Input nodes</legend>" +
              "<div style='display:flex;flex-direction:column;font-family:monospace'>";
      html += "<div style='float:left' class='bnet_inputs_header'>"
      html += "<pre style='margin:0'>";
      html += this.MakeLabel("", "Node", 15) + "| " + this.MakeLabel("", "Data", 10) + "| " + this.MakeLabel("", "Events", 10) + "| " + 
              this.MakeLabel("class='bnet_trb_clear'", "HUBs", 40);    
      html += "</pre>";
      html += "</div>";
      for (var node in this.InputItems) {
         html += "<div style='float:left' class='bnet_input" + node + "'>";
         html += "<pre style='margin:0'>";
         html += "<label>" + this.InputItems[node] + "</label>";
         html += "</pre>";
         html += "</div>";
      }

      html += "</div>" +
              "</fieldset>";
      
      html += "<fieldset class='bnet_trb_info' style='margin:5px;display:none'>" +
              "<legend>Calibration</legend>" +
              "<div class='bnet_hub_info'></div>" +
              "<div class='bnet_tdc_calibr'></div>" +
              "</fieldset>";
      
      html += "<fieldset style='margin:5px'>" +
              "<legend>Builder nodes</legend>" +
              "<div style='display:flex;flex-direction:column;font-family:monospace'>";
      html += "<div style='float:left' class='bnet_builders_header'>"
      html += "<pre style='margin:0'>";
      html += this.MakeLabel("", "Node", 15) + "| " + this.MakeLabel("", "Data", 10) + "| " + this.MakeLabel("", "Events", 10) + 
              "| " + this.MakeLabel("", "File", 23) +  "| " + this.MakeLabel("", "Size, MB", 10) /* +  "| " + this.MakeLabel("", "Info", 30) */;    
      html += "</pre>";
      html += "</div>";
      for (var node in this.BuilderItems) {
         html += "<div style='float:left' class='bnet_builder" + node + "'>";
         html += "<pre style='margin:0'>";
         html += "<label>" + this.BuilderItems[node] + "</label>";
         html += "</pre>";
         html += "</div>";
      }

      html += "</div>" +
              "</fieldset>";

      html += "<fieldset style='margin:5px'>" +
              "<legend class='bnet_state'>Run control</legend>" +
              "<button class='bnet_startrun'>Start run</button>" +
              "<select class='bnet_selectrun'>" + 
              "<option>data</option>" +
              "<option>test</option>" +
              "<option>cosmic</option>" +
              "</select>" +
              "<button class='bnet_stoprun'>Stop run</button>" +
              "</fieldset>";
      
      html += "</div>";
      
      var main = d3.select(this.frame).html(html);
      main.classed("jsroot_fixed_frame", true);
      main.selectAll(".bnet_trb_clear").on("click", this.DisplayCalItem.bind(this,0,""));
      
      var itemname = this.itemname;
      
      $(main.node()).find(".bnet_selectrun").selectmenu({ width: 150 } );
      
      $(main.node()).find(".bnet_startrun").button().click(function() {
         DABC.InvokeCommand(itemname+"/StartRun", "prefix=" + $(main.node()).find(".bnet_selectrun").selectmenu().val());
      });
      $(main.node()).find(".bnet_stoprun").button().click(function() { 
         DABC.InvokeCommand(itemname+"/StopRun");
      });


      // set DivId after drawing
      this.SetDivId(this.frame);
   }
   
   DABC.BnetPainter.prototype.DisplayItem = function(itemname) {
      if (!this.mdi) return;
      
      var frame = this.mdi.FindFrame("bnet_drawing");
      if (frame) this.mdi.CleanupFrame(frame);
      frame = this.mdi.FindFrame("bnet_drawing", true);
      if (frame) 
         this.hpainter.display(itemname.substr(1), "divid:"+d3.select(frame).attr('id'));         
   }
   
   DABC.BnetPainter.prototype.DisplayCalItem = function(hubid, itemname) {
      this.CalibrHub = hubid;
      this.CalibrItem = itemname;
      
      d3.select(this.frame).select('.bnet_trb_info')
                           .style("display", (itemname || hubid) ? null : "none")
                           .select("legend").html("HUB: 0x" + hubid.toString(16));
      
      d3.select(this.frame).select('.bnet_tdc_calibr').html(""); // clear
   }
   
   DABC.BnetPainter.prototype.ProcessReq = function(isbuild, indx, res) {
      var frame = d3.select(this.frame), elem, 
          html = "", itemname = "", hadaqinfo = null, hadaqdata = null, hadaqevents = null, hadaqstate = null;

      for (var n=0;n<res._childs.length;++n) {
         var chld = res._childs[n];
         if (chld._name == "HadaqData") hadaqdata = chld; else 
         if (chld._name == "HadaqEvents") hadaqevents = chld; else
         if (chld._name == "HadaqInfo") hadaqinfo = chld; else
         if (chld._name == "State") hadaqstate = chld;
      }

      html += "<pre style='margin:0'>";
      
      if (isbuild) {
         this.BuilderInfo[indx] = res;
         elem = frame.select(".bnet_builder" + indx);
         var col = "red";
         if (hadaqstate.value == "NoFile") col = "yellow"; else
         if (hadaqstate.value == "Ready") col = "lightgreen";
         html += this.MakeLabel("style='background-color:" + col + "' title='State: " + hadaqstate.value + "'", this.BuilderNodes[indx].substr(7), 15);
         itemname = this.BuilderItems[indx];
      } else {
         this.InputInfo[indx] = res;
         elem = frame.select(".bnet_input" + indx);
         var col = "red";
         if (hadaqstate.value == "NoCalibr") col = "yellow"; else
         if (hadaqstate.value == "Ready") col = "lightgreen";
         html += this.MakeLabel("style='background-color:" + col + "' title='State: " + hadaqstate.value + "'", this.InputNodes[indx].substr(7), 15);
         itemname = this.InputItems[indx];
      }
      
      var prefix = "class='bnet_item_label' itemname='" + itemname + "/";
      
      html += "| " + this.MakeLabel(prefix + hadaqdata._name + "'", hadaqdata.value, 10); 
      html += "| " + this.MakeLabel(prefix + hadaqevents._name + "'", hadaqevents.value, 10);
      
      if (isbuild) {
         html += "| " + this.MakeLabel("", res.runname, 23);
         html += "| " + this.MakeLabel("", (res.runsize/1024/1024).toFixed(2), 10);
         // if (hadaqinfo) html += "| " + this.MakeLabel(prefix + hadaqinfo._name + "'", hadaqinfo.value, 30);
      } else {
         // info with HUBs and port numbers
         var totallen = 0;
         html += "|";
         if (res.ports && res.hubs && (res.ports.length == res.hubs.length)) {
            for (var k=0;k<res.ports.length;++k) {
               if (this.CalibrHub == res.hubs[k])
                  frame.select(".bnet_hub_info").html("<pre>" + res.hubs_info[k] + "</pre>");
               var txt = "0x"+res.hubs[k].toString(16);
               totallen += txt.length;
               var title = "state:" + res.hubs_state[k] + " " + res.hubs_info[k];
               var style = "background-color:";
               if (res.hubs_state[k]=="Ready") style+="lightgreen"; else
               if (res.hubs_state[k]=="NoCalibr") style+="lightblue"; else style+="red";
               var calitem = "";
               if (res.calibr[k]) 
                  calitem = itemname.substr(0, itemname.lastIndexOf("/")+1) + res.calibr[k];
               var attr = "hubid='" + res.hubs[k] + "' itemname='" + calitem + "' class='bnet_trb_label'";
               
               html += " " + this.MakeLabel("title='" + title + "' style='" + style + "' " + attr, txt, txt.length);
               if (totallen>40) break;
            }
         } 

         if (!totallen) html += " " + this.MakeLabel("", "failure", 40);
      }
      
      html += "</pre>";
      
      var painter = this, 
          main = elem.html(html);
      
      main.selectAll(".bnet_item_label").on("click", function() {
        painter.DisplayItem(d3.select(this).attr("itemname"));
      });
      main.selectAll(".bnet_trb_label").on("click", function() {
         painter.DisplayCalItem(parseInt(d3.select(this).attr("hubid")), d3.select(this).attr("itemname"));
      });
   }
   
   DABC.BnetPainter.prototype.ProcessCalibrReq = function(res) {
      if (!res) return;
      
      this.CalibrInfo = res;
      
      DABC.UpdateTRBStatus($(this.frame).find('.bnet_tdc_calibr'), res, this.hpainter, false);
   }
   
   DABC.BnetPainter.prototype.SendInfoRequests = function() {
      for (var n in this.InputItems)
         JSROOT.NewHttpRequest(this.InputItems[n] + "/get.json", "object", this.ProcessReq.bind(this, false, n)).send();
      for (var n in this.BuilderItems)
         JSROOT.NewHttpRequest(this.BuilderItems[n] + "/get.json", "object", this.ProcessReq.bind(this, true, n)).send();
      if (this.CalibrItem)
         JSROOT.NewHttpRequest(this.CalibrItem + "/Status/get.json", "object", this.ProcessCalibrReq.bind(this)).send();
   }
   
   DABC.BnetPainter.prototype.SendMainRequest = function() {
      if (this.mainreq) return;
      
      var pthis = this;
      
      this.mainreq = JSROOT.NewHttpRequest(this.itemname + "/get.json", "object", function(res) {
         pthis.mainreq = null;
         if (!res) return;
         
         var inp = null, bld = null, state = null, ninp = [], nbld = [],  changed = false;
         for (var k in res._childs) {
            var elem = res._childs[k];
            if (elem._name == "Inputs") { inp = elem.value; ninp = elem.nodes; } else
            if (elem._name == "Builders") { bld = elem.value; nbld = elem.nodes; } else
            if (elem._name == "State") { state = elem.value }
         }
         
         if (state !== null) {
            d3.select(pthis.frame).select(".bnet_state").text("Run control: "+state);
         }
         
         if (!DABC.CompareArrays(pthis.InputItems,inp)) {
            pthis.InputItems = inp;
            pthis.InputNodes = ninp;
            pthis.InputInfo = [];
            changed = true;
         }
         
         if (!DABC.CompareArrays(pthis.BuilderItems,bld)) {
            pthis.BuilderItems = bld;
            pthis.BuilderNodes = nbld;
            pthis.BuilderInfo = [];
            changed = true;
         }
         
         
         if (changed) {
            pthis.DisplayCalItem(0, "");
            pthis.RefreshHTML();
            pthis.hpainter.reload(); // also refresh hpainter - most probably items are changed 
         }

         pthis.SendInfoRequests();
      });

      this.mainreq.send();
      
   }
   
   DABC.BnetControl = function(hpainter, itemname) {
      hpainter.CreateCustomDisplay(itemname, "vert2", function(mdi) {
         var painter = new DABC.BnetPainter(hpainter, mdi, itemname);
         if (painter.active()) {
            painter.RefreshHTML();
            painter.main_timer = setInterval(painter.SendMainRequest.bind(painter), 2000);
         }
      });
   }
   
   // ================================== NEW CODE ========================================================

   
   DABC.ReqH = function(hpainter, item, url) {
      return 'get.json.gz?compact=3';
   }

   DABC.ConvertH = function(hpainter, item, obj) {
      if (!obj) return;

      var res = {};
      for (var k in obj) res[k] = obj[k]; // first copy all keys 
      for (var k in res) delete obj[k];   // then delete them from source
      
      if (res._kind == 'ROOT.TH1D') {
         JSROOT.Create("TH1D", obj);
         JSROOT.extend(obj, { fName: res._name, fTitle: res._title });
         
         JSROOT.extend(obj.fXaxis, { fNbins: res.nbins, fXmin: res.left,  fXmax: res.right });
         res.bins.splice(0,3); // 3 first items in array are not bins
         obj.fArray = res.bins; 
         obj.fNcells = res.nbins + 2;
      } else if (res._kind == 'ROOT.TH2D') {
         JSROOT.Create("TH2D", obj);
         JSROOT.extend(obj, { fName: res._name, fTitle: res._title });
         
         JSROOT.extend(obj.fXaxis, { fNbins:res.nbins1, fXmin: res.left1,  fXmax: res.right1 });
         JSROOT.extend(obj.fYaxis, { fNbins:res.nbins2, fXmin: res.left2,  fXmax: res.right2 });
         res.bins.splice(0,6); // 6 first items in array are not bins
         obj.fNcells = (res.nbins1+2) * (res.nbins2+2);
         obj.fArray = res.bins;
      } else {
         return;
      }

      if ('xtitle' in res) obj.fXaxis.fTitle = res.xtitle;         
      if ('ytitle' in res) obj.fYaxis.fTitle = res.ytitle;   
      if ('xlabels' in res) {
         obj.fXaxis.fLabels = JSROOT.Create('THashList');
         var lbls = res.xlabels.split(",");
         for (var n in lbls) {
            var lbl = JSROOT.Create('TObjString');
            lbl.fUniqueID = parseInt(n)+1;
            lbl.fString = lbls[n];
            obj.fXaxis.fLabels.Add(lbl);
         }
      }
      if ('ylabels' in res) {
         obj.fYaxis.fLabels = JSROOT.Create('THashList');
         var lbls = res.ylabels.split(",");
         for (var n in lbls) {
            var lbl = JSROOT.Create('TObjString');
            lbl.fUniqueID = parseInt(n)+1;
            lbl.fString = lbls[n];
            obj.fYaxis.fLabels.Add(lbl);
         }
      }
   }

   
   DABC.ExtractSeries = function(name, kind, obj, history) {

      var ExtractField = function(node) {
         if (!node || !(name in node)) return null;

         if (kind=="number") return Number(node[name]); 
         if (kind=="time") {
            var d  = new Date(node[name]);
            return d.getTime() / 1000.;
         }
         return node[name];
      }
      
      // xml node must have attribute, which will be extracted
      var val = ExtractField(obj);
      if (val==null) return null;

      var arr = new Array();
      arr.push(val);

      if (history!=null) 
         for (var n=history.length-1;n>=0;n--) {
            // in any case stop iterating when see property delete 
            if ("dabc:del" in history[n]) break; 
            var newval = ExtractField(history[n]);
            if (newval!=null) val = newval;
            arr.push(val);
         }

      arr.reverse();
      return arr;
   }
   
   DABC.MakeItemRequest = function(h, item, fullpath, option) {
      item['fullitemname'] = fullpath;
      if (!('_history' in item) || (option=="gauge") || (option=='last')) return "get.json?compact=0"; 
      if (!('hlimit' in item)) item.hlimit = 100;
      var url = "get.json?compact=0&history=" + item.hlimit;
      if (('request_version' in item) && (item.request_version>0)) url += "&version=" + item.request_version;
      item['request_version'] = 0;
      return url;      
   }
   
   DABC.AfterItemRequest = function(h, item, obj, option) {
      if (obj==null) return;
      
      if (!('_history' in item) || (option=="gauge") || (option=='last')) {
         obj.fullitemname = item.fullitemname; 
         // for gauge output special scripts should be loaded, use unique class name for it
         if (obj._kind == 'rate') obj._typename = "DABC_RateGauge";
         return;
      }
      
      var new_version = Number(obj._version);

      var modified = (item.request_version != new_version);

      this.request_version = new_version;

      // this is array with history entries 
      var arr = obj.history;

      if (arr!=null) {
         // gap indicates that we could not get full history relative to provided version number
         var gap = obj.history_gap;

         // join both arrays with history entries
         if ((item.history == null) || (arr.length >= item['hlimit']) || gap) {
            item.history = arr;
         } else
            if (arr.length>0) {
               modified = true;
               var total = item.history.length + arr.length; 
               if (total > item['hlimit']) 
                  this.history.splice(0, total - item['hlimit']);

               item.history = item.history.concat(arr);
            }
      }
      
      if (obj._kind == "log") {
         obj.log = DABC.ExtractSeries("value","string", obj, item.history);
         return;
      }
      
      // now we should produce TGraph from the object

      var x = DABC.ExtractSeries("time", "time", obj, item.history);
      var y = DABC.ExtractSeries("value", "number", obj, item.history);

      for (var k in obj) delete obj[k];  // delete all object keys
      
      obj._typename = 'TGraph';
      JSROOT.Create('TGraph', obj);
      obj.fHistogram = JSROOT.CreateTH1(x.length);

      var _title = item._title || item.fullitemname;
      
      JSROOT.extend(obj, { fBits: 0x3000408, fName: item._name, fTitle: _title, 
                           fX:x, fY:y, fNpoints: x.length,
                           fLineColor: 2, fLineWidth: 2 });
      
      var xrange = x[x.length-1] - x[0];
      obj.fHistogram.fTitle = obj.fTitle;
      obj.fHistogram.fXaxis.fXmin = x[0] - xrange*0.03;
      obj.fHistogram.fXaxis.fXmax = x[x.length-1] + xrange*0.03;
      
      var ymin = Math.min.apply(null,y), ymax = Math.max.apply(null,y);  
      
      obj.fHistogram.fYaxis.fXmin = ymin - 0.05 * (ymax-ymin);
      obj.fHistogram.fYaxis.fXmax = ymax + 0.05 * (ymax-ymin);
      
      obj.fHistogram.fXaxis.fTimeDisplay = true;
      obj.fHistogram.fXaxis.fTimeFormat = "%H:%M:%S%F0"; // %FJanuary 1, 1970 00:00:00
   }
   
   DABC.DrawGauage = function(divid, obj, opt) {
      
      // at this momemnt justgauge should be loaded

      if (typeof Gauge == 'undefined') {
         alert('gauge.js not loaded');
         return null;
      }
      
      var painter = new JSROOT.TObjectPainter(obj);
      
      painter.gauge = null;
      painter.min = 0;
      painter.max = 1;
      painter.lastval = 0;
      painter.lastsz = 0;
      painter._title = "";
      
      painter.Draw = function(obj) {
         if (!obj) return;
         
         var val = Number(obj["value"]);
         
         if ('low' in obj) {
            var min = Number(obj["low"]);
            if (!isNaN(min) && (min<this.min)) this.min = min;
         }
         
         if ('up' in obj) {
            var max = Number(obj["up"]);
            if (!isNaN(max) && (max>this.max)) this.max = max;
         }
         
         var redo = false;
         
         if (val > this.max) {
            this.max = 1;
            var cnt = 0;
            while (val > this.max) 
               this.max *= (((cnt++ % 3) == 1) ? 2.5 : 2);
            
            redo = true;
         }

         this._title = obj._name || "gauge"
         val = JSROOT.FFormat(val,"5.3g");
         this.DrawValue(val, redo);
      }
      
      painter.DrawValue = function(val, force) {
         var rect = d3.select("#"+this.divid).node().getBoundingClientRect();
         var sz = Math.min(rect.height, rect.width);
         
         if ((sz > this.lastsz*1.2) || (sz < this.lastsz*0.9)) force = true; 
         
         if (force) {
            d3.select("#"+this.divid).selectAll("*").remove();
            this.gauge = null; 
         }
         
         this.lastval = val;
         
         if (!this.gauge) {
            this.lastsz = sz;   
            
            var config =  {
               size: sz,
               label: this._title,
               min: this.min,
               max: this.max,
               majorTicks : 6,
               minorTicks: 5
            };
            
            if ('units' in obj) config['units'] = obj['units'] + "/s";
            
            if (this.gauge == null)
               this.gauge = new Gauge(this.divid, config);
            else
               this.gauge.configure(config);
            
            this.gauge.render(val);

         } else {
            this.gauge.redraw(val);
         }
         
         // always set painter 
         this.SetDivId(this.divid);
      }
      
      painter.CheckResize = function() {
         this.DrawValue(this.lastval);
      }
      
      painter.RedrawObject = function(obj) {
         this.Draw(obj);
         return true;
      }
      
      painter.SetDivId(divid, -1);
      
      painter.Draw(obj);
      
      return painter.DrawingReady();
   }
   
   // ==========================================================================================

   DABC.DrawLog = function(divid, obj, opt) {
      var painter = new JSROOT.TBasePainter();
      painter.obj = obj;
      painter.history = (opt!="last") && ('log' in obj); // by default draw complete history
      
      var frame = d3.select("#"+ divid);
      if (painter.history) {
         frame.html("<div style='overflow:auto; max-height: 100%; max-width: 100%; font-family:monospace'></div>");
      } else {
         frame.html("<div></div>");
      }
      // set divid after child element created - only then we could set painter
      painter.SetDivId(divid);
      
      painter.RedrawObject = function(obj) {
         this.obj = obj;
         this.Draw();
         return true;
      }
      
      painter.Draw = function() {
         var html = "";
         
         if (this.history && ('log' in this.obj)) {
            for (var i in this.obj.log) {
               html+="<pre>"+this.obj.log[i]+"</pre>";
            }
         } else {
            html += obj['fullitemname'] + "<br/>";
            html += "<h5>"+ this.obj.value +"</h5>";
         }
         d3.select("#" + this.divid + " > div").html(html);
      }
      
      painter.Draw();
      
      return painter.DrawingReady();
   }
   
   
   DABC.DrawCommand = function(divid, obj, opt) {
      
      painter = new JSROOT.TBasePainter;
      
      painter.SetDivId(divid, -1);
      painter.jsonnode = obj;
      painter.req = null;

      painter.NumArgs = function() {
         if (this.jsonnode==null) return 0;
         return this.jsonnode["numargs"];
      }

      painter.ArgName = function(n) {
         return (n<this.NumArgs()) ? this.jsonnode["arg"+n] : "";
      }

      painter.ArgKind = function(n) {
         return (n<this.NumArgs()) ? this.jsonnode["arg"+n+"_kind"] : "";
      }

      painter.ArgDflt = function(n) {
         return (n<this.NumArgs()) ? this.jsonnode["arg"+n+"_dflt"] : "";
      }

      painter.ArgMin = function(n) {
         return (n<this.NumArgs()) ? this.jsonnode["arg"+n+"_min"] : null;
      }

      painter.ArgMax = function(n) {
         return (n<this.NumArgs()) ? this.jsonnode["arg"+n+"_max"] : null;
      }
      
      painter.ShowCommand = function() {

         var frame = $("#" + this.divid);

         frame.empty();

         if (this.jsonnode==null) {
            frame.append("cannotr access command definition...<br/>");
            return;
         } 

         frame.append("<h3>" + this.jsonnode.fullitemname + "</h3>");
         
         var entryInfo = "<input id='" + this.divid + "_button' type='button' title='Execute' value='Execute'/><br/>";

         for (var cnt=0;cnt<this.NumArgs();cnt++) {
            var argname = this.ArgName(cnt);
            var argkind = this.ArgKind(cnt);
            var argdflt = this.ArgDflt(cnt);

            var argid = this.divid + "_arg" + cnt; 
            var argwidth = (argkind=="int") ? "80px" : "170px";

            entryInfo += "Arg: " + argname + " "; 
            entryInfo += "<input id='" + argid + "' style='width:" + argwidth + "' value='"+argdflt+"' argname = '" + argname + "'/>";    
            entryInfo += "<br/>";
         }

         entryInfo += "<div id='" +this.divid + "_res'/>";

         frame.append(entryInfo);
         
         var pthis = this;
         
         $("#"+ this.divid + "_button").click(function() { pthis.InvokeCommand(); });

         for (var cnt=0;cnt<this.NumArgs();cnt++) {
            var argid = this.divid + "_arg" + cnt;
            var argkind = this.ArgKind(cnt);
            var argmin = this.ArgMin(cnt);
            var argmax = this.ArgMax(cnt);

            if ((argkind=="int") && (argmin!=null) && (argmax!=null))
               $("#"+argid).spinner({ min:argmin, max:argmax});
         }
      }
      
      painter.InvokeCommand = function() {
         if (this.req!=null) return;

         var resdiv = $("#" + this.divid + "_res");
         resdiv.html("<h5>Send command to server</h5>");

         var url = this.jsonnode.fullitemname + "/execute";

         for (var cnt=0;cnt<this.NumArgs();cnt++) {
            var argid = this.divid + "_arg" + cnt;
            var argkind = this.ArgKind(cnt);
            var argmin = this.ArgMin(cnt);
            var argmax = this.ArgMax(cnt);

            var arginp = $("#"+argid);

            if (cnt==0) url+="?"; else url+="&";

            url += arginp.attr("argname");
            url += "=";
            if ((argkind=="int") && (argmin!=null) && (argmax!=null))
               url += arginp.spinner("value");
            else
               url += new String(arginp.val());
         }

         var pthis = this;
         
         this.req = JSROOT.NewHttpRequest(url,"object", function(res) {
            pthis.req = null;
            if (res==null) resdiv.html("<h5>missing reply from server</h5>");
            else resdiv.html("<h5>Get reply res=" + res['_Result_'] + "</h5>");
         });

         this.req.send(null);

      }
      
      painter.ShowCommand();
      
      painter.SetDivId(divid);
      
      return painter.DrawingReady();
   }
   
   // =============================================================
   
   JSROOT.addDrawFunc({
      name: "kind:rate",
      icon: DABC.source_dir + "../img/gauge.png",
      func: "<dummy>",
      opt: "line;gauge",
      monitor: true,
      make_request: DABC.MakeItemRequest,
      after_request: DABC.AfterItemRequest
   });

   JSROOT.addDrawFunc({
      name: "kind:log",
      icon: "img_text", 
      func: DABC.DrawLog,
      opt: "log;last",
      monitor: 'always',
      make_request: DABC.MakeItemRequest,
      after_request: DABC.AfterItemRequest
   });

   JSROOT.addDrawFunc({
      name: "kind:DABC.Command",
      icon: DABC.source_dir + "../img/dabc.png",
      make_request: DABC.MakeItemRequest,
      after_request: DABC.AfterItemRequest,
      opt: "command",
      prereq: 'jq',
      monitor: 'never',
      func: 'DABC.DrawCommand'
   });

   // only indicate that item with such kind can be opened as direct link
   JSROOT.addDrawFunc({
      name: "kind:DABC.HTML",
      icon: DABC.source_dir + "../img/dabc.png",
      aslink: true
   });
   
   // example of external scripts loading
   JSROOT.addDrawFunc({ 
      name: "DABC_RateGauge", 
      func: "DABC.DrawGauage", 
      opt: "gauge",
      script: DABC.source_dir + 'gauge.js' 
   });

   
})();
