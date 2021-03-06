/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2014-2018,  Regents of the University of California,
 *                           Arizona Board of Regents,
 *                           Colorado State University,
 *                           University Pierre & Marie Curie, Sorbonne University,
 *                           Washington University in St. Louis,
 *                           Beijing Institute of Technology,
 *                           The University of Memphis.
 *
 * This file is part of NFD (Named Data Networking Forwarding Daemon).
 * See AUTHORS.md for complete list of NFD authors and contributors.
 *
 * NFD is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * NFD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * NFD, e.g., in COPYING.md file.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "internal-transport.hpp"

namespace nfd {
namespace face {

NFD_LOG_MEMBER_INIT(InternalForwarderTransport, InternalForwarderTransport);

InternalForwarderTransport::InternalForwarderTransport(boost::asio::io_service::strand& strand,
                                                       const FaceUri& localUri, const FaceUri& remoteUri,
                                                       ndn::nfd::FaceScope scope, ndn::nfd::LinkType linkType)
  : m_strand(strand)
{
  this->setLocalUri(localUri);
  this->setRemoteUri(remoteUri);
  this->setScope(scope);
  this->setPersistency(ndn::nfd::FACE_PERSISTENCY_PERMANENT);
  this->setLinkType(linkType);
  this->setMtu(MTU_UNLIMITED);

  NFD_LOG_FACE_INFO("Creating transport");
}

void
InternalForwarderTransport::receiveFromLink(const Block& packet)
{
  NFD_LOG_FACE_TRACE(__func__);

  m_strand.dispatch([this, packet] {
    Packet p;
    p.packet = packet;
    this->receive(std::move(p));
  });
}

void
InternalForwarderTransport::doSend(Packet&& packet)
{
  NFD_LOG_FACE_TRACE(__func__);

  this->emitSignal(afterSend, packet.packet);
}

void
InternalForwarderTransport::doClose()
{
  NFD_LOG_FACE_TRACE(__func__);

  this->setState(TransportState::CLOSED);
}

InternalClientTransport::InternalClientTransport(boost::asio::io_service::strand& strand)
  : m_strand(strand)
{
}

void
InternalClientTransport::connectToForwarder(InternalForwarderTransport* forwarderTransport)
{
  m_fwToClientTransmitConn.disconnect();
  m_clientToFwTransmitConn.disconnect();
  m_fwTransportStateConn.disconnect();

  if (forwarderTransport != nullptr) {
    m_fwToClientTransmitConn = forwarderTransport->afterSend.connect([this] (const Block& packet) {
      this->receiveFromLink(packet);
    });
    m_clientToFwTransmitConn = this->afterSend.connect([forwarderTransport] (const Block& packet) {
      forwarderTransport->receiveFromLink(packet);
    });
    m_fwTransportStateConn = forwarderTransport->afterStateChange.connect(
      [this] (TransportState oldState, TransportState newState) {
        if (newState == TransportState::CLOSED) {
          this->connectToForwarder(nullptr);
        }
      });
  }
}

void
InternalClientTransport::receiveFromLink(const Block& packet)
{
  if (m_receiveCallback) {
    m_strand.dispatch([this, packet] { m_receiveCallback(packet); });
  }
}

void
InternalClientTransport::send(const Block& wire)
{
  this->emitSignal(afterSend, wire);
}

void
InternalClientTransport::send(const Block& header, const Block& payload)
{
  ndn::EncodingBuffer encoder(header.size() + payload.size(), header.size() + payload.size());
  encoder.appendByteArray(header.wire(), header.size());
  encoder.appendByteArray(payload.wire(), payload.size());

  this->send(encoder.block());
}

} // namespace face
} // namespace nfd
