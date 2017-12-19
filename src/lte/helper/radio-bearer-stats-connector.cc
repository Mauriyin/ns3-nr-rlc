/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2012 Centre Tecnologic de Telecomunicacions de Catalunya (CTTC)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Nicola Baldo <nbaldo@cttc.es>
 */


#include <ns3/log.h>


#include "radio-bearer-stats-connector.h"
#include "radio-bearer-stats-calculator.h"
#include <ns3/lte-enb-rrc.h>
#include <ns3/lte-enb-net-device.h>
#include <ns3/lte-ue-rrc.h>
#include <ns3/lte-ue-net-device.h>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("RadioBearerStatsConnector");

/**
  * Less than operator for CellIdRnti, because it is used as key in map
  */
bool
operator < (const RadioBearerStatsConnector::CellIdRnti& a, const RadioBearerStatsConnector::CellIdRnti& b)
{
  return ( (a.cellId < b.cellId) || ( (a.cellId == b.cellId) && (a.rnti < b.rnti) ) );
}

/**
 * This structure is used as interface between trace
 * sources and RadioBearerStatsCalculator. It stores
 * and provides calculators with cellId and IMSI,
 * because most trace sources do not provide it.
 */
struct BoundCallbackArgument : public SimpleRefCount<BoundCallbackArgument>
{
public:
  Ptr<RadioBearerStatsCalculator> stats;  //!< statistics calculator
  uint64_t imsi; //!< imsi
  uint16_t cellId; //!< cellId
};

struct BoundCallbackArgumentRetx : public SimpleRefCount<BoundCallbackArgumentRetx>
{
public:
  Ptr<RetxStatsCalculator> stats;  //!< statistics calculator
  uint64_t imsi; //!< imsi
  uint16_t cellId; //!< cellId
};

struct BoundCallbackArgumentMacTx : public SimpleRefCount<BoundCallbackArgumentMacTx>
{
public:
  Ptr<MacTxStatsCalculator> stats;  //!< statistics calculator
};

/**
 * Callback function for DL TX statistics for both RLC and PDCP
 * /param arg
 * /param path
 * /param rnti
 * /param lcid
 * /param packetSize
 */
void
DlTxPduCallback (Ptr<BoundCallbackArgument> arg, std::string path,
                 uint16_t rnti, uint8_t lcid, uint32_t packetSize)
{
  NS_LOG_FUNCTION (path << rnti << (uint16_t)lcid << packetSize);
  arg->stats->DlTxPdu (arg->cellId, arg->imsi, rnti, lcid, packetSize);
}

/**
 * Callback function for DL RX statistics for both RLC and PDCP
 * /param arg
 * /param path
 * /param rnti
 * /param lcid
 * /param packetSize
 * /param delay
 */
void
DlRxPduCallback (Ptr<BoundCallbackArgument> arg, std::string path,
                 uint16_t rnti, uint8_t lcid, uint32_t packetSize, uint64_t delay)
{
  NS_LOG_FUNCTION (path << rnti << (uint16_t)lcid << packetSize << delay);
  arg->stats->DlRxPdu (arg->cellId, arg->imsi, rnti, lcid, packetSize, delay);
}

/**
 * Callback function for UL TX statistics for both RLC and PDCP
 * /param arg
 * /param path
 * /param rnti
 * /param lcid
 * /param packetSize
 */
void
UlTxPduCallback (Ptr<BoundCallbackArgument> arg, std::string path,
                 uint16_t rnti, uint8_t lcid, uint32_t packetSize)
{
  NS_LOG_FUNCTION (path << rnti << (uint16_t)lcid << packetSize);

  arg->stats->UlTxPdu (arg->cellId, arg->imsi, rnti, lcid, packetSize);
}

/**
 * Callback function for UL RX statistics for both RLC and PDCP
 * /param arg
 * /param path
 * /param rnti
 * /param lcid
 * /param packetSize
 * /param delay
 */
