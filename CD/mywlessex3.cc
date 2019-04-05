
#include <fstream>
#include <iostream>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/aodv-module.h"
#include "ns3/olsr-module.h"
#include "ns3/dsdv-module.h"
#include "ns3/dsr-module.h"
#include "ns3/applications-module.h"
#include "myapp.h"
#include "ns3/flow-monitor-module.h"
using namespace ns3;
using namespace dsr;

NS_LOG_COMPONENT_DEFINE ("mywlessex3");

class RoutingExperiment
{
public:
  RoutingExperiment ();
  void Run (int nSinks, double txp, std::string CSVfileName);
  //static void SetMACParam (ns3::NetDeviceContainer & devices,
  //                                 int slotDistance);
  std::string CommandSetup (int argc, char **argv);

private:
  Ptr<Socket> SetupPacketReceive (Ipv4Address addr, Ptr<Node> node);
  void ReceivePacket (Ptr<Socket> socket);
  void CheckThroughput ();

  uint32_t port;
  uint32_t bytesTotal;
  uint32_t packetsReceived;

  std::string m_CSVfileName;
  int m_nSinks;
  std::string m_protocolName;
  double m_txp;
  bool m_traceMobility;
  uint32_t m_protocol;
};

RoutingExperiment::RoutingExperiment ()
  : port (9),
    bytesTotal (0),
    packetsReceived (0),
    m_CSVfileName ("mywless3_2.csv"),
    m_traceMobility (false),
    m_protocol (2) // AODV
{
}

static inline std::string
PrintReceivedPacket (Ptr<Socket> socket, Ptr<Packet> packet, Address senderAddress)
{
  std::ostringstream oss;

  oss << Simulator::Now ().GetSeconds () << " " << socket->GetNode ()->GetId ();

  if (InetSocketAddress::IsMatchingType (senderAddress))
    {
      InetSocketAddress addr = InetSocketAddress::ConvertFrom (senderAddress);
      oss << " received one packet from " << addr.GetIpv4 ();
    }
  else
    {
      oss << " received one packet!";
    }
  return oss.str ();
}

void
RoutingExperiment::ReceivePacket (Ptr<Socket> socket)
{
  Ptr<Packet> packet;
  Address senderAddress;
  while ((packet = socket->RecvFrom (senderAddress)))
    {
      bytesTotal += packet->GetSize ();
      packetsReceived += 1;
      NS_LOG_UNCOND (PrintReceivedPacket (socket, packet, senderAddress));
    }
}

void
RoutingExperiment::CheckThroughput ()
{
  double kbs = (bytesTotal * 8.0) / 1000;
  bytesTotal = 0;

  std::ofstream out (m_CSVfileName.c_str (), std::ios::app);

  out << (Simulator::Now ()).GetSeconds () << ","
      << kbs << ","
      << packetsReceived << ","
      << m_nSinks << ","
      << m_protocolName << ","
      << m_txp << ""
      << std::endl;


  out.close ();
  packetsReceived = 0;
  Simulator::Schedule (Seconds (1.0), &RoutingExperiment::CheckThroughput, this);
}

Ptr<Socket>
RoutingExperiment::SetupPacketReceive (Ipv4Address addr, Ptr<Node> node)
{
  TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
  Ptr<Socket> sink = Socket::CreateSocket (node, tid);
  InetSocketAddress local = InetSocketAddress (addr, port);
  sink->Bind (local);
  sink->SetRecvCallback (MakeCallback (&RoutingExperiment::ReceivePacket, this));

  return sink;
}

std::string
RoutingExperiment::CommandSetup (int argc, char **argv)
{
  CommandLine cmd;
  cmd.AddValue ("CSVfileName", "The name of the CSV output file name", m_CSVfileName);
  cmd.AddValue ("traceMobility", "Enable mobility tracing", m_traceMobility);
  cmd.AddValue ("protocol", "1=OLSR;2=AODV;3=DSDV;4=DSR", m_protocol);
  cmd.Parse (argc, argv);
  return m_CSVfileName;
}

