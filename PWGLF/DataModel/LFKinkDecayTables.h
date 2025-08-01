// Copyright 2019-2020 CERN and copyright holders of ALICE O2.
// See https://alice-o2.web.cern.ch/copyright for details of the copyright holders.
// All rights not expressly granted are reserved.
//
// This software is distributed under the terms of the GNU General Public
// License v3 (GPL Version 3), copied verbatim in the file "COPYING".
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.

///
/// \file LFKinkDecayTables.h
/// \brief Slim tables for kinks
/// \author Francesco Mazzaschi <francesco.mazzaschi@cern.ch>
///

#include "Common/Core/RecoDecay.h"

#include "Framework/ASoAHelpers.h"
#include "Framework/AnalysisDataModel.h"

#ifndef PWGLF_DATAMODEL_LFKINKDECAYTABLES_H_
#define PWGLF_DATAMODEL_LFKINKDECAYTABLES_H_

namespace o2::aod
{

namespace kinkcand
{

DECLARE_SOA_INDEX_COLUMN_FULL(TrackMoth, trackMoth, int, TracksIU, "_Moth"); //!
DECLARE_SOA_INDEX_COLUMN_FULL(TrackDaug, trackDaug, int, TracksIU, "_Daug"); //!
DECLARE_SOA_INDEX_COLUMN(Collision, collision);                              //!

DECLARE_SOA_COLUMN(XDecVtx, xDecVtx, float);         //! Decay vertex of the candidate (x direction)
DECLARE_SOA_COLUMN(YDecVtx, yDecVtx, float);         //! Decay vertex of the candidate (y direction)
DECLARE_SOA_COLUMN(ZDecVtx, zDecVtx, float);         //! Decay vertex of the candidate (z direction)
DECLARE_SOA_COLUMN(PxMoth, pxMoth, float);           //! Px of the mother kink
DECLARE_SOA_COLUMN(PyMoth, pyMoth, float);           //! Py of the mother kink
DECLARE_SOA_COLUMN(PzMoth, pzMoth, float);           //! Pz of the mother kink
DECLARE_SOA_COLUMN(PxDaug, pxDaug, float);           //! Px of the daughter kink
DECLARE_SOA_COLUMN(PyDaug, pyDaug, float);           //! Py of the daughter kink
DECLARE_SOA_COLUMN(PzDaug, pzDaug, float);           //! Pz of the daughter kink
DECLARE_SOA_COLUMN(MothSign, mothSign, int);         //! Sign of the mother kink
DECLARE_SOA_COLUMN(DcaMothPv, dcaMothPv, float);     //! DCA of the mother to the primary vertex
DECLARE_SOA_COLUMN(DcaDaugPv, dcaDaugPv, float);     //! DCA of the daughter kink to the primary vertex
DECLARE_SOA_COLUMN(DcaKinkTopo, dcaKinkTopo, float); //! DCA of the kink topology

DECLARE_SOA_COLUMN(NSigmaTPCPi, nSigmaTPCPi, float); //! Number of sigmas for the pion candidate from Sigma kink in TPC
DECLARE_SOA_COLUMN(NSigmaTPCPr, nSigmaTPCPr, float); //! Number of sigmas for the proton candidate from Sigma kink in TPC
DECLARE_SOA_COLUMN(NSigmaTPCKa, nSigmaTPCKa, float); //! Number of sigmas for the kaon candidate from Sigma kink in TPC
DECLARE_SOA_COLUMN(NSigmaTOFPi, nSigmaTOFPi, float); //! Number of sigmas for the pion candidate from Sigma kink in TOF
DECLARE_SOA_COLUMN(NSigmaTOFPr, nSigmaTOFPr, float); //! Number of sigmas for the proton candidate from Sigma kink in TOF
DECLARE_SOA_COLUMN(NSigmaTOFKa, nSigmaTOFKa, float); //! Number of sigmas for the kaon candidate from Sigma kink in TOF

// MC Columns
DECLARE_SOA_COLUMN(MothPdgCode, mothPdgCode, int);            //! PDG code of the Sigma daughter
DECLARE_SOA_COLUMN(DaugPdgCode, daugPdgCode, int);            //! PDG code of the kink daughter
DECLARE_SOA_COLUMN(PtMC, ptMC, float);                        //! pT of the candidate in MC
DECLARE_SOA_COLUMN(MassMC, massMC, float);                    //! Invariant mass of the candidate in MC
DECLARE_SOA_COLUMN(DecayRadiusMC, decayRadiusMC, float);      //! Decay radius of the candidate in MC
DECLARE_SOA_COLUMN(CollisionIdCheck, collisionIdCheck, bool); //! Check if mcDaughter collision ID matches the reconstructed collision ID

// DYNAMIC COLUMNS

DECLARE_SOA_DYNAMIC_COLUMN(PxDaugNeut, pxDaugNeut, //! Px of the daughter neutral particle
                           [](float pxmoth, float pxdau) -> float { return pxmoth - pxdau; });

DECLARE_SOA_DYNAMIC_COLUMN(PyDaugNeut, pyDaugNeut, //! Py of the daughter neutral particle
                           [](float pymoth, float pydau) -> float { return pymoth - pydau; });

DECLARE_SOA_DYNAMIC_COLUMN(PzDaugNeut, pzDaugNeut, //! Pz of the daughter neutral particle
                           [](float pzmoth, float pzdau) -> float { return pzmoth - pzdau; });

DECLARE_SOA_DYNAMIC_COLUMN(PtMoth, ptMoth, //! pT of the mother kink
                           [](float pxmoth, float pymoth) -> float { return std::hypot(pxmoth, pymoth); });

DECLARE_SOA_DYNAMIC_COLUMN(PtDaug, ptDaug, //!
                           [](float pxdaug, float pydaug) -> float { return std::hypot(pxdaug, pydaug); });

DECLARE_SOA_DYNAMIC_COLUMN(MSigmaMinus, mSigmaMinus, //! mass under sigma minus hypothesis
                           [](float pxmoth, float pymoth, float pzmoth, float pxch, float pych, float pzch) -> float {
                            float pxneut = pxmoth - pxch;
                            float pyneut = pymoth - pych;
                            float pzneut = pzmoth - pzch;
                            return RecoDecay::m(std::array{std::array{pxch, pych, pzch}, std::array{pxneut, pyneut, pzneut}}, std::array{o2::constants::physics::MassPionCharged, o2::constants::physics::MassNeutron}); });

DECLARE_SOA_DYNAMIC_COLUMN(MSigmaPlus, mSigmaPlus, //! mass under sigma plus hypothesis
                           [](float pxmoth, float pymoth, float pzmoth, float pxch, float pych, float pzch) -> float {
                            float pxneut = pxmoth - pxch;
                            float pyneut = pymoth - pych;
                            float pzneut = pzmoth - pzch;
                            return RecoDecay::m(std::array{std::array{pxch, pych, pzch}, std::array{pxneut, pyneut, pzneut}}, std::array{o2::constants::physics::MassProton, o2::constants::physics::MassPionNeutral}); });

DECLARE_SOA_DYNAMIC_COLUMN(MXiMinus, mXiMinus, //! mass under Xi minus hypothesis
                           [](float pxmoth, float pymoth, float pzmoth, float pxch, float pych, float pzch) -> float {
                            float pxneut = pxmoth - pxch;
                            float pyneut = pymoth - pych;
                            float pzneut = pzmoth - pzch;
                            return RecoDecay::m(std::array{std::array{pxch, pych, pzch}, std::array{pxneut, pyneut, pzneut}}, std::array{o2::constants::physics::MassPionCharged, o2::constants::physics::MassLambda}); });

} // namespace kinkcand

DECLARE_SOA_TABLE(KinkCands, "AOD", "KINKCANDS",
                  o2::soa::Index<>, kinkcand::CollisionId, kinkcand::TrackMothId, kinkcand::TrackDaugId,
                  kinkcand::XDecVtx, kinkcand::YDecVtx, kinkcand::ZDecVtx,
                  kinkcand::MothSign, kinkcand::PxMoth, kinkcand::PyMoth, kinkcand::PzMoth,
                  kinkcand::PxDaug, kinkcand::PyDaug, kinkcand::PzDaug,
                  kinkcand::DcaMothPv, kinkcand::DcaDaugPv, kinkcand::DcaKinkTopo,

                  // dynamic columns
                  kinkcand::PxDaugNeut<kinkcand::PxMoth, kinkcand::PxDaug>,
                  kinkcand::PyDaugNeut<kinkcand::PyMoth, kinkcand::PyDaug>,
                  kinkcand::PzDaugNeut<kinkcand::PzMoth, kinkcand::PzDaug>,
                  kinkcand::PtMoth<kinkcand::PxMoth, kinkcand::PyMoth>,
                  kinkcand::PtDaug<kinkcand::PxDaug, kinkcand::PyDaug>,
                  kinkcand::MSigmaMinus<kinkcand::PxMoth, kinkcand::PyMoth, kinkcand::PzMoth, kinkcand::PxDaug, kinkcand::PyDaug, kinkcand::PzDaug>,
                  kinkcand::MSigmaPlus<kinkcand::PxMoth, kinkcand::PyMoth, kinkcand::PzMoth, kinkcand::PxDaug, kinkcand::PyDaug, kinkcand::PzDaug>,
                  kinkcand::MXiMinus<kinkcand::PxMoth, kinkcand::PyMoth, kinkcand::PzMoth, kinkcand::PxDaug, kinkcand::PyDaug, kinkcand::PzDaug>);

DECLARE_SOA_TABLE(KinkCandsUnbound, "AOD", "UBKINKCANDS",
                  o2::soa::Index<>, kinkcand::XDecVtx, kinkcand::YDecVtx, kinkcand::ZDecVtx,
                  kinkcand::MothSign, kinkcand::PxMoth, kinkcand::PyMoth, kinkcand::PzMoth,
                  kinkcand::PxDaug, kinkcand::PyDaug, kinkcand::PzDaug,
                  kinkcand::DcaMothPv, kinkcand::DcaDaugPv, kinkcand::DcaKinkTopo,

                  // dynamic columns
                  kinkcand::PxDaugNeut<kinkcand::PxMoth, kinkcand::PxDaug>,
                  kinkcand::PyDaugNeut<kinkcand::PyMoth, kinkcand::PyDaug>,
                  kinkcand::PzDaugNeut<kinkcand::PzMoth, kinkcand::PzDaug>,
                  kinkcand::PtMoth<kinkcand::PxMoth, kinkcand::PyMoth>,
                  kinkcand::PtDaug<kinkcand::PxDaug, kinkcand::PyDaug>,
                  kinkcand::MSigmaMinus<kinkcand::PxMoth, kinkcand::PyMoth, kinkcand::PzMoth, kinkcand::PxDaug, kinkcand::PyDaug, kinkcand::PzDaug>,
                  kinkcand::MSigmaPlus<kinkcand::PxMoth, kinkcand::PyMoth, kinkcand::PzMoth, kinkcand::PxDaug, kinkcand::PyDaug, kinkcand::PzDaug>,
                  kinkcand::MXiMinus<kinkcand::PxMoth, kinkcand::PyMoth, kinkcand::PzMoth, kinkcand::PxDaug, kinkcand::PyDaug, kinkcand::PzDaug>);

DECLARE_SOA_TABLE(SlimKinkCands, "AOD", "SLIMKINKCANDS",
                  kinkcand::XDecVtx, kinkcand::YDecVtx, kinkcand::ZDecVtx,
                  kinkcand::PxMoth, kinkcand::PyMoth, kinkcand::PzMoth,
                  kinkcand::PxDaug, kinkcand::PyDaug, kinkcand::PzDaug,
                  kinkcand::DcaMothPv, kinkcand::DcaDaugPv, kinkcand::DcaKinkTopo,
                  kinkcand::MothSign,
                  kinkcand::NSigmaTPCPi, kinkcand::NSigmaTPCPr, kinkcand::NSigmaTPCKa,
                  kinkcand::NSigmaTOFPi, kinkcand::NSigmaTOFPr, kinkcand::NSigmaTOFKa);

DECLARE_SOA_TABLE(SlimKinkCandsMC, "AOD", "SLIMKINKCANDSMC",
                  kinkcand::XDecVtx, kinkcand::YDecVtx, kinkcand::ZDecVtx,
                  kinkcand::PxMoth, kinkcand::PyMoth, kinkcand::PzMoth,
                  kinkcand::PxDaug, kinkcand::PyDaug, kinkcand::PzDaug,
                  kinkcand::DcaMothPv, kinkcand::DcaDaugPv, kinkcand::DcaKinkTopo,
                  kinkcand::MothSign,
                  kinkcand::NSigmaTPCPi, kinkcand::NSigmaTPCPr, kinkcand::NSigmaTPCKa,
                  kinkcand::NSigmaTOFPi, kinkcand::NSigmaTOFPr, kinkcand::NSigmaTOFKa,
                  kinkcand::MothPdgCode, kinkcand::DaugPdgCode,
                  kinkcand::PtMC, kinkcand::MassMC, kinkcand::DecayRadiusMC, kinkcand::CollisionIdCheck);

} // namespace o2::aod

#endif // PWGLF_DATAMODEL_LFKINKDECAYTABLES_H_