void
UlRxPduCallback (Ptr<BoundCallbackArgument> arg, std::string path,
                 uint16_t rnti, uint8_t lcid, uint32_t packetSize, uint64_t delay)
{
  NS_LOG_FUNCTION (path << rnti << (uint16_t)lcid << packetSize << delay);

  arg->stats->UlRxPdu (arg->cellId, arg->imsi, rnti, lcid, packetSize, delay);
}

void
DlRetxCallback (Ptr<BoundCallbackArgumentRetx> arg, std::string path,
                 uint16_t rnti, uint8_t lcid, uint32_t packetSize, uint32_t numRetx)
{
  NS_LOG_FUNCTION(path << arg->stats << arg->imsi);
  arg->stats->RegisterRetxDl(arg->imsi, arg->cellId, rnti, lcid, packetSize, numRetx);
}

void
UlRetxCallback (Ptr<BoundCallbackArgumentRetx> arg, std::string path,
                 uint16_t rnti, uint8_t lcid, uint32_t packetSize, uint32_t numRetx)
{
  NS_LOG_FUNCTION(path << arg->stats << arg->imsi);
  arg->stats->RegisterRetxUl(arg->imsi, arg->cellId, rnti, lcid, packetSize, numRetx);
}

void
NotifyDlMacTx (Ptr<BoundCallbackArgumentMacTx> arg, std::string path, uint16_t rnti, uint16_t cellId, uint32_t packetSize, uint8_t numRetx)
{
  NS_LOG_FUNCTION(path << rnti << cellId << packetSize << (uint32_t)numRetx);
  arg->stats->RegisterMacTxDl(rnti, cellId, packetSize, numRetx);
}


RadioBearerStatsConnector::RadioBearerStatsConnector ()
  : m_connected (false)
{
  m_retxStats = CreateObject<RetxStatsCalculator> ();
  m_macTxStats = CreateObject<MacTxStatsCalculator> ();
}

void
RadioBearerStatsConnector::EnableRlcStats (Ptr<RadioBearerStatsCalculator> rlcStats)
{
  m_rlcStats = rlcStats;
  EnsureConnected ();
}

void
RadioBearerStatsConnector::EnablePdcpStats (Ptr<RadioBearerStatsCalculator> pdcpStats)
{
  m_pdcpStats = pdcpStats;
  EnsureConnected ();
}

// TypeId
// RadioBearerStatsConnector::GetTypeId (void)
// {
//   static TypeId tid =
//     TypeId ("ns3::RadioBearerStatsConnector")
//     .SetParent<Object> ()
//     .AddConstructor<RadioBearerStatsConnector> ()
//     .SetGroupName("Lte");
//   return tid;
// }

void
RadioBearerStatsConnector::EnsureConnected ()
{
  NS_LOG_FUNCTION (this);
  if (!m_connected)
    {
      Config::Connect ("/NodeList/*/DeviceList/*/LteEnbRrc/NewUeContext",
		       MakeBoundCallback (&RadioBearerStatsConnector::NotifyNewUeContextEnb, this));
      Config::Connect ("/NodeList/*/DeviceList/*/LteUeRrc/RandomAccessSuccessful",
		       MakeBoundCallback (&RadioBearerStatsConnector::NotifyRandomAccessSuccessfulUe, this));
      Config::Connect ("/NodeList/*/DeviceList/*/LteEnbRrc/ConnectionReconfiguration",
		       MakeBoundCallback (&RadioBearerStatsConnector::NotifyConnectionReconfigurationEnb, this));
      Config::Connect ("/NodeList/*/DeviceList/*/LteUeRrc/ConnectionReconfiguration",
		       MakeBoundCallback (&RadioBearerStatsConnector::NotifyConnectionReconfigurationUe, this));
      Config::Connect ("/NodeList/*/DeviceList/*/LteEnbRrc/HandoverStart",
		       MakeBoundCallback (&RadioBearerStatsConnector::NotifyHandoverStartEnb, this));
      Config::Connect ("/NodeList/*/DeviceList/*/LteUeRrc/HandoverStart",
		       MakeBoundCallback (&RadioBearerStatsConnector::NotifyHandoverStartUe, this));
      Config::Connect ("/NodeList/*/DeviceList/*/LteEnbRrc/HandoverEndOk",
		       MakeBoundCallback (&RadioBearerStatsConnector::NotifyHandoverEndOkEnb, this));
      Config::Connect ("/NodeList/*/DeviceList/*/LteUeRrc/HandoverEndOk",
		       MakeBoundCallback (&RadioBearerStatsConnector::NotifyHandoverEndOkUe, this));
      // MAC related callback
      if(m_macTxStats)
      {
        Ptr<BoundCallbackArgumentMacTx> arg = Create<BoundCallbackArgumentMacTx>();
        arg->stats = m_macTxStats;
        Config::Connect ("/NodeList/*/DeviceList/*/MmWaveEnbMac/DlMacTxCallback",
           MakeBoundCallback (&NotifyDlMacTx, arg));
      }
      m_connected = true;
      m_numOfConnectDrbTracesUe = 0;
    }
}

