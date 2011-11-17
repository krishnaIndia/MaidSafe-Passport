/*
* ============================================================================
*
* Copyright [2010] maidsafe.net limited
*
* Description:  Unit tests for Passport class
* Version:      1.0
* Created:      2010-10-19-23.59.27
* Revision:     none
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

#include <cstdint>

#include "maidsafe/common/test.h"
#include "maidsafe/common/utils.h"

#include "maidsafe/pki/packet.h"

#include "maidsafe/passport/log.h"
#include "maidsafe/passport/passport.h"
#include "maidsafe/passport/system_packet_handler.h"

namespace maidsafe {

namespace passport {

namespace test {

class PassportTest : public testing::Test {
 public:
  PassportTest()
      : passport_(),
        username_(RandomAlphaNumericString(6)),
        pin_("1111"),
        password_(RandomAlphaNumericString(8)),
        master_data_(RandomString(1000)),
        surrogate_data_(RandomString(1000)),
        appendix_(passport_.kSmidAppendix_) {}

 protected:
  typedef std::shared_ptr<MidPacket> MidPacketPtr;
  typedef std::shared_ptr<TmidPacket> TmidPacketPtr;
  bool VerifySignatures() {
    for (int pt(kAnmid); pt != kMid; ++pt) {
      PacketType casted(static_cast<PacketType>(pt)), signer;
      if (casted == kMaid)
        signer = kAnmaid;
      else if (casted == kPmid)
        signer = kMaid;
      else
        signer = casted;
      pki::SignaturePacketPtr signing_packet(
            std::static_pointer_cast<pki::SignaturePacket>(
                passport_.handler_->GetPacket(signer, true)));
      if (!signing_packet) {
        DLOG(ERROR) << "1. Packet: " << DebugString(casted) << ", Signer: "
                    << DebugString(signer);
        return false;
      }

      if (signing_packet->private_key().empty()) {
        DLOG(ERROR) << "1.5. Packet: " << DebugString(casted) << ", Signer: "
                    << DebugString(signer);
        return false;
      }

      if (!crypto::AsymCheckSig(passport_.PacketValue(casted, true),
                                passport_.PacketSignature(casted, true),
                                signing_packet->value())) {
        DLOG(ERROR) << "2. Packet: " << DebugString(casted) << ", Signer: "
                    << DebugString(signer);
        return false;
      }
    }
    return true;
  }

  bool VerifyIdentityContents() {
    MidPacketPtr mid(std::static_pointer_cast<MidPacket>(
                         passport_.handler_->GetPacket(kMid, true)));
    if (passport_.PacketName(kTmid, true) !=
        mid->DecryptRid(mid->value())) {
      DLOG(ERROR) << "kMid doesn't contain pointer to kTmid";
      return false;
    }

    MidPacketPtr smid(std::static_pointer_cast<MidPacket>(
                          passport_.handler_->GetPacket(kSmid, true)));
    if (passport_.PacketName(kStmid, true) !=
        smid->DecryptRid(smid->value())) {
      DLOG(ERROR) << "kSmid doesn't contain pointer to kTmid";
      return false;
    }
    return true;
  }

  bool VerifySaveSession() {
    MidPacketPtr c_mid(std::static_pointer_cast<MidPacket>(
                           passport_.handler_->GetPacket(kMid, true)));
    MidPacketPtr p_mid(std::static_pointer_cast<MidPacket>(
                           passport_.handler_->GetPacket(kMid, false)));
    if (c_mid->name() != p_mid->name()) {
      DLOG(ERROR) << "kMid names not the same";
      return false;
    }

    MidPacketPtr c_smid(std::static_pointer_cast<MidPacket>(
                            passport_.handler_->GetPacket(kSmid, true)));
    MidPacketPtr p_smid(std::static_pointer_cast<MidPacket>(
                            passport_.handler_->GetPacket(kSmid, false)));
    if (c_smid->name() != p_smid->name()) {
      DLOG(ERROR) << "kSmid names not the same";
      return false;
    }

    if (passport_.PacketName(kTmid, true) !=
            passport_.PacketName(kStmid, false) ||
        passport_.PacketValue(kTmid, true) !=
            passport_.PacketValue(kStmid, false)) {
      DLOG(ERROR) << "Pending kStmid doesn't match confirmed kTmid";
      return false;
    }

    if (passport_.PacketName(kTmid, true) ==
            passport_.PacketName(kTmid, false) ||
        passport_.PacketName(kStmid, true) ==
            passport_.PacketName(kStmid, false)) {
      DLOG(ERROR) << "Pending kStmid doesn't match confirmed kTmid";
      return false;
    }

    return true;
  }

  bool VerifyChangeDetails(const std::string &new_username,
                           const std::string &new_pin) {
    MidPacketPtr c_mid(std::static_pointer_cast<MidPacket>(
                           passport_.handler_->GetPacket(kMid, true)));
    MidPacketPtr p_mid(std::static_pointer_cast<MidPacket>(
                           passport_.handler_->GetPacket(kMid, false)));
    if (c_mid->name() == p_mid->name()) {
      DLOG(ERROR) << "kMid names the same";
      return false;
    }
    if (crypto::Hash<crypto::SHA512>(new_username + new_pin) != p_mid->name()) {
      DLOG(ERROR) << "kMid name incorrect";
      return false;
    }

    MidPacketPtr c_smid(std::static_pointer_cast<MidPacket>(
                            passport_.handler_->GetPacket(kSmid, true)));
    MidPacketPtr p_smid(std::static_pointer_cast<MidPacket>(
                            passport_.handler_->GetPacket(kSmid, false)));
    if (c_smid->name() == p_smid->name()) {
      DLOG(ERROR) << "kSmid names the same";
      return false;
    }
    if (crypto::Hash<crypto::SHA512>(new_username + new_pin + appendix_) !=
        p_smid->name()) {
      DLOG(ERROR) << "kSmid name incorrect";
      return false;
    }

    TmidPacketPtr c_tmid(std::static_pointer_cast<TmidPacket>(
                             passport_.handler_->GetPacket(kTmid, true)));
    TmidPacketPtr p_stmid(std::static_pointer_cast<TmidPacket>(
                              passport_.handler_->GetPacket(kStmid, false)));
    if (p_stmid->DecryptPlainData(password_,
                                  passport_.PacketValue(kStmid, false)) !=
        c_tmid->DecryptPlainData(password_,
                                 passport_.PacketValue(kTmid, true))) {
      DLOG(ERROR) << "New kStmid plain value is not old kTmid plain value";
      return false;
    }

    return true;
  }

  bool VerifyChangePassword(const std::string &new_password) {
    MidPacketPtr c_mid(std::static_pointer_cast<MidPacket>(
                           passport_.handler_->GetPacket(kMid, true)));
    MidPacketPtr p_mid(std::static_pointer_cast<MidPacket>(
                           passport_.handler_->GetPacket(kMid, false)));
    if (c_mid->name() != p_mid->name()) {
      DLOG(ERROR) << "kMid names not the same";
      return false;
    }

    MidPacketPtr c_smid(std::static_pointer_cast<MidPacket>(
                            passport_.handler_->GetPacket(kSmid, true)));
    MidPacketPtr p_smid(std::static_pointer_cast<MidPacket>(
                            passport_.handler_->GetPacket(kSmid, false)));
    if (c_smid->name() != p_smid->name()) {
      DLOG(ERROR) << "kSmid names not the same";
      return false;
    }

    TmidPacketPtr c_tmid(std::static_pointer_cast<TmidPacket>(
                             passport_.handler_->GetPacket(kTmid, true)));
    TmidPacketPtr p_stmid(std::static_pointer_cast<TmidPacket>(
                              passport_.handler_->GetPacket(kStmid, false)));
    if (p_stmid->DecryptPlainData(new_password,
                                  passport_.PacketValue(kStmid, false)) !=
        c_tmid->DecryptPlainData(password_,
                                 passport_.PacketValue(kTmid, true))) {
      DLOG(ERROR) << "New kStmid plain value is not old kTmid plain value";
      return false;
    }

    return true;
  }

  Passport passport_;
  std::string username_, pin_, password_, master_data_, surrogate_data_,
              appendix_;
};

TEST_F(PassportTest, BEH_SigningPackets) {
  ASSERT_EQ(kSuccess, passport_.CreateSigningPackets());

  // Check hashability of signature packets
  for (int pt(kAnmid); pt != kMid; ++pt) {
    PacketType casted(static_cast<PacketType>(pt));
    ASSERT_EQ(passport_.PacketName(casted, false),
              crypto::Hash<crypto::SHA512>(
                  passport_.PacketValue(casted, false) +
                  passport_.PacketSignature(casted, false)));
    ASSERT_TRUE(passport_.PacketName(casted, true).empty());
  }

  // Confirm and check
  ASSERT_EQ(kSuccess, passport_.ConfirmSigningPackets());
  for (int pt(kAnmid); pt != kMid; ++pt) {
    PacketType casted(static_cast<PacketType>(pt));
    ASSERT_TRUE(passport_.PacketName(casted, false).empty());
    ASSERT_EQ(passport_.PacketName(casted, true),
              crypto::Hash<crypto::SHA512>(
                  passport_.PacketValue(casted, true) +
                  passport_.PacketSignature(casted, true)));
  }

  // Verify the signatures
  ASSERT_TRUE(VerifySignatures());
}

TEST_F(PassportTest, BEH_IdentityPackets) {
  ASSERT_EQ(kSuccess, passport_.CreateSigningPackets());
  ASSERT_EQ(kSuccess, passport_.ConfirmSigningPackets());

  ASSERT_EQ(kSuccess, passport_.SetIdentityPackets(username_,
                                                      pin_,
                                                      password_,
                                                      master_data_,
                                                      surrogate_data_));

  // Check pending packets
  for (int pt(kMid); pt != kAnmpid; ++pt) {
    PacketType casted(static_cast<PacketType>(pt));
    ASSERT_EQ("", passport_.PacketName(casted, true));
    ASSERT_NE("", passport_.PacketName(casted, false));
  }

  // Check confirmed packets
  ASSERT_EQ(kSuccess, passport_.ConfirmIdentityPackets());
  for (int pt(kMid); pt != kAnmpid; ++pt) {
    PacketType casted(static_cast<PacketType>(pt));
    ASSERT_NE("", passport_.PacketName(casted, true));
  }

  ASSERT_EQ(passport_.PacketName(kMid, true),
            crypto::Hash<crypto::SHA512>(username_ + pin_));
  ASSERT_EQ(passport_.PacketName(kSmid, true),
            crypto::Hash<crypto::SHA512>(username_ + pin_ + appendix_));
  ASSERT_EQ(passport_.PacketName(kTmid, true),
            crypto::Hash<crypto::SHA512>(passport_.PacketValue(kTmid, true)));
  ASSERT_EQ(passport_.PacketName(kStmid, true),
            crypto::Hash<crypto::SHA512>(passport_.PacketValue(kStmid, true)));
  // Verify value of kMid & kSmid
  ASSERT_TRUE(VerifyIdentityContents());
}

TEST_F(PassportTest, BEH_ChangingIdentityPackets) {
  ASSERT_EQ(kSuccess, passport_.CreateSigningPackets());
  ASSERT_EQ(kSuccess, passport_.ConfirmSigningPackets());
  ASSERT_EQ(kSuccess, passport_.SetIdentityPackets(username_,
                                                      pin_,
                                                      password_,
                                                      master_data_,
                                                      surrogate_data_));
  ASSERT_EQ(kSuccess, passport_.ConfirmIdentityPackets());

  // Save session
  std::string next_surrogate1(RandomString(1000));
  ASSERT_EQ(kSuccess, passport_.SetIdentityPackets(username_,
                                                      pin_,
                                                      password_,
                                                      next_surrogate1,
                                                      master_data_));
  ASSERT_TRUE(VerifySaveSession());
  ASSERT_EQ(kSuccess, passport_.ConfirmIdentityPackets());

  // Changing details
  std::string new_username(RandomAlphaNumericString(6)), new_pin("2222"),
              next_surrogate2(RandomString(1000));
  ASSERT_EQ(kSuccess, passport_.SetIdentityPackets(new_username,
                                                      new_pin,
                                                      password_,
                                                      next_surrogate2,
                                                      next_surrogate1));
  ASSERT_TRUE(VerifyChangeDetails(new_username, new_pin));
  ASSERT_EQ(kSuccess, passport_.ConfirmIdentityPackets());

  // Changing password
  std::string next_surrogate3(RandomString(1000)),
              new_password(RandomAlphaNumericString(8));
  ASSERT_EQ(kSuccess, passport_.SetIdentityPackets(new_username,
                                                      new_pin,
                                                      new_password,
                                                      next_surrogate3,
                                                      next_surrogate2));
  ASSERT_TRUE(VerifyChangePassword(new_password));
  ASSERT_EQ(kSuccess, passport_.ConfirmIdentityPackets());
}

}  // namespace test

}  // namespace passport

}  // namespace maidsafe