int
main (int argc, char *argv[])
{
  RoutingExperiment experiment;
  std::string CSVfileName = experiment.CommandSetup (argc,argv);

  LogComponentEnable("UdpEchoClientApplication",LOG_LEVEL_INFO);
  	LogComponentEnable("UdpEchoServerApplication",LOG_LEVEL_INFO);

  //blank out the last output file and write the column headers


  int nSinks = 10;
  double txp = 7.5;

  experiment.Run (nSinks, txp, CSVfileName);
}

void
RoutingExperiment::Run (int nSinks, double txp, std::string CSVfileName)
{
  Packet::EnablePrinting ();
  m_nSinks = nSinks;
  m_txp = txp;
  m_CSVfileName = CSVfileName;

  int nWifis = 20;

  double TotalTime = 200.0;
  std::string rate ("2048bps");
  std::string phyMode ("DsssRate11Mbps");
  std::string tr_name ("mywlessex3");
  int nodeSpeed = 1; //in m/s
  int nodePause = 0; //in s
  m_protocolName = "protocol";

  Config::SetDefault  ("ns3::OnOffApplication::PacketSize",StringValue ("64"));
  Config::SetDefault ("ns3::OnOffApplication::DataRate",  StringValue (rate));

  //Set Non-unicastMode rate to unicast mode
  Config::SetDefault ("ns3::WifiRemoteStationManager::NonUnicastMode",StringValue (phyMode));

  NodeContainer adhocNodes;
  adhocNodes.Create (nWifis);

  // setting up wifi phy and channel using helpers
  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy =  YansWifiPhyHelper::Default ();
  YansWifiChannelHelper wifiChannel;
  wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
  wifiChannel.AddPropagationLoss ("ns3::FriisPropagationLossModel");
  wifiPhy.SetChannel (wifiChannel.Create ());

  // Add a mac and disable rate control
  WifiMacHelper wifiMac;
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                "DataMode",StringValue (phyMode),
                                "ControlMode",StringValue (phyMode));

  wifiPhy.Set ("TxPowerStart",DoubleValue (txp));
  wifiPhy.Set ("TxPowerEnd", DoubleValue (txp));

  wifiMac.SetType ("ns3::AdhocWifiMac");
  NetDeviceContainer adhocDevices = wifi.Install (wifiPhy, wifiMac, adhocNodes);

  MobilityHelper mobilityAdhoc;
  int64_t streamIndex = 0; // used to get consistent mobility across scenarios

  ObjectFactory pos;
  pos.SetTypeId ("ns3::RandomDiscPositionAllocator");
 // pos.Set ("X", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=300.0]"));
 // pos.Set ("Y", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=1500.0]"));

  Ptr<PositionAllocator> taPositionAlloc = pos.Create ()->GetObject<PositionAllocator> ();
  streamIndex += taPositionAlloc->AssignStreams (streamIndex);

  std::stringstream ssSpeed;
  ssSpeed << "ns3::UniformRandomVariable[Min=0.0|Max=" << nodeSpeed << "]";
  std::stringstream ssPause;
  ssPause << "ns3::ConstantRandomVariable[Constant=" << nodePause << "]";
  mobilityAdhoc.SetMobilityModel ("ns3::RandomWaypointMobilityModel",
                                  "Speed", StringValue (ssSpeed.str ()),
                                  "Pause", StringValue (ssPause.str ()),
                                  "PositionAllocator", PointerValue (taPositionAlloc));
  mobilityAdhoc.SetPositionAllocator (taPositionAlloc);
  mobilityAdhoc.Install (adhocNodes);
  streamIndex += mobilityAdhoc.AssignStreams (adhocNodes, streamIndex);
  NS_UNUSED (streamIndex); // From this point, streamIndex is unused

  AodvHelper aodv;
  AodvHelper malaodv;
  OlsrHelper olsr;
  DsdvHelper dsdv;
  DsrHelper dsr;
  DsrMainHelper dsrMain;
  Ipv4ListRoutingHelper list;
  InternetStackHelper internet;

  /*switch (m_protocol)
    {
    case 1:
      list.Add (olsr, 100);
      m_protocolName = "OLSR";
      break;
    case 2:
      list.Add (aodv, 100);
      m_protocolName = "AODV";
      break;
    case 3:
      list.Add (dsdv, 100);
      m_protocolName = "DSDV";
      break;
    case 4:
      m_protocolName = "DSR";
      break;
    default:
      NS_FATAL_ERROR ("No such protocol:" << m_protocol);
    }

  if (m_protocol < 4)
    {
      internet.SetRoutingHelper (list);
      internet.Install (adhocNodes);
    }
  else if (m_protocol == 4)
    {
      internet.Install (adhocNodes);
      dsrMain.Install (dsr, adhocNodes);
    }*/

  NS_LOG_INFO ("assigning ip address");


  OnOffHelper onoff1 ("ns3::UdpSocketFactory",Address ());
  onoff1.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1.0]"));
  onoff1.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0.0]"));


 /* uint16_t sinkport=6;
  Address sinkAddress(InetSocketAddress(adhocInterfaces.GetAddress(10),sinkport));
  PacketSinkHelper packetsinkhelper("ns3::UdpSocketFactory",InetSocketAddress(Ipv4Address::GetAny(),sinkport));
 // ApplicationContainer sinkApps;
  ApplicationContainer sinkApps=packetsinkhelper.Install(adhocNodes.Get(10));
  sinkApps.Start(Seconds(1.0));
  sinkApps.Stop(Seconds(100.0));

  Ptr<Socket> ns3UdpSocket =Socket::CreateSocket(adhocNodes.Get(5),UdpSocketFactory::GetTypeId());

  Ptr<MyApp> app =CreateObject<MyApp>();
  app->Setup(ns3UdpSocket,sinkAddress,1024,5,DataRate("250Kbps"));
  adhocNodes.Get(5)->AddApplication(app);
  app->SetStartTime(Seconds(3.0));
  app->SetStopTime(Seconds(100.0));*/

  InternetStackHelper stack;
  stack.SetRoutingHelper(aodv);

  stack.Install(adhocNodes.Get(5));


  stack.Install(adhocNodes.Get(10));
  stack.Install(adhocNodes.Get(14));
     stack.Install(adhocNodes.Get(0));
       stack.Install(adhocNodes.Get(1));
       stack.Install(adhocNodes.Get(2));
         stack.Install(adhocNodes.Get(9));
         stack.Install(adhocNodes.Get(12));
          stack.Install(adhocNodes.Get(13));
          stack.Install(adhocNodes.Get(18));
            stack.Install(adhocNodes.Get(19));



            stack.Install(adhocNodes.Get(4));
            stack.Install(adhocNodes.Get(6));
              stack.Install(adhocNodes.Get(7));
              stack.Install(adhocNodes.Get(8));
              stack.Install(adhocNodes.Get(11));
              stack.Install(adhocNodes.Get(15));
                stack.Install(adhocNodes.Get(16));
                stack.Install(adhocNodes.Get(17));




  malaodv.Set("ismal",BooleanValue(true));
  stack.SetRoutingHelper(malaodv);
  stack.Install(adhocNodes.Get(3));





  Ipv4AddressHelper addressAdhoc;
  addressAdhoc.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer adhocInterfaces;
  adhocInterfaces = addressAdhoc.Assign (adhocDevices);




 // for(int i=0;i<20;i++)
 // {
	//Ptr<Socket> sink=SetupPacketReceive(adhocInterfaces.GetAddress(10),adhocNodes.Get(10));
  //}

  UdpEchoServerHelper es(9);
  ApplicationContainer sa=es.Install(adhocNodes.Get(5));
  sa.Start(Seconds(1.0));
  sa.Stop(Seconds(100.0));

  UdpEchoClientHelper ec(adhocInterfaces.GetAddress(10),9);
  ec.SetAttribute("MaxPackets",UintegerValue(10000));
  ec.SetAttribute("Interval",TimeValue(Seconds(0.1)));
  ec.SetAttribute("PacketSize",UintegerValue(25));

  ApplicationContainer ca=ec.Install(adhocNodes.Get(10));
  ca.Start(Seconds(1.5));
  ca.Stop(Seconds(90.5));

 // Ptr<Node> nd=adhocDevices.Get(5);
  //Ipv4Address ad=adhocInterfaces.GetAddress(10);
 // SetupPacketReceive(ad,nd);






  /*for (int i = 0; i < nSinks; i++)
    {



	  Ptr<Socket> sink = SetupPacketReceive (adhocInterfaces.GetAddress (i), adhocNodes.Get (i));

      AddressValue remoteAddress (InetSocketAddress (adhocInterfaces.GetAddress (i), port));
      onoff1.SetAttribute ("Remote", remoteAddress);

      Ptr<UniformRandomVariable> var = CreateObject<UniformRandomVariable> ();
      ApplicationContainer temp = onoff1.Install (adhocNodes.Get (i + nSinks));
      temp.Start (Seconds (var->GetValue (100.0,101.0)));
      temp.Stop (Seconds (TotalTime));


    }*/

  std::stringstream ss;
  ss << nWifis;
  std::string nodes = ss.str ();

  std::stringstream ss2;
  ss2 << nodeSpeed;
  std::string sNodeSpeed = ss2.str ();

  std::stringstream ss3;
  ss3 << nodePause;
  std::string sNodePause = ss3.str ();

  std::stringstream ss4;
  ss4 << rate;
  std::string sRate = ss4.str ();

  NS_LOG_INFO ("Configure Tracing.");
  tr_name = tr_name + "_" + m_protocolName +"_" + nodes + "nodes_" + sNodeSpeed + "speed_" + sNodePause + "pause_" + sRate + "rate";

 AsciiTraceHelper ascii;
  Ptr<OutputStreamWrapper> osw = ascii.CreateFileStream ( (tr_name + ".tr").c_str());
  wifiPhy.EnableAsciiAll (osw);
  //AsciiTraceHelper ascii;
  //MobilityHelper::EnableAsciiAll (ascii.CreateFileStream (tr_name + ".mob"));

  //Ptr<FlowMonitor> flowmon;
  //FlowMonitorHelper flowmonHelper;
  //flowmon = flowmonHelper.InstallAll ();
  FlowMonitorHelper flowmon;
         	Ptr<FlowMonitor> monitor =flowmon.InstallAll();

    wifiPhy.EnablePcapAll("mywless3");


  NS_LOG_INFO ("Run Simulation.");

 // CheckThroughput ();

  Simulator::Stop (Seconds (TotalTime));
  Simulator::Run ();
  //uint32_t txPacketSum =0;
  //     uint32_t rxPacketSum=0;
  //     uint32_t DropPacketSum=0;
  //     uint32_t LostPacketSum=0;
 //      double DelaySum=0;

   //    monitor->StartRightNow();


       monitor->CheckForLostPackets();



       std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();

       std::ofstream out1 (CSVfileName.c_str ());

              //std::ofstream out (CSVfileName.c_str ());
                out1 <<"FlowId," <<
                		"SourceAddress,"<<
						"DestinationAddress,"<<
                         "TxBytes," <<
                          "RxBytes," <<
                       "Throughput," <<

                       std::endl;



                Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());

       for(std::map<FlowId,FlowMonitor::FlowStats>::const_iterator i = stats.begin();i != stats.end();++i)
       {
    	   Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);

    	   std::cout << "Flow " << i->first  << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";


    	   std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
    	   std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
    	   std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds())/1024/1024  << " Mbps\n";

          out1<<i->first<<","
              <<t.sourceAddress<<","
			  <<t.destinationAddress<<","
        	  <<i->second.txBytes<<","
			  <<i->second.rxBytes<<","
			  <<i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds())/1024/1024
			  <<std::endl;

      	/* out1<<txPacketSum<<","
      	     <<rxPacketSum<<","
			 <<LostPacketSum<<","
			 <<DropPacketSum<<","
			 <<DelaySum<<""
			 <<std::endl;*/



       }

       out1.close();
       //std::cout << "  All Tx Packets: " << txPacketSum << "\n";
       //std::cout << "  All Rx Packets: " << rxPacketSum << "\n";
       //std::cout << "  All Delay: " << DelaySum / txPacketSum <<"\n";
       //std::cout << "  All Lost Packets: " << LostPacketSum << "\n";
       //std::cout << "  All Drop Packets: " << DropPacketSum << "\n";
      // std::cout << "  Packets Delivery Ratio: " << ((rxPacketSum *100) / txPacketSum) << "%" << "\n";
      // std::cout << "  Packets Lost Ratio: " << ((LostPacketSum *100) / txPacketSum) << "%" << "\n";





  monitor->SerializeToXmlFile ((tr_name + ".flowmon").c_str(), false, false);

  Simulator::Destroy ();
}