void
RadioBearerStatsConnector::NotifyRandomAccessSuccessfulUe (RadioBearerStatsConnector* c, std::string context, uint64_t imsi, uint16_t cellId, uint16_t rnti)
{
  c->ConnectSrb0Traces (context, imsi, cellId, rnti);
}

void
RadioBearerStatsConnector::NotifyConnectionSetupUe (RadioBearerStatsConnector* c, std::string context, uint64_t imsi, uint16_t cellId, uint16_t rnti)
{
  c->ConnectSrb1TracesUe (context, imsi, cellId, rnti);
}

void
RadioBearerStatsConnector::NotifyConnectionReconfigurationUe (RadioBearerStatsConnector* c, std::string context, uint64_t imsi, uint16_t cellId, uint16_t rnti)
{
  c->ConnectTracesUeIfFirstTime (context, imsi, cellId, rnti);
}

void
RadioBearerStatsConnector::NotifyHandoverStartUe (RadioBearerStatsConnector* c, std::string context, uint64_t imsi, uint16_t cellId, uint16_t rnti, uint16_t targetCellId)
{
  c->DisconnectTracesUe (context, imsi, cellId, rnti);
}

void
RadioBearerStatsConnector::NotifyHandoverEndOkUe (RadioBearerStatsConnector* c, std::string context, uint64_t imsi, uint16_t cellId, uint16_t rnti)
{
  c->ConnectSrb1TracesUe (context, imsi, cellId, rnti);
  c->ConnectDrbTracesUe (context, imsi, cellId, rnti);

}

void
RadioBearerStatsConnector::NotifyNewUeContextEnb (RadioBearerStatsConnector* c, std::string context, uint16_t cellId, uint16_t rnti)
{
  c->StoreUeManagerPath (context, cellId, rnti);
}

void
RadioBearerStatsConnector::NotifyConnectionReconfigurationEnb (RadioBearerStatsConnector* c, std::string context, uint64_t imsi, uint16_t cellId, uint16_t rnti)
{
  c->ConnectTracesEnbIfFirstTime (context, imsi, cellId, rnti);
}

void
RadioBearerStatsConnector::NotifyHandoverStartEnb (RadioBearerStatsConnector* c, std::string context, uint64_t imsi, uint16_t cellId, uint16_t rnti, uint16_t targetCellId)
{
  c->DisconnectTracesEnb (context, imsi, cellId, rnti);
}

void
RadioBearerStatsConnector::NotifyHandoverEndOkEnb (RadioBearerStatsConnector* c, std::string context, uint64_t imsi, uint16_t cellId, uint16_t rnti)
{
  c->ConnectSrb1TracesEnb (context, imsi, cellId, rnti);
  c->ConnectDrbTracesEnb (context, imsi, cellId, rnti);
}

