/*
* ============================================================================
*
* Copyright [2011] maidsafe.net limited
*
* Description:  MaidSafe Passport Class
* Version:      1.0
* Created:      2010-10-13-14.01.23
* Company:      maidsafe.net limited
*
* The following source code is property of maidsafe.net limited and is not
* meant for external use.  The use of this code is governed by the license
* file LICENSE.TXT found in the root of this directory and also on
* www.maidsafe.net.
*
* You are not free to copy, amend or otherwise use this source code without
* the explicit written permission of the board of directors of maidsafe.net.
*
* ============================================================================
*/

#include "maidsafe/passport/passport.h"

#include <vector>

#include "maidsafe/passport/log.h"
#include "maidsafe/passport/system_packet_handler.h"

namespace maidsafe {

namespace passport {

std::string MidName(const std::string &username,
                    const std::string &pin,
                    bool surrogate) {
  if (surrogate)
    return MidName(username, pin, g_smid_appendix);
  else
    return MidName(username, pin, "");
}

std::string DecryptRid(const std::string &username,
                       const std::string &pin,
                       const std::string &encrypted_rid,
                       bool surrogate) {
  if (username.empty() || pin.empty() || encrypted_rid.empty()) {
    DLOG(ERROR) << "Empty encrypted RID or user data.";
    return "";
  }
  MidPacket mid(username, pin, (surrogate ? g_smid_appendix : ""));
  return mid.DecryptRid(encrypted_rid);
}

std::string PacketDebugString(const int &packet_type) {
  return DebugString(packet_type);
}


Passport::Passport()
    : handler_(new SystemPacketHandler),
      kSmidAppendix_(g_smid_appendix) {}

int Passport::CreateSigningPackets() {
  // kAnmid
  std::vector<pki::SignaturePacketPtr> packets;
  if (pki::CreateChainedId(&packets, 1) != kSuccess || packets.size() != 1U) {
    DLOG(ERROR) << "Failed to create kAnmid";
    return kFailedToCreatePacket;
  }
  packets.at(0)->set_packet_type(kAnmid);
  if (!handler_->AddPendingPacket(packets.at(0))) {
    DLOG(ERROR) << "Failed to add pending kAnmid";
    return kFailedToCreatePacket;
  }

  // kAnsmid
  packets.clear();
  if (pki::CreateChainedId(&packets, 1) != kSuccess || packets.size() != 1U) {
    DLOG(ERROR) << "Failed to create kAnsmid";
    return kFailedToCreatePacket;
  }
  packets.at(0)->set_packet_type(kAnsmid);
  if (!handler_->AddPendingPacket(packets.at(0))) {
    DLOG(ERROR) << "Failed to add pending kAnsmid";
    return kFailedToCreatePacket;
  }

  // kAntmid
  packets.clear();
  if (pki::CreateChainedId(&packets, 1) != kSuccess || packets.size() != 1U) {
    DLOG(ERROR) << "Failed to create kAntmid";
    return kFailedToCreatePacket;
  }
  packets.at(0)->set_packet_type(kAntmid);
  if (!handler_->AddPendingPacket(packets.at(0))) {
    DLOG(ERROR) << "Failed to add pending kAntmid";
    return kFailedToCreatePacket;
  }

  // kAnmaid, kMaid, kPmid
  packets.clear();
  if (pki::CreateChainedId(&packets, 3) != kSuccess || packets.size() != 3U) {
    DLOG(ERROR) << "Failed to create kAntmid";
    return kFailedToCreatePacket;
  }
  packets.at(0)->set_packet_type(kAnmaid);
  packets.at(1)->set_packet_type(kMaid);
  packets.at(2)->set_packet_type(kPmid);
  if (!handler_->AddPendingPacket(packets.at(0)) ||
      !handler_->AddPendingPacket(packets.at(1)) ||
      !handler_->AddPendingPacket(packets.at(2))) {
    DLOG(ERROR) << "Failed to add pending kAnmaid/kMaid/kPmid";
    return kFailedToCreatePacket;
  }

  return kSuccess;
}

int Passport::ConfirmSigningPackets() {
  int result(kSuccess);
  for (int pt(kAnmid); pt != kMid; ++pt) {
    if (handler_->ConfirmPacket(handler_->GetPacket(
          static_cast<PacketType>(pt), false)) != kSuccess) {
      DLOG(ERROR) << "Failed confirming packet " << DebugString(pt);
      result = kFailedToConfirmPacket;
      break;
    }
  }

  return result;
}

int Passport::SetIdentityPackets(const std::string &username,
                                 const std::string &pin,
                                 const std::string &password,
                                 const std::string &master_data,
                                 const std::string &surrogate_data) {
  if (username.empty() || pin.empty() || password.empty() ||
      master_data.empty() || surrogate_data.empty()) {
    DLOG(ERROR) << "At least one empty parameter passed in";
    return kEmptyParameter;
  }

  std::shared_ptr<TmidPacket> tmid(new TmidPacket(username,
                                                  pin,
                                                  false,
                                                  password,
                                                  master_data));
  std::shared_ptr<TmidPacket> stmid(new TmidPacket(username,
                                                   pin,
                                                   true,
                                                   password,
                                                   surrogate_data));

  std::shared_ptr<MidPacket> mid(new MidPacket(username, pin, ""));
  std::shared_ptr<MidPacket> smid(new MidPacket(username, pin, kSmidAppendix_));
  mid->SetRid(tmid->name());
  smid->SetRid(stmid->name());
  BOOST_ASSERT(!mid->name().empty());
  BOOST_ASSERT(!smid->name().empty());
  BOOST_ASSERT(!tmid->name().empty());
  BOOST_ASSERT(!stmid->name().empty());

  if (!handler_->AddPendingPacket(mid) ||
      !handler_->AddPendingPacket(smid) ||
      !handler_->AddPendingPacket(tmid) ||
      !handler_->AddPendingPacket(stmid)) {
    DLOG(ERROR) << "Failed to add pending identity packet";
    return kFailedToCreatePacket;
  }

  return kSuccess;
}

int Passport::ConfirmIdentityPackets() {
  int result(kSuccess);
  for (int pt(kMid); pt != kAnmpid; ++pt) {
    if (handler_->ConfirmPacket(handler_->GetPacket(
          static_cast<PacketType>(pt), false)) != kSuccess) {
      DLOG(ERROR) << "Failed confirming packet " << DebugString(pt);
      result = kFailedToConfirmPacket;
      break;
    }
  }

  return result;
}

std::string Passport::SerialiseKeyring() const {
  return handler_->SerialiseKeyring();
}

int Passport::ParseKeyring(const std::string &serialised_keyring) {
  int result = handler_->ParseKeyring(serialised_keyring);
  if (result != kSuccess) {
    DLOG(ERROR) << "Failed parsing keyring";
    return result;
  }

  result = ConfirmIdentityPackets();
  if (result != kSuccess)
    DLOG(ERROR) << "Failed parsing keyring";

  return result;
}

// Getters
std::string Passport::PacketName(PacketType packet_type, bool confirmed) const {
  PacketPtr packet(handler_->GetPacket(packet_type, confirmed));
  if (!packet) {
    DLOG(ERROR) << "Packet " << DebugString(packet_type) << " in state "
                << std::boolalpha << confirmed << " not found";
    return "";
  }

  return packet->name();
}

std::string Passport::PacketValue(PacketType packet_type,
                                  bool confirmed) const {
  PacketPtr packet(handler_->GetPacket(packet_type, confirmed));
  if (!packet) {
    DLOG(ERROR) << "Packet " << DebugString(packet_type) << " in state "
                << std::boolalpha << confirmed << " not found";
    return "";
  }

  return packet->value();
}

std::string Passport::PacketSignature(PacketType packet_type,
                                      bool confirmed) const {
  PacketPtr packet(handler_->GetPacket(packet_type, confirmed));
  if (!packet) {
    DLOG(ERROR) << "Packet " << DebugString(packet_type) << " in state "
                << std::boolalpha << confirmed << " not found";
    return "";
  }

  if (IsSignature(packet_type, false))
    return std::static_pointer_cast<pki::SignaturePacket>(packet)->signature();

  PacketType signing_packet_type;
  if (packet_type == kMid)
    signing_packet_type = kAnmid;
  else if (packet_type == kSmid)
    signing_packet_type = kAnsmid;
  else if (packet_type == kTmid || packet_type == kStmid)
    signing_packet_type = kAntmid;

  pki::SignaturePacketPtr signing_packet(
      std::static_pointer_cast<pki::SignaturePacket>(
          handler_->GetPacket(signing_packet_type, confirmed)));
  
  if (!signing_packet || signing_packet->private_key().empty()) {
    DLOG(ERROR) << "Packet " << DebugString(packet_type) << " in state "
                << std::boolalpha << confirmed << " doesn't have a signer";
    return "";
  }

  return crypto::AsymSign(packet->value(), signing_packet->private_key());
}

}  // namespace passport

}  // namespace maidsafe
