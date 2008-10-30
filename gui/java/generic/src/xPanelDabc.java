package xgui;
/**
* @author goofy
*/
import javax.swing.JLabel;
import javax.swing.JButton;
import javax.swing.JTextField;
import javax.swing.JPasswordField;
import javax.swing.JCheckBox;
import javax.swing.ImageIcon;
import java.awt.Dimension;
import java.awt.Point;
//import javax.swing.GroupLayout;

import java.io.IOException;
import java.awt.*;
import java.awt.event.*;
import java.util.*;
import java.io.*;
import java.awt.Color;
import org.w3c.dom.*;
import org.xml.sax.*;
import javax.xml.parsers.*;

import dim.*;

/**
* DIM GUI class
*/
public class xPanelDabc extends xPanelPrompt implements ActionListener, Runnable 
{
private xRemoteShell dabcshell;
private ImageIcon storeIcon, closeIcon, dabcIcon, launchIcon, killIcon, workIcon;
private ImageIcon configIcon, enableIcon, startIcon, stopIcon, haltIcon, exitIcon, setupIcon, wcloseIcon;
private JTextField DimName, DabcNode, DabcServers, DabcName, Username, DabcUserpath, DabcPath, DabcScript, DabcSetup, DabcLaunchFile;
private JPasswordField Password;
private JCheckBox getnew;
private xDimBrowser browser;
private xiDesktop desk;
private xInternalFrame progress;
private xState progressState;
private xFormDabc setupDabc;
private ActionListener action;
private DocumentBuilderFactory factory;
private DocumentBuilder builder;
private Document document;
private String dabcMaster;
private String Action;
private int nServers;
private Element root, elem;
private xDimCommand doConfig, doEnable, doStart, doStop, doHalt;
private Vector<xDimParameter> runState;
private Vector<Object> doExit, doShutdown;
private Thread threxe;
private ActionEvent ae;
private xTimer etime;
private boolean threadRunning=false;
private Point panpnt;
private xSetup setup;
private Vector<xPanelSetup> PanelSetupList;
private Vector<String> names;
private Vector<String> types;
private Vector<String> values;
private Vector<String> titles;


public xPanelDabc(String title, xDimBrowser diminfo, xiDesktop desktop, ActionListener al) {
    super(title);
    action=al;
    desk=desktop;
    browser=diminfo;
    dabcIcon    = xSet.getIcon("icons/DABClogo1small.png");
    closeIcon   = xSet.getIcon("icons/fileclose.png");
    workIcon    = xSet.getIcon("icons/control.png");
    setupIcon   = xSet.getIcon("icons/usericon.png");
    wcloseIcon  = xSet.getIcon("icons/windowclose.png");
    storeIcon   = xSet.getIcon("icons/savewin.png");
    launchIcon  = xSet.getIcon("icons/connprm.png");
    killIcon    = xSet.getIcon("icons/disconn.png");
    configIcon  = xSet.getIcon("icons/dabcconfig.png");
    enableIcon  = xSet.getIcon("icons/dabcenable.png");
    startIcon   = xSet.getIcon("icons/dabcstart.png");
    stopIcon    = xSet.getIcon("icons/dabcstop.png");
    haltIcon    = xSet.getIcon("icons/dabchalt.png");
    exitIcon    = xSet.getIcon("icons/exitall.png");
    addButton("dabcQuit","Close window",closeIcon,this);
    addButton("ReadSetup","Read setup file from user path",setupIcon,this);
    addButton("CloseWindows","Close setup windows",wcloseIcon,this);
    addButton("dabcSave","Save this form and setup to files",storeIcon,this);
    addButton("dabcLaunch","Launch DABC",launchIcon,this);
    addButton("dabcConfig","Configure DABC",configIcon,this);
    // addButton("dabcEnable","Enable DABC",enableIcon,this);
    addButton("dabcStart","Start",startIcon,this);
    addButton("dabcStop","Stop",stopIcon,this);
    addButton("dabcHalt","Halt",haltIcon,this);
    addButton("dabcExit","Exit and shut down",killIcon,this);
    addButton("dabcCleanup","Cleanup DABC",exitIcon,this);
    addButton("dabcShell","ssh Node -l Username Script",workIcon,this);
    int width=25;
    // read dabc setup from file
    if(System.getenv("DABC_LAUNCH_DABC")!=null)
        setupDabc=new xFormDabc(System.getenv("DABC_LAUNCH_DABC"));
    else setupDabc=new xFormDabc("DabcLaunch.xml");
    setupDabc.addActionListener(this);
    xSet.addObject(setupDabc);
    // setupDabc=(xFormDabc)xSet.getObject("xgui.xFormDabc"); // how to retrieve
    DimName=new JTextField(xSet.getDimDns(),width);
    DimName.setEditable(false);
    Username=new JTextField(xSet.getUserName(),width);
    Username.setEditable(false);
    Password=new JPasswordField(width);
    Password.setEchoChar('*');
    Password.addActionListener(this);
    Password.setActionCommand("setpwd");
    
    addPrompt("Name server: ",DimName);
    addPrompt("User name: ",Username);
    addPrompt("Password [RET]: ",Password);
    DabcNode=addPrompt("Master node: ",setupDabc.getMaster(),"set",width,this);
    DabcName=addPrompt("Master name: ",setupDabc.getName(),"set",width,this);
    DabcServers=addPrompt("Servers: ",setupDabc.getServers(),"set",width,this);
    DabcPath=addPrompt("System path: ",setupDabc.getSystemPath(),"set",width,this);
    DabcUserpath=addPrompt("User path: ",setupDabc.getUserPath(),"set",width,this);
    DabcSetup=addPrompt("Setup file: ",setupDabc.getSetup(),"set",width,this);
    DabcScript=addPrompt("Script: ",setupDabc.getScript(),"set",width,this);
    DabcLaunchFile=addPrompt("Launch file: ",setupDabc.getLaunchFile(),"set",width,this);
    
// Add checkboxes
    getnew = new JCheckBox();
    getnew.setSelected(true);
    addCheckBox("Get new setup",getnew);

    panpnt = new Point(0,1); // layout for compound
    dabcshell = new xRemoteShell("ssh");
    nServers=1+Integer.parseInt(setupDabc.getServers()); // add DNS
    setDimServices();
    System.out.println("DABC  servers needed: DNS + "+(nServers-1));
    etime = new xTimer(al, false); // fire only once
}

private void setLaunch(){
xSet.setAccess(Password.getPassword());
setupDabc.setMaster(DabcNode.getText());
setupDabc.setServers(DabcServers.getText());
setupDabc.setSystemPath(DabcPath.getText());
setupDabc.setUserPath(DabcUserpath.getText());
setupDabc.setScript(DabcScript.getText());
setupDabc.setLaunchFile(DabcLaunchFile.getText());
setupDabc.setName(DabcName.getText());
setupDabc.setSetup(DabcSetup.getText());
//setupDabc.printForm();
}
// get from command list special commands for buttons.
public void setDimServices(){
int i;
releaseDimServices();
System.out.println("Dabc setDimServices");
doExit=new Vector<Object>();
runState=new Vector<xDimParameter>();
Vector<xDimCommand> list=browser.getCommandList();
for(i=0;i<list.size();i++){
if(list.get(i).getParser().getFull().indexOf(DabcName.getText()+"/DoConfigure")>0) doConfig=list.get(i);
if(list.get(i).getParser().getFull().indexOf(DabcName.getText()+"/DoEnable")>0) doEnable=list.get(i);
if(list.get(i).getParser().getFull().indexOf(DabcName.getText()+"/DoStart")>0) doStart=list.get(i);
if(list.get(i).getParser().getFull().indexOf(DabcName.getText()+"/DoStop")>0) doStop=list.get(i);
if(list.get(i).getParser().getFull().indexOf(DabcName.getText()+"/DoHalt")>0) doHalt=list.get(i);
if(list.get(i).getParser().getFull().indexOf("/EXIT")>0) doExit.add(list.get(i));
}
Vector<xDimParameter> para=browser.getParameterList();
if(para != null)for(i=0;i<para.size();i++){
if(para.get(i).getParser().getFull().indexOf("/RunStatus")>0) runState.add(para.get(i));
}}

public void releaseDimServices(){
System.out.println("Dabc releaseDimServices");
doConfig=null;
doEnable=null;
doStart=null;
doStop=null;
doHalt=null;
if(doExit != null) doExit.removeAllElements();
if(runState != null) runState.removeAllElements();
doExit=null;
runState=null;
}

private void setProgress(String info, Color color){
setTitle(info,color);
if(threadRunning) progressState.redraw(-1,xSet.blueL(),info, true);
}
private void startProgress(){
    xLayout la= new xLayout("progress");
    la.set(new Point(50,200), new Dimension(300,100),0,true);
    progress=new xInternalFrame("Work in progress, please wait", la);
    progressState=new xState("Current action:",350,30);
    progressState.redraw(-1,"Green","Starting", true);
    progressState.setSizeXY();
    progress.setupFrame(workIcon, null, progressState, true);
    etime.action(new ActionEvent(progress,1,"DisplayFrame"));
}
private void stopProgress(){
    etime.action(new ActionEvent(progress,1,"RemoveFrame"));
}

private boolean waitState(int timeout, String state){
int t=0;
boolean ok;
System.out.print("Wait for "+state);
    while(t < timeout){
        ok=true;
        for(int i=0;i<runState.size();i++){
            if(!runState.elementAt(i).getValue().equals(state)) ok=false;
        }
        setProgress(new String("Wait for "+state+" "+t+" ["+timeout+"]"),xSet.blueD());
        if(ok) return true;
        System.out.print(".");
        browser.sleep(1);
        t++;
    }
    return false;
}
//React to menu selections.
public void actionPerformed(ActionEvent e) {
boolean doit=true;
if ("set".equals(e.getActionCommand())) {
setLaunch();
return;
}
if ("setpwd".equals(e.getActionCommand())) {
setLaunch();
return;
}
if ("ReadSetup".equals(e.getActionCommand())) {
int off[]=new int[100],len[]=new int[100],ind,i;
    String name,header;
    if(getnew.isSelected()){
        setup=null;
        if(titles != null) for(i=0;i<titles.size();i++) desk.removeDesktop(titles.get(i));
        titles=null;
    }
    getnew.setSelected(false);
    if(setup == null){
        setup=new xSetup();
        if(!setup.parseSetup(DabcUserpath.getText()+"/"+DabcSetup.getText())) {
            tellError("Setup parse failed");
            System.out.println("Setup parse failed: "+DabcUserpath.getText()+"/"+DabcSetup.getText());
            getnew.setSelected(true);
            setup=null;
            titles=null;
            return;
        }
        names=setup.getNames();
        types=setup.getTypes();
        values=setup.getValues();
        // number of nodes
        int number=setup.getContextNumber();
        PanelSetupList=new Vector<xPanelSetup>(0);
        titles=new Vector<String>(0);
        ind=1;
        off[0]=0; // first offset is 0
        for(i=1;i<names.size();i++){
            if(names.get(i).equals("Context")) {
                off[ind]=i;
                len[ind-1]=i-off[ind-1];
                ind++;
            }
        }    
        len[ind-1]=names.size()-off[ind-1];
        // create the panels
        for(i=0;i<ind;i++){
            header=new String(values.get(off[i]+1));
            titles.add(values.get(off[i]));
            PanelSetupList.add(new xPanelSetup(header,names,types,values,off[i],len[i]));
        }
    }
    System.out.println("Setup parse: "+DabcUserpath.getText()+"/"+DabcSetup.getText());
    if(titles != null) for(i=0;i<titles.size();i++) desk.removeDesktop(titles.get(i));
    // display panels
    for(i=0;i<PanelSetupList.size();i++){
        name=new String(titles.get(i));
        xLayout panlo=xSet.getLayout(name); // if not loaded from setup file, create new one
        panlo=xSet.getLayout(name);
        if(panlo==null)panlo=xSet.createLayout(name,new Point(100+100*i,200+10*i), new Dimension(100,75),1,true);
        xInternalCompound ic =new xInternalCompound(name,setupIcon, panpnt, panlo,null);
        ic.rebuild(PanelSetupList.get(i)); // top 2, bottom 1
        desk.addDesktop(ic); // old frame will be deleted
        ic.moveToFront();
    }
    desk.frameToFront("DabcLauncher");
    return;
}
else if ("dabcSave".equals(e.getActionCommand())) {
    xLogger.print(1,Action+" "+DabcLaunchFile.getText());
    setLaunch();
    setupDabc.saveSetup(DabcLaunchFile.getText());
    String msg=new String("Dabc launch: "+DabcLaunchFile.getText());
    if(setup != null){
        for(int i=0;i<PanelSetupList.size();i++)PanelSetupList.get(i).updateList();
        if(!setup.updateSetup()) tellError("Setup update failed");
        else { 
        if(!setup.writeSetup(DabcUserpath.getText()+"/"+DabcSetup.getText())) tellError("Write setup failed");
        else {
            String mes=new String(msg+"\nDabc setup: "+DabcUserpath.getText()+"/"+DabcSetup.getText());
            tellInfo(mes);
        }
        }
    } else  tellInfo(msg);
    return;
}
else if ("CloseWindows".equals(e.getActionCommand())) {
    if(titles != null) for(int i=0;i<titles.size();i++) desk.removeDesktop(titles.get(i));
    return;
}
    
if(!threadRunning){
    Action = new String(e.getActionCommand());
    // must do confirm here, because in thread it would block forever
    if ("dabcExit".equals(Action)) doit=choose("Exit, shut down and cleanup DABC?");
    if ("dabcCleanup".equals(Action)) doit=choose("Kill DABC tasks?");
    if(doit){
        startProgress();
        ae=e;
        threxe = new Thread(this);
        threadRunning=true;
        threxe.start();
    }
    } else tellError("Execution thread not yet finished!");
}
// start thread by threxe.start()
// CAUTION: Do not use tell or choose here: Thread will never continue!
public void run(){
    dabcMaster = DabcNode.getText();
    
    if ("dabcQuit".equals(Action)) {
        xLogger.print(1,Action);
    }
    else if ("dabcLaunch".equals(Action)) {
        setProgress("Launch DABC servers, wait 5 sec ...",xSet.blueD());
        String cmd = new String(DabcPath.getText()+
                                "/script/dabcstartup.sh "+DabcPath.getText()+" "+
                                DabcUserpath.getText()+" "+DabcSetup.getText()+" "+
                                xSet.getDimDns()+" "+dabcMaster+" "+xSet.getAccess()+" &");
        xLogger.print(1,cmd);
        int timeout=0;
        int nserv=0;
        if(dabcshell.rsh(dabcMaster,Username.getText(),cmd,5L)){
            System.out.print("DABC wait "+(nServers-1));
            nserv=browser.getNofServers();
            while(nserv < nServers){
                setProgress(new String("Wait "+timeout+" [10] for "+(nServers-1)+" servers, found "+nserv),xSet.blueD());
                System.out.print("."+nserv);
                browser.sleep(1);
                timeout++;
                if(timeout > 10) break;
                nserv=browser.getNofServers();
            }
        if(nserv >= nServers){
            System.out.println("\nDABC connnected "+(nServers-1)+" servers");
            setProgress("OK: DABC servers ready, update parameters ...",xSet.blueD());
            xSet.setSuccess(false);
            etime.action(new ActionEvent(ae.getSource(),ae.getID(),"Update"));
            browser.sleep(1);
            if(!xSet.isSuccess()) {etime.action(new ActionEvent(ae.getSource(),ae.getID(),"Update"));
            browser.sleep(1);}
            if(!xSet.isSuccess()) setProgress(xSet.getMessage(),xSet.redD());
            else setProgress("OK: DABC servers ready",xSet.greenD());
            //setDimServices();
        }
        else {
            System.out.println("\nFailed ");
            setProgress("Servers missing: "+(nServers-nserv)+" from "+nServers,xSet.redD());
        }
        }
        else setProgress("Failed: DABC startup script",xSet.redD());
    }
    else if ("dabcShell".equals(Action)) {
        //xLogger.print(1,Command);
        xLogger.print(1,"dabcShell: "+DabcScript.getText());
        dabcshell.rsh(DabcNode.getText(),Username.getText(),DabcScript.getText(),0L);
    }
    else if ("dabcCleanup".equals(Action)) {
            setProgress("DABC cleanup ...",xSet.blueD());
            String cmd = new String(DabcPath.getText()+
                                "/script/dabcshutdown.sh "+DabcPath.getText()+" "+
                                DabcUserpath.getText()+" "+DabcSetup.getText()+" "+
                                xSet.getDimDns()+" "+dabcMaster+" &");
            xLogger.print(1,cmd);
            dabcshell.rsh(dabcMaster,Username.getText(),cmd,0L);
            setProgress("OK: DABC down, update parameters ...",xSet.blueD());
            browser.sleep(2);
            xSet.setSuccess(false);
            etime.action(new ActionEvent(ae.getSource(),ae.getID(),"Update"));
            browser.sleep(1);
            if(!xSet.isSuccess()) {etime.action(new ActionEvent(ae.getSource(),ae.getID(),"Update"));
            browser.sleep(1);}
            if(!xSet.isSuccess()) setProgress(xSet.getMessage(),xSet.redD());
            else setProgress("OK: DABC down",xSet.greenD());
     }
    else if ("dabcExit".equals(Action)) {
        setProgress("Exit DABC ...",xSet.blueD());
        if(doExit != null){
            for(int i=0;i<doExit.size();i++){
                xLogger.print(1,((xDimCommand)doExit.elementAt(i)).getParser().getFull());
                ((xDimCommand)doExit.elementAt(i)).exec(xSet.getAccess());
            }
            setProgress("OK: DABC down, update parameters ...",xSet.blueD());
            browser.sleep(2);
            xSet.setSuccess(false);
            etime.action(new ActionEvent(ae.getSource(),ae.getID(),"Update"));
            browser.sleep(1);
            if(!xSet.isSuccess()) {etime.action(new ActionEvent(ae.getSource(),ae.getID(),"Update"));
            browser.sleep(1);}
            if(!xSet.isSuccess()) setProgress(xSet.getMessage(),xSet.redD());
            else setProgress("OK: DABC down",xSet.greenD());
        }
        else setProgress("No DABC exit commands available!",xSet.redD());
    }
    // all following need the commands
    else {
    if(doHalt == null) setDimServices();
    if(doHalt == null) {
        setProgress("No DABC commands available!",xSet.redD());
        threadRunning=false;
        stopProgress();
        System.out.println("Thread done!!!");
        //tellError("DABC commands not available!");
        return;
    }
    if ("dabcConfig".equals(Action)) {
        setProgress("Configure DABC",xSet.blueD());
        xLogger.print(1,doConfig.getParser().getFull());
        doConfig.exec(xSet.getAccess());
        if(waitState(10,"Configured")){
            setProgress("OK: DABC configured, enable ...",xSet.blueD());
            System.out.println(" ");
            xLogger.print(1,doEnable.getParser().getFull());
            doEnable.exec(xSet.getAccess());
            if(waitState(10,"Ready")){
                System.out.println(" ");
                xSet.setSuccess(false);
                setProgress("OK: DABC enabled, update parameters ...",xSet.blueD());
                etime.action(new ActionEvent(ae.getSource(),ae.getID(),"Update"));
                browser.sleep(1);
                if(!xSet.isSuccess()) {etime.action(new ActionEvent(ae.getSource(),ae.getID(),"Update"));
                browser.sleep(1);}
                if(!xSet.isSuccess()) setProgress(xSet.getMessage(),xSet.redD());
                else setProgress("OK: DABC configured and enabled",xSet.greenD());
                //setDimServices();
            } else setProgress("DABC enable failed",xSet.greenD());
        } else setProgress("DABC configure failed",xSet.redD());
    }
    else if ("dabcEnable".equals(Action)) {
        xLogger.print(1,doEnable.getParser().getFull());
        doEnable.exec(xSet.getAccess());
        setProgress("OK: DABC enabled",xSet.greenD());
    }
    else if ("dabcStart".equals(Action)) {
        xLogger.print(1,doStart.getParser().getFull());
        doStart.exec(xSet.getAccess());
        setProgress("OK: DABC started",xSet.greenD());
    }
    else if ("dabcHalt".equals(Action)) {
        setProgress("Halt DABC ...",xSet.blueD());
        xLogger.print(1,doHalt.getParser().getFull());
        doHalt.exec(xSet.getAccess());
        browser.sleep(3);
            if(waitState(10,"Halted")){
            setProgress("OK: DABC halteded, update parameters ...",xSet.blueD());
            xSet.setSuccess(false);
            etime.action(new ActionEvent(ae.getSource(),ae.getID(),"Update"));
            browser.sleep(1);
            if(!xSet.isSuccess()) {etime.action(new ActionEvent(ae.getSource(),ae.getID(),"Update"));
            browser.sleep(1);}
            if(!xSet.isSuccess()) setProgress(xSet.getMessage(),xSet.redD());
            else setProgress("OK: DABC halted, servers ready",xSet.greenD());
            //setDimServices();
        } else setProgress("DABC halt failed",xSet.redD());
    }
    else if ("dabcStop".equals(Action)) {
        xLogger.print(1,doStop.getParser().getFull());
        doStop.exec(xSet.getAccess());
        setProgress("DABC stopped",xSet.greenD());
    }
}
threadRunning=false;
stopProgress();
System.out.println("Thread done!!!");
}
}