void
RadioBearerStatsConnector::StoreUeManagerPath (std::string context, uint16_t cellId, uint16_t rnti)
{
  NS_LOG_FUNCTION (this << context << cellId << rnti);
  std::ostringstream ueManagerPath;
  ueManagerPath <<  context.substr (0, context.rfind ("/")) << "/UeMap/" << (uint32_t) rnti;
  CellIdRnti key;
  key.cellId = cellId;
  key.rnti = rnti;
  m_ueManagerPathByCellIdRnti[key] = ueManagerPath.str ();
}

void
RadioBearerStatsConnector::ConnectSrb0Traces (std::string context, uint64_t imsi, uint16_t cellId, uint16_t rnti)
{
  NS_LOG_FUNCTION (this << imsi << cellId << rnti);
  std::string ueRrcPath =  context.substr (0, context.rfind ("/"));
  CellIdRnti key;
  key.cellId = cellId;
  key.rnti = rnti;
  std::map<CellIdRnti, std::string>::iterator it = m_ueManagerPathByCellIdRnti.find (key);
  NS_ASSERT (it != m_ueManagerPathByCellIdRnti.end ());
  std::string ueManagerPath = it->second;
  NS_LOG_LOGIC (this << " ueManagerPath: " << ueManagerPath);
  m_ueManagerPathByCellIdRnti.erase (it);

  if (m_rlcStats)
    {
      Ptr<BoundCallbackArgument> arg = Create<BoundCallbackArgument> ();
      arg->imsi = imsi;
      arg->cellId = cellId;
      arg->stats = m_rlcStats;

      // diconnect eventually previously connected SRB0 both at UE and eNB
      Config::Disconnect (ueRrcPath + "/Srb0/LteRlc/TxPDU",
                          MakeBoundCallback (&UlTxPduCallback, arg));
      Config::Disconnect (ueRrcPath + "/Srb0/LteRlc/RxPDU",
                          MakeBoundCallback (&DlRxPduCallback, arg));
      Config::Disconnect (ueManagerPath + "/Srb0/LteRlc/TxPDU",
                          MakeBoundCallback (&DlTxPduCallback, arg));
      Config::Disconnect (ueManagerPath + "/Srb0/LteRlc/RxPDU",
                          MakeBoundCallback (&UlRxPduCallback, arg));

      // connect SRB0 both at UE and eNB
      Config::Connect (ueRrcPath + "/Srb0/LteRlc/TxPDU",
                       MakeBoundCallback (&UlTxPduCallback, arg));
      Config::Connect (ueRrcPath + "/Srb0/LteRlc/RxPDU",
                       MakeBoundCallback (&DlRxPduCallback, arg));
      Config::Connect (ueManagerPath + "/Srb0/LteRlc/TxPDU",
                       MakeBoundCallback (&DlTxPduCallback, arg));
      Config::Connect (ueManagerPath + "/Srb0/LteRlc/RxPDU",
                       MakeBoundCallback (&UlRxPduCallback, arg));

      // connect SRB1 at eNB only (at UE SRB1 will be setup later)
      Config::Connect (ueManagerPath + "/Srb1/LteRlc/TxPDU",
                       MakeBoundCallback (&DlTxPduCallback, arg));
      Config::Connect (ueManagerPath + "/Srb1/LteRlc/RxPDU",
                       MakeBoundCallback (&UlRxPduCallback, arg));
    }
  if (m_pdcpStats)
    {
      Ptr<BoundCallbackArgument> arg = Create<BoundCallbackArgument> ();
      arg->imsi = imsi;
      arg->cellId = cellId;
      arg->stats = m_pdcpStats;

      // connect SRB1 at eNB only (at UE SRB1 will be setup later)
      Config::Connect (ueManagerPath + "/Srb1/LtePdcp/RxPDU",
		       MakeBoundCallback (&UlRxPduCallback, arg));
      Config::Connect (ueManagerPath + "/Srb1/LtePdcp/TxPDU",
		       MakeBoundCallback (&DlTxPduCallback, arg));
    }
}

void
RadioBearerStatsConnector::ConnectTracesUeIfFirstTime (std::string context, uint64_t imsi, uint16_t cellId, uint16_t rnti)
{
  NS_LOG_FUNCTION (this << context);

  //Connect PDCP and RLC traces for SRB1
  if(m_imsiSeenUeSrb.find (imsi) == m_imsiSeenUeSrb.end ())
  {
      m_imsiSeenUeSrb.insert (imsi);
      ConnectSrb1TracesUe (context, imsi, cellId, rnti);
  }

  std::string basePath = context.substr (0, context.rfind ("/"));
  Config::MatchContainer rlc_container = Config::LookupMatches("/NodeList/*/DeviceList/*/LteUeRrc/DataRadioBearerMap/*/LteRlc/");
  NS_LOG_DEBUG("basePath: " + basePath);

  //Connect PDCP and RLC for data radio bearers
  //Look for the RLCs
  if (m_imsiSeenUeDrb.find (imsi) == m_imsiSeenUeDrb.end () && m_numOfConnectDrbTracesUe < rlc_container.GetN() )
    {
      //If it is the first time for this imsi
      NS_LOG_DEBUG("Number of calls " + std::to_string(m_numOfConnectDrbTracesUe) + " Number of RLC " + std::to_string(rlc_container.GetN()));
      NS_LOG_DEBUG("Insert imsi " + std::to_string(imsi));
      m_imsiSeenUeDrb.insert (imsi);
      m_numOfConnectDrbTracesUe ++; //TODO Check if there could be more than one RLC to connect
      ConnectDrbTracesUe (context, imsi, cellId, rnti);
    }
  else
    {
      NS_LOG_DEBUG("Number of calls " + std::to_string(m_numOfConnectDrbTracesUe) + " Number of RLC" + std::to_string(rlc_container.GetN()));
      if(m_numOfConnectDrbTracesUe < rlc_container.GetN())
      {
        //If this imsi has already been connected but a new DRB is established
        NS_LOG_DEBUG("There is a new RLC. Call ConnectDrbTracesUe to connect the traces.");
        m_numOfConnectDrbTracesUe ++; //TODO Check if there could be more than one RLC to connect
        ConnectDrbTracesUe (context, imsi, cellId, rnti);
      }
      else
      {
        m_numOfConnectDrbTracesUe = rlc_container.GetN(); //One or more DRBs could have been removed
        NS_LOG_DEBUG("All RLCs traces are already connected. No need for a call to ConnectDrbTracesUe.");
      }
    }
}

void
RadioBearerStatsConnector::ConnectTracesEnbIfFirstTime (std::string context, uint64_t imsi, uint16_t cellId, uint16_t rnti)
{
  NS_LOG_FUNCTION (this << context);

  //Connect PDCP and RLC traces for SRB1
  if(m_imsiSeenEnbSrb.find (imsi) == m_imsiSeenEnbSrb.end ())
  {
    m_imsiSeenEnbSrb.insert (imsi);
    ConnectSrb1TracesEnb (context, imsi, cellId, rnti);
  }

  //Connect PDCP and RLC for data radio bearers
  //Look for the RLCs
  std::string basePath = context.substr (0, context.rfind ("/")) + "/UeMap/" + std::to_string((uint32_t) rnti);
  Config::MatchContainer rlc_container = Config::LookupMatches(basePath +  "/DataRadioBearerMap/*/LteRlc/");

   if (m_imsiSeenEnbDrb.find (imsi) == m_imsiSeenEnbDrb.end () && rlc_container.GetN() > 0)
    {
      //it is executed only if there exist at least one rlc layer
      m_imsiSeenEnbDrb.insert (imsi);
      ConnectDrbTracesEnb (context, imsi, cellId, rnti);
    }
}

void
RadioBearerStatsConnector::ConnectSrb1TracesUe (std::string ueRrcPath, uint64_t imsi, uint16_t cellId, uint16_t rnti)
{
  NS_LOG_FUNCTION (this << imsi << cellId << rnti);
   if (m_rlcStats)
    {
      Ptr<BoundCallbackArgument> arg = Create<BoundCallbackArgument> ();
      arg->imsi = imsi;
      arg->cellId = cellId;
      arg->stats = m_rlcStats;
      Config::Connect (ueRrcPath + "/Srb1/LteRlc/TxPDU",
                       MakeBoundCallback (&UlTxPduCallback, arg));
      Config::Connect (ueRrcPath + "/Srb1/LteRlc/RxPDU",
                       MakeBoundCallback (&DlRxPduCallback, arg));
    }
  if (m_pdcpStats)
    {
      Ptr<BoundCallbackArgument> arg = Create<BoundCallbackArgument> ();
      arg->imsi = imsi;
      arg->cellId = cellId;
      arg->stats = m_pdcpStats;
      Config::Connect (ueRrcPath + "/Srb1/LtePdcp/RxPDU",
           MakeBoundCallback (&DlRxPduCallback, arg));
      Config::Connect (ueRrcPath + "/Srb1/LtePdcp/TxPDU",
           MakeBoundCallback (&UlTxPduCallback, arg));
    }
}

void
RadioBearerStatsConnector::ConnectDrbTracesUe (std::string context, uint64_t imsi, uint16_t cellId, uint16_t rnti)
{
  NS_LOG_FUNCTION (this << context);
  NS_LOG_LOGIC (this << "expected context should match /NodeList/*/DeviceList/*/LteUeRrc/");
  std::string basePath = context.substr (0, context.rfind ("/"));
  if (m_rlcStats)
    {
      Ptr<BoundCallbackArgument> arg = Create<BoundCallbackArgument> ();
      arg->imsi = imsi;
      arg->cellId = cellId;
      arg->stats = m_rlcStats;
      Config::Connect (basePath + "/DataRadioBearerMap/*/LteRlc/TxPDU",
		       MakeBoundCallback (&UlTxPduCallback, arg));
      Config::Connect (basePath + "/DataRadioBearerMap/*/LteRlc/RxPDU",
		       MakeBoundCallback (&DlRxPduCallback, arg));

    }
  if (m_pdcpStats)
    {
      Ptr<BoundCallbackArgument> arg = Create<BoundCallbackArgument> ();
      arg->imsi = imsi;
      arg->cellId = cellId;
      arg->stats = m_pdcpStats;
      Config::Connect (basePath + "/DataRadioBearerMap/*/LtePdcp/RxPDU",
		       MakeBoundCallback (&DlRxPduCallback, arg));
      Config::Connect (basePath + "/DataRadioBearerMap/*/LtePdcp/TxPDU",
		       MakeBoundCallback (&UlTxPduCallback, arg));
    }
  if (m_retxStats) // TODO set condition
    {
      Ptr<BoundCallbackArgumentRetx> arg = Create<BoundCallbackArgumentRetx> ();
      arg->imsi = imsi;
      arg->cellId = cellId;
      arg->stats = m_retxStats;
      Config::Connect (basePath + "/DataRadioBearerMap/*/LteRlc/TxCompletedCallback",
         MakeBoundCallback (&UlRetxCallback, arg));
    }
}

void
RadioBearerStatsConnector::ConnectSrb1TracesEnb (std::string context, uint64_t imsi, uint16_t cellId, uint16_t rnti)
{
  NS_LOG_FUNCTION (this << context);
  NS_LOG_LOGIC (this << "expected context  should match /NodeList/*/DeviceList/*/LteEnbRrc/");
  std::ostringstream basePath;
  basePath <<  context.substr (0, context.rfind ("/")) << "/UeMap/" << (uint32_t) rnti;
  if (m_rlcStats)
    {
      Ptr<BoundCallbackArgument> arg = Create<BoundCallbackArgument> ();
      arg->imsi = imsi;
      arg->cellId = cellId;
      arg->stats = m_rlcStats;
      Config::Connect (basePath.str () + "/Srb0/LteRlc/RxPDU",
		       MakeBoundCallback (&UlRxPduCallback, arg));
      Config::Connect (basePath.str () + "/Srb0/LteRlc/TxPDU",
		       MakeBoundCallback (&DlTxPduCallback, arg));
      Config::Connect (basePath.str () + "/Srb1/LteRlc/RxPDU",
		       MakeBoundCallback (&UlRxPduCallback, arg));
      Config::Connect (basePath.str () + "/Srb1/LteRlc/TxPDU",
		       MakeBoundCallback (&DlTxPduCallback, arg));
    }
  if (m_pdcpStats)
    {
      Ptr<BoundCallbackArgument> arg = Create<BoundCallbackArgument> ();
      arg->imsi = imsi;
      arg->cellId = cellId;
      arg->stats = m_pdcpStats;
      Config::Connect (basePath.str () + "/Srb1/LtePdcp/TxPDU",
		       MakeBoundCallback (&DlTxPduCallback, arg));
      Config::Connect (basePath.str () + "/Srb1/LtePdcp/RxPDU",
		       MakeBoundCallback (&UlRxPduCallback, arg));
    }
}

void
RadioBearerStatsConnector::ConnectDrbTracesEnb (std::string context, uint64_t imsi, uint16_t cellId, uint16_t rnti)
{
  NS_LOG_FUNCTION (this << context);
  NS_LOG_LOGIC (this << "expected context  should match /NodeList/*/DeviceList/*/LteEnbRrc/");
  std::ostringstream basePath;
  basePath <<  context.substr (0, context.rfind ("/")) << "/UeMap/" << (uint32_t) rnti;
  if (m_rlcStats)
    {
      Ptr<BoundCallbackArgument> arg = Create<BoundCallbackArgument> ();
      arg->imsi = imsi;
      arg->cellId = cellId;
      arg->stats = m_rlcStats;
      Config::Connect (basePath.str () + "/DataRadioBearerMap/*/LteRlc/RxPDU",
		       MakeBoundCallback (&UlRxPduCallback, arg));
      Config::Connect (basePath.str () + "/DataRadioBearerMap/*/LteRlc/TxPDU",
		       MakeBoundCallback (&DlTxPduCallback, arg));
    }
  if (m_pdcpStats)
    {
      Ptr<BoundCallbackArgument> arg = Create<BoundCallbackArgument> ();
      arg->imsi = imsi;
      arg->cellId = cellId;
      arg->stats = m_pdcpStats;
      Config::Connect (basePath.str () + "/DataRadioBearerMap/*/LtePdcp/TxPDU",
		       MakeBoundCallback (&DlTxPduCallback, arg));
      Config::Connect (basePath.str () + "/DataRadioBearerMap/*/LtePdcp/RxPDU",
		       MakeBoundCallback (&UlRxPduCallback, arg));
    }
  if (m_retxStats)
    {
      Ptr<BoundCallbackArgumentRetx> arg = Create<BoundCallbackArgumentRetx> ();
      arg->imsi = imsi;
      arg->cellId = cellId;
      arg->stats = m_retxStats;
      Config::Connect (basePath.str () + "/DataRadioBearerMap/*/LteRlc/TxCompletedCallback",
         MakeBoundCallback (&DlRetxCallback, arg));
    }
}


void
RadioBearerStatsConnector::DisconnectTracesUe (std::string context, uint64_t imsi, uint16_t cellId, uint16_t rnti)
{
  NS_LOG_FUNCTION (this);
}


void
RadioBearerStatsConnector::DisconnectTracesEnb (std::string context, uint64_t imsi, uint16_t cellId, uint16_t rnti)
{
  NS_LOG_FUNCTION (this);
}



} // namespace ns3
