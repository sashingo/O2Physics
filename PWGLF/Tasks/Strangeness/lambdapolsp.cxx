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
// Lambda polarisation task
// prottay.das@cern.ch

#include "PWGLF/DataModel/LFStrangenessPIDTables.h"
#include "PWGLF/DataModel/LFStrangenessTables.h"
#include "PWGLF/DataModel/SPCalibrationTables.h"

#include "Common/Core/RecoDecay.h"
#include "Common/Core/TrackSelection.h"
#include "Common/Core/trackUtilities.h"
#include "Common/DataModel/Centrality.h"
#include "Common/DataModel/EventSelection.h"
#include "Common/DataModel/FT0Corrected.h"
#include "Common/DataModel/Multiplicity.h"
#include "Common/DataModel/PIDResponse.h"
#include "Common/DataModel/TrackSelectionTables.h"

#include "CCDB/BasicCCDBManager.h"
#include "CommonConstants/PhysicsConstants.h"
#include "DataFormatsParameters/GRPMagField.h"
#include "DataFormatsParameters/GRPObject.h"
#include "Framework/ASoAHelpers.h"
#include "Framework/AnalysisDataModel.h"
#include "Framework/AnalysisTask.h"
#include "Framework/HistogramRegistry.h"
#include "Framework/O2DatabasePDGPlugin.h"
#include "Framework/StepTHn.h"
#include "Framework/runDataProcessing.h"
#include "ReconstructionDataFormats/Track.h"

#include "Math/GenVector/Boost.h"
#include "Math/Vector3D.h"
#include "Math/Vector4D.h"
#include "TF1.h"
#include "TRandom3.h"
#include <TDirectory.h>
#include <TFile.h>
#include <TH1F.h>
#include <TH2F.h>
#include <THn.h>
#include <TLorentzVector.h>
#include <TMath.h>
#include <TObjArray.h>
#include <TPDGCode.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <deque>
#include <iostream>
#include <iterator>
#include <set> // <<< CHANGED: for dedup sets
#include <string>
#include <type_traits>
#include <unordered_map> // <<< CHANGED: for seenMap
#include <utility>
#include <vector>

using namespace o2;
using namespace o2::framework;
using namespace o2::framework::expressions;
using std::array;
using namespace o2::aod::rctsel;

using dauTracks = soa::Join<aod::DauTrackExtras, aod::DauTrackTPCPIDs>;
using v0Candidates = soa::Join<aod::V0CollRefs, aod::V0Cores, aod::V0Extras>;

struct lambdapolsp {

  int mRunNumber;
  Service<o2::ccdb::BasicCCDBManager> ccdb;
  Service<o2::framework::O2DatabasePDG> pdg;

  // fill output
  Configurable<bool> additionalEvSel{"additionalEvSel", false, "additionalEvSel"};
  Configurable<bool> additionalEvSel2{"additionalEvSel2", false, "additionalEvSel2"};
  Configurable<bool> additionalEvSel3{"additionalEvSel3", false, "additionalEvSel3"};
  Configurable<bool> additionalEvSel4{"additionalEvSel4", false, "additionalEvSel4"};
  Configurable<bool> globalpt{"globalpt", true, "select tracks based on pt global vs tpc"};
  Configurable<bool> cqvas{"cqvas", false, "change q vectors after shift correction"};
  Configurable<int> useprofile{"useprofile", 3, "flag to select profile vs Sparse"};
  Configurable<int> cfgMaxOccupancy{"cfgMaxOccupancy", 1000, "maximum occupancy of tracks in neighbouring collisions in a given time range"};
  Configurable<int> cfgMinOccupancy{"cfgMinOccupancy", 0, "maximum occupancy of tracks in neighbouring collisions in a given time range"};
  Configurable<int> sys{"sys", 1, "flag to select systematic source"};
  Configurable<bool> dosystematic{"dosystematic", false, "flag to perform systematic study"};
  Configurable<bool> needetaaxis{"needetaaxis", false, "flag to use last axis"};
  struct : ConfigurableGroup {
    Configurable<bool> doRandomPsi{"doRandomPsi", true, "randomize psi"};
    Configurable<bool> doRandomPsiAC{"doRandomPsiAC", true, "randomize psiAC"};
    Configurable<bool> doRandomPhi{"doRandomPhi", true, "randomize phi"};
    Configurable<double> etaMix{"etaMix", 0.1, "eta difference in mixing"};
    Configurable<double> ptMix{"ptMix", 0.1, "pt difference in mixing"};
    Configurable<double> phiMix{"phiMix", 0.1, "phi difference in mixing"};
  } randGrp;
  // events
  Configurable<float> cfgCutVertex{"cfgCutVertex", 10.0f, "Accepted z-vertex range"};
  Configurable<float> cfgCutCentralityMax{"cfgCutCentralityMax", 50.0f, "Accepted maximum Centrality"};
  Configurable<float> cfgCutCentralityMin{"cfgCutCentralityMin", 30.0f, "Accepted minimum Centrality"};
  // proton track cut
  Configurable<float> cfgCutPT{"cfgCutPT", 0.15, "PT cut on daughter track"};
  Configurable<float> cfgCutEta{"cfgCutEta", 0.8, "Eta cut on daughter track"};
  Configurable<float> cfgCutDCAxy{"cfgCutDCAxy", 0.1f, "DCAxy range for tracks"};
  Configurable<float> cfgCutDCAz{"cfgCutDCAz", 0.1f, "DCAz range for tracks"};
  Configurable<int> cfgITScluster{"cfgITScluster", 5, "Number of ITS cluster"};
  Configurable<int> cfgTPCcluster{"cfgTPCcluster", 70, "Number of TPC cluster"};
  Configurable<bool> isPVContributor{"isPVContributor", true, "is PV contributor"};
  Configurable<bool> checkwithpub{"checkwithpub", true, "checking results with published"};

  // Configs for V0
  Configurable<float> ConfV0PtMin{"ConfV0PtMin", 0.f, "Minimum transverse momentum of V0"};
  Configurable<float> ConfV0Rap{"ConfV0Rap", 0.8f, "Rapidity range of V0"};
  Configurable<double> ConfV0DCADaughMax{"ConfV0DCADaughMax", 0.2f, "Maximum DCA between the V0 daughters"};
  Configurable<double> ConfV0CPAMin{"ConfV0CPAMin", 0.9998f, "Minimum CPA of V0"};
  Configurable<float> ConfV0TranRadV0Min{"ConfV0TranRadV0Min", 1.5f, "Minimum transverse radius"};
  Configurable<float> ConfV0TranRadV0Max{"ConfV0TranRadV0Max", 100.f, "Maximum transverse radius"};
  Configurable<double> cMaxV0DCA{"cMaxV0DCA", 1.2, "Maximum V0 DCA to PV"};
  Configurable<double> cMinV0DCAPr{"cMinV0DCAPr", 0.05, "Minimum V0 daughters DCA to PV for Pr"};
  Configurable<double> cMinV0DCAPi{"cMinV0DCAPi", 0.05, "Minimum V0 daughters DCA to PV for Pi"};
  Configurable<float> cMaxV0LifeTime{"cMaxV0LifeTime", 20, "Maximum V0 life time"};
  Configurable<bool> analyzeLambda{"analyzeLambda", true, "flag for lambda analysis"};
  Configurable<bool> analyzeK0s{"analyzeK0s", false, "flag for K0s analysis"};
  Configurable<float> qtArmenterosMinForK0{"qtArmenterosMinForK0", 0.2, "Armenterous cut for K0s"};

  // config for V0 daughters
  Configurable<float> ConfDaughEta{"ConfDaughEta", 0.8f, "V0 Daugh sel: max eta"};
  Configurable<float> cfgDaughPrPt{"cfgDaughPrPt", 0.4, "minimum daughter proton pt"};
  Configurable<float> cfgDaughPiPt{"cfgDaughPiPt", 0.2, "minimum daughter pion pt"};
  Configurable<float> rcrfc{"rcrfc", 0.8f, "Ratio of CR to FC"};
  Configurable<float> ConfDaughTPCnclsMin{"ConfDaughTPCnclsMin", 50.f, "V0 Daugh sel: Min. nCls TPC"};
  Configurable<float> ConfDaughPIDCuts{"ConfDaughPIDCuts", 3, "PID selections for Lambda daughters"};
  Configurable<bool> usesubdet{"usesubdet", false, "use subdet"};
  Configurable<bool> useAccCorr{"useAccCorr", false, "use acceptance correction"};
  Configurable<std::string> ConfAccPathL{"ConfAccPathL", "Users/p/prottay/My/Object/From379780/Fulldata/NewPbPbpass4_28032025/acccorrL", "Path to acceptance correction for Lambda"};
  Configurable<std::string> ConfAccPathAL{"ConfAccPathAL", "Users/p/prottay/My/Object/From379780/Fulldata/NewPbPbpass4_28032025/acccorrAL", "Path to acceptance correction for AntiLambda"};

  struct : ConfigurableGroup {
    Configurable<int> QxyNbins{"QxyNbins", 100, "Number of bins in QxQy histograms"};
    Configurable<float> lbinQxy{"lbinQxy", -5.0, "lower bin value in QxQy histograms"};
    Configurable<float> hbinQxy{"hbinQxy", 5.0, "higher bin value in QxQy histograms"};
    Configurable<int> PolNbins{"PolNbins", 20, "Number of bins in polarisation"};
    Configurable<float> lbinPol{"lbinPol", -1.0, "lower bin value in #phi-#psi histograms"};
    Configurable<float> hbinPol{"hbinPol", 1.0, "higher bin value in #phi-#psi histograms"};
    Configurable<int> IMNbins{"IMNbins", 100, "Number of bins in invariant mass"};
    Configurable<float> lbinIM{"lbinIM", 1.0, "lower bin value in IM histograms"};
    Configurable<float> hbinIM{"hbinIM", 1.2, "higher bin value in IM histograms"};
    Configurable<int> resNbins{"resNbins", 50, "Number of bins in reso"};
    Configurable<float> lbinres{"lbinres", 0.0, "lower bin value in reso histograms"};
    Configurable<float> hbinres{"hbinres", 10.0, "higher bin value in reso histograms"};
    Configurable<int> spNbins{"spNbins", 2000, "Number of bins in sp"};
    Configurable<float> lbinsp{"lbinsp", -1.0, "lower bin value in sp histograms"};
    Configurable<float> hbinsp{"hbinsp", 1.0, "higher bin value in sp histograms"};
    // Configurable<int> CentNbins{"CentNbins", 16, "Number of bins in cent histograms"};
    // Configurable<float> lbinCent{"lbinCent", 0.0, "lower bin value in cent histograms"};
    // Configurable<float> hbinCent{"hbinCent", 80.0, "higher bin value in cent histograms"};
  } binGrp;
  /*
  ConfigurableAxis configcentAxis{"configcentAxis", {VARIABLE_WIDTH, 0.0, 10.0, 40.0, 80.0}, "Cent V0M"};
  ConfigurableAxis configthnAxispT{"configthnAxisPt", {VARIABLE_WIDTH, 0.2, 0.5, 1.0, 1.5, 2.0, 2.5, 3.0, 4.0, 5.0, 6.5, 8.0, 10.0, 100.0}, "#it{p}_{T} (GeV/#it{c})"};
  ConfigurableAxis configetaAxis{"configetaAxis", {VARIABLE_WIDTH, -0.8, -0.4, -0.2, 0, 0.2, 0.4, 0.8}, "Eta"};
  ConfigurableAxis configthnAxisPol{"configthnAxisPol", {VARIABLE_WIDTH, -1.0, -0.6, -0.2, 0, 0.2, 0.4, 0.8}, "Pol"};
  ConfigurableAxis configbinAxis{"configbinAxis", {VARIABLE_WIDTH, -0.8, -0.4, -0.2, 0, 0.2, 0.4, 0.8}, "BA"};
  */
  // ConfigurableAxis configphiAxis{"configphiAxis", {VARIABLE_WIDTH, 0.0, 0.2, 0.4, 0.8, 1.0, 2.0, 2.5, 3.0, 4.0, 5.0, 5.5, 6.28}, "PhiAxis"};
  struct : ConfigurableGroup {
    Configurable<bool> requireRCTFlagChecker{"requireRCTFlagChecker", true, "Check event quality in run condition table"};
    Configurable<std::string> cfgEvtRCTFlagCheckerLabel{"cfgEvtRCTFlagCheckerLabel", "CBT_hadronPID", "Evt sel: RCT flag checker label"};
    Configurable<bool> cfgEvtRCTFlagCheckerZDCCheck{"cfgEvtRCTFlagCheckerZDCCheck", true, "Evt sel: RCT flag checker ZDC check"};
    Configurable<bool> cfgEvtRCTFlagCheckerLimitAcceptAsBad{"cfgEvtRCTFlagCheckerLimitAcceptAsBad", false, "Evt sel: RCT flag checker treat Limited Acceptance As Bad"};
  } rctCut;
  struct : ConfigurableGroup {
    ConfigurableAxis configcentAxis{"configcentAxis", {VARIABLE_WIDTH, 0.0, 10.0, 40.0, 80.0}, "Cent V0M"};
    ConfigurableAxis configthnAxispT{"configthnAxisPt", {VARIABLE_WIDTH, 0.2, 0.5, 1.0, 1.5, 2.0, 2.5, 3.0, 4.0, 5.0, 6.5, 8.0, 10.0, 100.0}, "#it{p}_{T} (GeV/#it{c})"};
    ConfigurableAxis configetaAxis{"configetaAxis", {VARIABLE_WIDTH, -0.8, -0.4, -0.2, 0, 0.2, 0.4, 0.8}, "Eta"};
    ConfigurableAxis configthnAxisPol{"configthnAxisPol", {VARIABLE_WIDTH, -1.0, -0.6, -0.2, 0, 0.2, 0.4, 0.8}, "Pol"};
    ConfigurableAxis configbinAxis{"configbinAxis", {VARIABLE_WIDTH, -0.8, -0.4, -0.2, 0, 0.2, 0.4, 0.8}, "BA"};
  } axisGrp;
  struct : ConfigurableGroup {
    ConfigurableAxis axisVertex{"axisVertex", {5, -10, 10}, "vertex axis for bin"};
    ConfigurableAxis axisMultiplicityClass{"axisMultiplicityClass", {8, 0, 80}, "multiplicity percentile for bin"};
    Configurable<int> nMix{"nMix", 5, "number of event mixing"};
  } meGrp;

  RCTFlagsChecker rctChecker;

  SliceCache cache;
  HistogramRegistry histos{"histos", {}, OutputObjHandlingPolicy::AnalysisObject};

  void init(o2::framework::InitContext&)
  {

    rctChecker.init(rctCut.cfgEvtRCTFlagCheckerLabel, rctCut.cfgEvtRCTFlagCheckerZDCCheck, rctCut.cfgEvtRCTFlagCheckerLimitAcceptAsBad);

    AxisSpec thnAxisres{binGrp.resNbins, binGrp.lbinres, binGrp.hbinres, "Reso"};
    AxisSpec thnAxisInvMass{binGrp.IMNbins, binGrp.lbinIM, binGrp.hbinIM, "#it{M} (GeV/#it{c}^{2})"};
    AxisSpec spAxis = {binGrp.spNbins, binGrp.lbinsp, binGrp.hbinsp, "Sp"};
    // AxisSpec qxZDCAxis = {binGrp.QxyNbins, binGrp.lbinQxy, binGrp.hbinQxy, "Qx"};
    //  AxisSpec centAxis = {CentNbins, lbinCent, hbinCent, "V0M (%)"};

    std::vector<AxisSpec> runaxes = {thnAxisInvMass, axisGrp.configthnAxispT, axisGrp.configthnAxisPol, axisGrp.configcentAxis};
    if (needetaaxis)
      runaxes.insert(runaxes.end(), {axisGrp.configbinAxis});

    if (checkwithpub) {
      if (useprofile == 2) {
        histos.add("hpuxQxpvscentpteta", "hpuxQxpvscentpteta", HistType::kTHnSparseF, {axisGrp.configcentAxis, axisGrp.configthnAxispT, axisGrp.configetaAxis, spAxis}, true);
        histos.add("hpuyQypvscentpteta", "hpuyQypvscentpteta", HistType::kTHnSparseF, {axisGrp.configcentAxis, axisGrp.configthnAxispT, axisGrp.configetaAxis, spAxis}, true);
        histos.add("hpuxQxtvscentpteta", "hpuxQxtvscentpteta", HistType::kTHnSparseF, {axisGrp.configcentAxis, axisGrp.configthnAxispT, axisGrp.configetaAxis, spAxis}, true);
        histos.add("hpuyQytvscentpteta", "hpuyQytvscentpteta", HistType::kTHnSparseF, {axisGrp.configcentAxis, axisGrp.configthnAxispT, axisGrp.configetaAxis, spAxis}, true);
        histos.add("hpuxyQxytvscentpteta", "hpuxyQxytvscentpteta", HistType::kTHnSparseF, {axisGrp.configcentAxis, axisGrp.configthnAxispT, axisGrp.configetaAxis, spAxis}, true);
        histos.add("hpuxyQxypvscentpteta", "hpuxyQxypvscentpteta", HistType::kTHnSparseF, {axisGrp.configcentAxis, axisGrp.configthnAxispT, axisGrp.configetaAxis, spAxis}, true);
        histos.add("hpoddv1vscentpteta", "hpoddv1vscentpteta", HistType::kTHnSparseF, {axisGrp.configcentAxis, axisGrp.configthnAxispT, axisGrp.configetaAxis, spAxis}, true);
        histos.add("hpevenv1vscentpteta", "hpevenv1vscentpteta", HistType::kTHnSparseF, {axisGrp.configcentAxis, axisGrp.configthnAxispT, axisGrp.configetaAxis, spAxis}, true);
        histos.add("hpv21", "hpv21", HistType::kTHnSparseF, {axisGrp.configcentAxis, axisGrp.configthnAxispT, axisGrp.configetaAxis, spAxis}, true);
        histos.add("hpv22", "hpv22", HistType::kTHnSparseF, {axisGrp.configcentAxis, axisGrp.configthnAxispT, axisGrp.configetaAxis, spAxis}, true);
        histos.add("hpv23", "hpv23", HistType::kTHnSparseF, {axisGrp.configcentAxis, axisGrp.configthnAxispT, axisGrp.configetaAxis, spAxis}, true);
        histos.add("hpx2Tx1Ax1Cvscentpteta", "hpx2Tx1Ax1Cvscentpteta", HistType::kTHnSparseF, {axisGrp.configcentAxis, axisGrp.configthnAxispT, axisGrp.configetaAxis, spAxis}, true);
        histos.add("hpx2Ty1Ay1Cvscentpteta", "hpx2Ty1Ay1Cvscentpteta", HistType::kTHnSparseF, {axisGrp.configcentAxis, axisGrp.configthnAxispT, axisGrp.configetaAxis, spAxis}, true);
        histos.add("hpy2Tx1Ay1Cvscentpteta", "hpy2Tx1Ay1Cvscentpteta", HistType::kTHnSparseF, {axisGrp.configcentAxis, axisGrp.configthnAxispT, axisGrp.configetaAxis, spAxis}, true);
        histos.add("hpy2Ty1Ax1Cvscentpteta", "hpy2Ty1Ax1Cvscentpteta", HistType::kTHnSparseF, {axisGrp.configcentAxis, axisGrp.configthnAxispT, axisGrp.configetaAxis, spAxis}, true);
        histos.add("hpx1Ax1Cvscentpteta", "hpx1Ax1Cvscentpteta", HistType::kTHnSparseF, {axisGrp.configcentAxis, axisGrp.configthnAxispT, axisGrp.configetaAxis, spAxis}, true);
        histos.add("hpy1Ay1Cvscentpteta", "hpy1Ay1Cvscentpteta", HistType::kTHnSparseF, {axisGrp.configcentAxis, axisGrp.configthnAxispT, axisGrp.configetaAxis, spAxis}, true);
        histos.add("hpx1Avscentpteta", "hpx1Avscentpteta", HistType::kTHnSparseF, {axisGrp.configcentAxis, axisGrp.configthnAxispT, axisGrp.configetaAxis, spAxis}, true);
        histos.add("hpx1Cvscentpteta", "hpx1Cvscentpteta", HistType::kTHnSparseF, {axisGrp.configcentAxis, axisGrp.configthnAxispT, axisGrp.configetaAxis, spAxis}, true);
        histos.add("hpy1Avscentpteta", "hpy1Avscentpteta", HistType::kTHnSparseF, {axisGrp.configcentAxis, axisGrp.configthnAxispT, axisGrp.configetaAxis, spAxis}, true);
        histos.add("hpy1Cvscentpteta", "hpy1Cvscentpteta", HistType::kTHnSparseF, {axisGrp.configcentAxis, axisGrp.configthnAxispT, axisGrp.configetaAxis, spAxis}, true);

        histos.add("hpx2Tx1Avscentpteta", "hpx2Tx1Avscentpteta", HistType::kTHnSparseF, {axisGrp.configcentAxis, axisGrp.configthnAxispT, axisGrp.configetaAxis, spAxis}, true);
        histos.add("hpx2Tx1Cvscentpteta", "hpx2Tx1Cvscentpteta", HistType::kTHnSparseF, {axisGrp.configcentAxis, axisGrp.configthnAxispT, axisGrp.configetaAxis, spAxis}, true);
        histos.add("hpx2Ty1Avscentpteta", "hpx2Ty1Avscentpteta", HistType::kTHnSparseF, {axisGrp.configcentAxis, axisGrp.configthnAxispT, axisGrp.configetaAxis, spAxis}, true);
        histos.add("hpx2Ty1Cvscentpteta", "hpx2Ty1Cvscentpteta", HistType::kTHnSparseF, {axisGrp.configcentAxis, axisGrp.configthnAxispT, axisGrp.configetaAxis, spAxis}, true);
        histos.add("hpy2Tx1Avscentpteta", "hpy2Tx1Avscentpteta", HistType::kTHnSparseF, {axisGrp.configcentAxis, axisGrp.configthnAxispT, axisGrp.configetaAxis, spAxis}, true);
        histos.add("hpy2Ty1Cvscentpteta", "hpy2Ty1Cvscentpteta", HistType::kTHnSparseF, {axisGrp.configcentAxis, axisGrp.configthnAxispT, axisGrp.configetaAxis, spAxis}, true);
        histos.add("hpy2Ty1Avscentpteta", "hpy2Ty1Avscentpteta", HistType::kTHnSparseF, {axisGrp.configcentAxis, axisGrp.configthnAxispT, axisGrp.configetaAxis, spAxis}, true);
        histos.add("hpy2Tx1Cvscentpteta", "hpy2Tx1Cvscentpteta", HistType::kTHnSparseF, {axisGrp.configcentAxis, axisGrp.configthnAxispT, axisGrp.configetaAxis, spAxis}, true);
        histos.add("hpx1Ay1Cvscentpteta", "hpx1Ay1Cvscentpteta", HistType::kTHnSparseF, {axisGrp.configcentAxis, axisGrp.configthnAxispT, axisGrp.configetaAxis, spAxis}, true);
        histos.add("hpy1Ax1Cvscentpteta", "hpy1Ax1Cvscentpteta", HistType::kTHnSparseF, {axisGrp.configcentAxis, axisGrp.configthnAxispT, axisGrp.configetaAxis, spAxis}, true);
        histos.add("hpx2Tvscentpteta", "hpx2Tvscentpteta", HistType::kTHnSparseF, {axisGrp.configcentAxis, axisGrp.configthnAxispT, axisGrp.configetaAxis, spAxis}, true);
        histos.add("hpy2Tvscentpteta", "hpy2Tvscentpteta", HistType::kTHnSparseF, {axisGrp.configcentAxis, axisGrp.configthnAxispT, axisGrp.configetaAxis, spAxis}, true);

        histos.add("hpuxvscentpteta", "hpuxvscentpteta", HistType::kTHnSparseF, {axisGrp.configcentAxis, axisGrp.configthnAxispT, axisGrp.configetaAxis, spAxis}, true);
        histos.add("hpuyvscentpteta", "hpuyvscentpteta", HistType::kTHnSparseF, {axisGrp.configcentAxis, axisGrp.configthnAxispT, axisGrp.configetaAxis, spAxis}, true);
        /*
              histos.add("hpuxvscentptetaneg", "hpuxvscentptetaneg", HistType::kTHnSparseF, {axisGrp.configcentAxis, configthnAxispT, configetaAxis, spAxis}, true);
              histos.add("hpuyvscentptetaneg", "hpuyvscentptetaneg", HistType::kTHnSparseF, {axisGrp.configcentAxis, configthnAxispT, configetaAxis, spAxis}, true);

              histos.add("hpuxQxpvscentptetaneg", "hpuxQxpvscentptetaneg", HistType::kTHnSparseF, {axisGrp.configcentAxis, configthnAxispT, configetaAxis, spAxis}, true);
              histos.add("hpuyQypvscentptetaneg", "hpuyQypvscentptetaneg", HistType::kTHnSparseF, {axisGrp.configcentAxis, configthnAxispT, configetaAxis, spAxis}, true);
              histos.add("hpuxQxtvscentptetaneg", "hpuxQxtvscentptetaneg", HistType::kTHnSparseF, {axisGrp.configcentAxis, configthnAxispT, configetaAxis, spAxis}, true);
              histos.add("hpuyQytvscentptetaneg", "hpuyQytvscentptetaneg", HistType::kTHnSparseF, {axisGrp.configcentAxis, configthnAxispT, configetaAxis, spAxis}, true);
              histos.add("hpuxyQxytvscentptetaneg", "hpuxyQxytvscentptetaneg", HistType::kTHnSparseF, {axisGrp.configcentAxis, configthnAxispT, configetaAxis, spAxis}, true);
              histos.add("hpuxyQxypvscentptetaneg", "hpuxyQxypvscentptetaneg", HistType::kTHnSparseF, {axisGrp.configcentAxis, configthnAxispT, configetaAxis, spAxis}, true);
              histos.add("hpoddv1vscentptetaneg", "hpoddv1vscentptetaneg", HistType::kTHnSparseF, {axisGrp.configcentAxis, configthnAxispT, configetaAxis, spAxis}, true);
              histos.add("hpevenv1vscentptetaneg", "hpevenv1vscentptetaneg", HistType::kTHnSparseF, {axisGrp.configcentAxis, configthnAxispT, configetaAxis, spAxis}, true);
        */

        histos.add("hpQxtQxpvscent", "hpQxtQxpvscent", HistType::kTHnSparseF, {axisGrp.configcentAxis, spAxis}, true);
        histos.add("hpQytQypvscent", "hpQytQypvscent", HistType::kTHnSparseF, {axisGrp.configcentAxis, spAxis}, true);
        histos.add("hpQxytpvscent", "hpQxytpvscent", HistType::kTHnSparseF, {axisGrp.configcentAxis, spAxis}, true);
        histos.add("hpQxtQypvscent", "hpQxtQypvscent", HistType::kTHnSparseF, {axisGrp.configcentAxis, spAxis}, true);
        histos.add("hpQxpQytvscent", "hpQxpQytvscent", HistType::kTHnSparseF, {axisGrp.configcentAxis, spAxis}, true);

        histos.add("hpQxpvscent", "hpQxpvscent", HistType::kTHnSparseF, {axisGrp.configcentAxis, spAxis}, true);
        histos.add("hpQxtvscent", "hpQxtvscent", HistType::kTHnSparseF, {axisGrp.configcentAxis, spAxis}, true);
        histos.add("hpQypvscent", "hpQypvscent", HistType::kTHnSparseF, {axisGrp.configcentAxis, spAxis}, true);
        histos.add("hpQytvscent", "hpQytvscent", HistType::kTHnSparseF, {axisGrp.configcentAxis, spAxis}, true);
      } else {
        histos.add("hpuxQxpvscentpteta", "hpuxQxpvscentpteta", HistType::kTHnSparseF, {axisGrp.configcentAxis, axisGrp.configthnAxispT, axisGrp.configetaAxis, spAxis}, true);
        histos.add("hpuyQypvscentpteta", "hpuyQypvscentpteta", HistType::kTHnSparseF, {axisGrp.configcentAxis, axisGrp.configthnAxispT, axisGrp.configetaAxis, spAxis}, true);
        histos.add("hpuxQxtvscentpteta", "hpuxQxtvscentpteta", HistType::kTHnSparseF, {axisGrp.configcentAxis, axisGrp.configthnAxispT, axisGrp.configetaAxis, spAxis}, true);
        histos.add("hpuyQytvscentpteta", "hpuyQytvscentpteta", HistType::kTHnSparseF, {axisGrp.configcentAxis, axisGrp.configthnAxispT, axisGrp.configetaAxis, spAxis}, true);
        histos.add("hpuxyQxytvscentpteta", "hpuxyQxytvscentpteta", HistType::kTHnSparseF, {axisGrp.configcentAxis, axisGrp.configthnAxispT, axisGrp.configetaAxis, spAxis}, true);
        histos.add("hpuxyQxypvscentpteta", "hpuxyQxypvscentpteta", HistType::kTHnSparseF, {axisGrp.configcentAxis, axisGrp.configthnAxispT, axisGrp.configetaAxis, spAxis}, true);
        histos.add("hpoddv1vscentpteta", "hpoddv1vscentpteta", HistType::kTHnSparseF, {axisGrp.configcentAxis, axisGrp.configthnAxispT, axisGrp.configetaAxis, spAxis}, true);
        histos.add("hpevenv1vscentpteta", "hpevenv1vscentpteta", HistType::kTHnSparseF, {axisGrp.configcentAxis, axisGrp.configthnAxispT, axisGrp.configetaAxis, spAxis}, true);

        histos.add("hpuxvscentpteta", "hpuxvscentpteta", HistType::kTHnSparseF, {axisGrp.configcentAxis, axisGrp.configthnAxispT, axisGrp.configetaAxis, spAxis}, true);
        histos.add("hpuyvscentpteta", "hpuyvscentpteta", HistType::kTHnSparseF, {axisGrp.configcentAxis, axisGrp.configthnAxispT, axisGrp.configetaAxis, spAxis}, true);
        /*histos.add("hpuxvscentptetaneg", "hpuxvscentptetaneg", HistType::kTHnSparseF, {axisGrp.configcentAxis, axisGrp.configthnAxispT, axisGrp.configetaAxis, spAxis}, true);
        histos.add("hpuyvscentptetaneg", "hpuyvscentptetaneg", HistType::kTHnSparseF, {axisGrp.configcentAxis, axisGrp.configthnAxispT, axisGrp.configetaAxis, spAxis}, true);

        histos.add("hpuxQxpvscentptetaneg", "hpuxQxpvscentptetaneg", HistType::kTHnSparseF, {axisGrp.configcentAxis, axisGrp.configthnAxispT, axisGrp.configetaAxis, spAxis}, true);
        histos.add("hpuyQypvscentptetaneg", "hpuyQypvscentptetaneg", HistType::kTHnSparseF, {axisGrp.configcentAxis, axisGrp.configthnAxispT, axisGrp.configetaAxis, spAxis}, true);
        histos.add("hpuxQxtvscentptetaneg", "hpuxQxtvscentptetaneg", HistType::kTHnSparseF, {axisGrp.configcentAxis, axisGrp.configthnAxispT, axisGrp.configetaAxis, spAxis}, true);
        histos.add("hpuyQytvscentptetaneg", "hpuyQytvscentptetaneg", HistType::kTHnSparseF, {axisGrp.configcentAxis, axisGrp.configthnAxispT, axisGrp.configetaAxis, spAxis}, true);
        histos.add("hpuxyQxytvscentptetaneg", "hpuxyQxytvscentptetaneg", HistType::kTHnSparseF, {axisGrp.configcentAxis, axisGrp.configthnAxispT, axisGrp.configetaAxis, spAxis}, true);
        histos.add("hpuxyQxypvscentptetaneg", "hpuxyQxypvscentptetaneg", HistType::kTHnSparseF, {axisGrp.configcentAxis, axisGrp.configthnAxispT, axisGrp.configetaAxis, spAxis}, true);
        histos.add("hpoddv1vscentptetaneg", "hpoddv1vscentptetaneg", HistType::kTHnSparseF, {axisGrp.configcentAxis, axisGrp.configthnAxispT, axisGrp.configetaAxis, spAxis}, true);
        histos.add("hpevenv1vscentptetaneg", "hpevenv1vscentptetaneg", HistType::kTHnSparseF, {axisGrp.configcentAxis, axisGrp.configthnAxispT, axisGrp.configetaAxis, spAxis}, true);*/

        histos.add("hpQxtQxpvscent", "hpQxtQxpvscent", HistType::kTHnSparseF, {axisGrp.configcentAxis, spAxis}, true);
        histos.add("hpQytQypvscent", "hpQytQypvscent", HistType::kTHnSparseF, {axisGrp.configcentAxis, spAxis}, true);
        histos.add("hpQxytpvscent", "hpQxytpvscent", HistType::kTHnSparseF, {axisGrp.configcentAxis, spAxis}, true);
        histos.add("hpQxtQypvscent", "hpQxtQypvscent", HistType::kTHnSparseF, {axisGrp.configcentAxis, spAxis}, true);
        histos.add("hpQxpQytvscent", "hpQxpQytvscent", HistType::kTHnSparseF, {axisGrp.configcentAxis, spAxis}, true);

        histos.add("hpQxpvscent", "hpQxpvscent", HistType::kTHnSparseF, {axisGrp.configcentAxis, spAxis}, true);
        histos.add("hpQxtvscent", "hpQxtvscent", HistType::kTHnSparseF, {axisGrp.configcentAxis, spAxis}, true);
        histos.add("hpQypvscent", "hpQypvscent", HistType::kTHnSparseF, {axisGrp.configcentAxis, spAxis}, true);
        histos.add("hpQytvscent", "hpQytvscent", HistType::kTHnSparseF, {axisGrp.configcentAxis, spAxis}, true);
      }
    }

    histos.add("hCentrality", "Centrality distribution", kTH1F, {{axisGrp.configcentAxis}});
    // histos.add("hpsiApsiC", "hpsiApsiC", kTHnSparseF, {psiACAxis, psiACAxis});
    //  histos.add("hpsiApsiC", "hpsiApsiC", kTH2F, {psiACAxis, psiACAxis});
    // histos.add("hphiminuspsiA", "hphiminuspisA", kTH1F, {{50, 0, 6.28}}, true);
    // histos.add("hphiminuspsiC", "hphiminuspisC", kTH1F, {{50, 0, 6.28}}, true);
    //  histos.add("hCentrality0", "Centrality distribution0", kTH1F, {{centAxis}});
    //  histos.add("hCentrality1", "Centrality distribution1", kTH1F, {{centAxis}});
    //  histos.add("hCentrality2", "Centrality distribution2", kTH1F, {{centAxis}});
    //  histos.add("hCentrality3", "Centrality distribution3", kTH1F, {{centAxis}});

    if (!checkwithpub) {
      // histos.add("hVtxZ", "Vertex distribution in Z;Z (cm)", kTH1F, {{20, -10.0, 10.0}});
      histos.add("hpRes", "hpRes", HistType::kTHnSparseF, {axisGrp.configcentAxis, thnAxisres});
      histos.add("hpResSin", "hpResSin", HistType::kTHnSparseF, {axisGrp.configcentAxis, thnAxisres});
      /*histos.add("hpCosPsiA", "hpCosPsiA", HistType::kTHnSparseF, {axisGrp.configcentAxis, thnAxisres});
      histos.add("hpCosPsiC", "hpCosPsiC", HistType::kTHnSparseF, {axisGrp.configcentAxis, thnAxisres});
      histos.add("hpSinPsiA", "hpSinPsiA", HistType::kTHnSparseF, {axisGrp.configcentAxis, thnAxisres});
      histos.add("hpSinPsiC", "hpSinPsiC", HistType::kTHnSparseF, {axisGrp.configcentAxis, thnAxisres});*/
      /*histos.add("hcentQxZDCA", "hcentQxZDCA", kTH2F, {{centAxis}, {qxZDCAxis}});
      histos.add("hcentQyZDCA", "hcentQyZDCA", kTH2F, {{centAxis}, {qxZDCAxis}});
      histos.add("hcentQxZDCC", "hcentQxZDCC", kTH2F, {{centAxis}, {qxZDCAxis}});
      histos.add("hcentQyZDCC", "hcentQyZDCC", kTH2F, {{centAxis}, {qxZDCAxis}});*/

      if (usesubdet) {
        histos.add("hSparseLambdaCosPsiA", "hSparseLambdaCosPsiA", HistType::kTHnSparseF, runaxes, true);
        histos.add("hSparseLambdaSinPsiA", "hSparseLambdaSinPsiA", HistType::kTHnSparseF, runaxes, true);
        histos.add("hSparseLambdaCosPsiC", "hSparseLambdaCosPsiC", HistType::kTHnSparseF, runaxes, true);
        histos.add("hSparseLambdaSinPsiC", "hSparseLambdaSinPsiC", HistType::kTHnSparseF, runaxes, true);
      }
      histos.add("hSparseLambdaCosPsi", "hSparseLambdaCosPsi", HistType::kTHnSparseF, runaxes, true);
      histos.add("hSparseLambdaSinPsi", "hSparseLambdaSinPsi", HistType::kTHnSparseF, runaxes, true);
      if (usesubdet) {
        histos.add("hSparseAntiLambdaCosPsiA", "hSparseAntiLambdaCosPsiA", HistType::kTHnSparseF, runaxes, true);
        histos.add("hSparseAntiLambdaSinPsiA", "hSparseAntiLambdaSinPsiA", HistType::kTHnSparseF, runaxes, true);
        histos.add("hSparseAntiLambdaCosPsiC", "hSparseAntiLambdaCosPsiC", HistType::kTHnSparseF, runaxes, true);
        histos.add("hSparseAntiLambdaSinPsiC", "hSparseAntiLambdaSinPsiC", HistType::kTHnSparseF, runaxes, true);
      }
      histos.add("hSparseAntiLambdaCosPsi", "hSparseAntiLambdaCosPsi", HistType::kTHnSparseF, runaxes, true);
      histos.add("hSparseAntiLambdaSinPsi", "hSparseAntiLambdaSinPsi", HistType::kTHnSparseF, runaxes, true);

      histos.add("hSparseLambdaPol", "hSparseLambdaPol", HistType::kTHnSparseF, runaxes, true);
      histos.add("hSparseLambdaPolwgt", "hSparseLambdaPolwgt", HistType::kTHnSparseF, runaxes, true);
      if (usesubdet) {
        histos.add("hSparseLambdaPolA", "hSparseLambdaPolA", HistType::kTHnSparseF, runaxes, true);
        histos.add("hSparseLambdaPolC", "hSparseLambdaPolC", HistType::kTHnSparseF, runaxes, true);
      }
      histos.add("hSparseAntiLambdaPol", "hSparseAntiLambdaPol", HistType::kTHnSparseF, runaxes, true);
      histos.add("hSparseAntiLambdaPolwgt", "hSparseAntiLambdaPolwgt", HistType::kTHnSparseF, runaxes, true);
      if (usesubdet) {
        histos.add("hSparseAntiLambdaPolA", "hSparseAntiLambdaPolA", HistType::kTHnSparseF, runaxes, true);
        histos.add("hSparseAntiLambdaPolC", "hSparseAntiLambdaPolC", HistType::kTHnSparseF, runaxes, true);
      }
      histos.add("hSparseLambda_corr1a", "hSparseLambda_corr1a", HistType::kTHnSparseF, runaxes, true);
      histos.add("hSparseLambda_corr1b", "hSparseLambda_corr1b", HistType::kTHnSparseF, runaxes, true);
      // histos.add("hSparseLambda_corr1c", "hSparseLambda_corr1c", HistType::kTHnSparseF, {thnAxisInvMass, configthnAxispT, configphiAxis, configcentAxis, configbinAxis}, true);
      histos.add("hSparseAntiLambda_corr1a", "hSparseAntiLambda_corr1a", HistType::kTHnSparseF, runaxes, true);
      histos.add("hSparseAntiLambda_corr1b", "hSparseAntiLambda_corr1b", HistType::kTHnSparseF, runaxes, true);
      // histos.add("hSparseAntiLambda_corr1c", "hSparseAntiLambda_corr1c", HistType::kTHnSparseF, {thnAxisInvMass, configthnAxispT, configphiAxis, configcentAxis, configbinAxis}, true);

      histos.add("hSparseLambda_corr2a", "hSparseLambda_corr2a", HistType::kTHnSparseF, runaxes, true);
      // histos.add("hSparseLambda_corr2b", "hSparseLambda_corr2b", HistType::kTHnSparseF, runaxes, true);
      histos.add("hSparseAntiLambda_corr2a", "hSparseAntiLambda_corr2a", HistType::kTHnSparseF, runaxes, true);
      // histos.add("hSparseAntiLambda_corr2b", "hSparseAntiLambda_corr2b", HistType::kTHnSparseF, runaxes, true);
    }
  }

  template <typename T>
  bool selectionTrack(const T& candidate)
  {
    if (!(candidate.isGlobalTrack() && candidate.isPVContributor() && candidate.itsNCls() > cfgITScluster && candidate.tpcNClsFound() > cfgTPCcluster && candidate.itsNClsInnerBarrel() >= 1)) {
      return false;
    }
    return true;
  }

  template <typename Collision, typename V0>
  bool SelectionV0(Collision const& collision, V0 const& candidate)
  {
    if (TMath::Abs(candidate.dcav0topv()) > cMaxV0DCA) {
      return false;
    }
    const float pT = candidate.pt();
    const float tranRad = candidate.v0radius();
    const float dcaDaughv0 = TMath::Abs(candidate.dcaV0daughters());
    const float cpav0 = candidate.v0cosPA();

    float CtauLambda = candidate.distovertotmom(collision.posX(), collision.posY(), collision.posZ()) * massLambda;
    float CtauK0s = candidate.distovertotmom(collision.posX(), collision.posY(), collision.posZ()) * massK0s;
    // float lowmasscutlambda = cMinLambdaMass;
    // float highmasscutlambda = cMaxLambdaMass;

    if (pT < ConfV0PtMin) {
      return false;
    }
    if (dcaDaughv0 > ConfV0DCADaughMax) {
      return false;
    }
    if (cpav0 < ConfV0CPAMin) {
      return false;
    }
    if (tranRad < ConfV0TranRadV0Min) {
      return false;
    }
    if (tranRad > ConfV0TranRadV0Max) {
      return false;
    }
    if (analyzeLambda && TMath::Abs(CtauLambda) > cMaxV0LifeTime) {
      return false;
    }
    if (analyzeK0s && TMath::Abs(CtauK0s) > cMaxV0LifeTime) {
      return false;
    }
    if (analyzeLambda && TMath::Abs(candidate.yLambda()) > ConfV0Rap) {
      return false;
    }
    if (analyzeK0s && TMath::Abs(candidate.yK0Short()) > ConfV0Rap) {
      return false;
    }
    return true;
  }

  template <typename V0, typename T>
  bool isSelectedV0Daughter(V0 const& candidate, T const& track, int pid)
  {
    // const auto eta = track.eta();
    // const auto pt = track.pt();
    const auto tpcNClsF = track.tpcNClsFound();
    if (track.tpcNClsCrossedRows() < cfgTPCcluster) {
      return false;
    }
    /*if (TMath::Abs(eta) > ConfDaughEta) {
      return false;
      }*/
    if (tpcNClsF < ConfDaughTPCnclsMin) {
      return false;
    }
    if (track.tpcCrossedRowsOverFindableCls() < rcrfc) {
      return false;
    }

    if (pid == 0 && TMath::Abs(track.tpcNSigmaPr()) > ConfDaughPIDCuts) {
      return false;
    }
    if (pid == 1 && TMath::Abs(track.tpcNSigmaPi()) > ConfDaughPIDCuts) {
      return false;
    }
    if (pid == 0 && (candidate.positivept() < cfgDaughPrPt || candidate.negativept() < cfgDaughPiPt)) {
      return false; // doesn´t pass lambda pT sels
    }
    if (pid == 1 && (candidate.positivept() < cfgDaughPiPt || candidate.negativept() < cfgDaughPrPt)) {
      return false; // doesn´t pass antilambda pT sels
    }
    if (std::abs(candidate.positiveeta()) > ConfDaughEta || std::abs(candidate.negativeeta()) > ConfDaughEta) {
      return false;
    }

    if (pid == 0 && (TMath::Abs(candidate.dcapostopv()) < cMinV0DCAPr || TMath::Abs(candidate.dcanegtopv()) < cMinV0DCAPi)) {
      return false;
    }
    if (pid == 1 && (TMath::Abs(candidate.dcapostopv()) < cMinV0DCAPi || TMath::Abs(candidate.dcanegtopv()) < cMinV0DCAPr)) {
      return false;
    }

    return true;
  }

  template <typename TV0>
  bool isCompatible(TV0 const& v0, int pid /*0: lambda, 1: antilambda*/)
  {
    // checks if this V0 is compatible with the requested hypothesis

    // de-ref track extras
    auto posTrackExtra = v0.template posTrackExtra_as<dauTracks>();
    auto negTrackExtra = v0.template negTrackExtra_as<dauTracks>();

    // check for desired kinematics
    if (pid == 0 && (v0.positivept() < cfgDaughPrPt || v0.negativept() < cfgDaughPiPt)) {
      return false; // doesn´t pass lambda pT sels
    }
    if (pid == 1 && (v0.positivept() < cfgDaughPiPt || v0.negativept() < cfgDaughPrPt)) {
      return false; // doesn´t pass antilambda pT sels
    }
    if (std::abs(v0.positiveeta()) > ConfDaughEta || std::abs(v0.negativeeta()) > ConfDaughEta) {
      return false;
    }

    // check TPC tracking properties

    if (posTrackExtra.tpcNClsCrossedRows() < cfgTPCcluster || negTrackExtra.tpcNClsCrossedRows() < cfgTPCcluster) {
      return false;
    }
    if (posTrackExtra.tpcNClsFound() < ConfDaughTPCnclsMin || negTrackExtra.tpcNClsFound() < ConfDaughTPCnclsMin) {
      return false;
    }
    if (posTrackExtra.tpcCrossedRowsOverFindableCls() < rcrfc || negTrackExtra.tpcCrossedRowsOverFindableCls() < rcrfc) {
      return false;
    }

    // check TPC PID
    if (pid == 0 && ((std::abs(posTrackExtra.tpcNSigmaPr()) > ConfDaughPIDCuts) || (std::abs(negTrackExtra.tpcNSigmaPi()) > ConfDaughPIDCuts))) {
      return false;
    }
    if (pid == 1 && ((std::abs(posTrackExtra.tpcNSigmaPi()) > ConfDaughPIDCuts) || (std::abs(negTrackExtra.tpcNSigmaPr()) > ConfDaughPIDCuts))) {
      return false;
    }

    if (pid == 0 && (TMath::Abs(v0.dcapostopv()) < cMinV0DCAPr || TMath::Abs(v0.dcanegtopv()) < cMinV0DCAPi)) {
      return false;
    }
    if (pid == 1 && (TMath::Abs(v0.dcapostopv()) < cMinV0DCAPi || TMath::Abs(v0.dcanegtopv()) < cMinV0DCAPr)) {
      return false;
    }

    // if we made it this far, it's good
    return true;
  }

  template <typename TV0>
  bool isCompatibleK0s(TV0 const& v0)
  {
    // checks if this V0 is compatible with the requested hypothesis

    // de-ref track extras
    auto posTrackExtra = v0.template posTrackExtra_as<dauTracks>();
    auto negTrackExtra = v0.template negTrackExtra_as<dauTracks>();

    // check for desired kinematics
    if ((v0.positivept() < cfgDaughPrPt || v0.negativept() < cfgDaughPiPt)) {
      return false; // doesn´t pass lambda pT sels
    }
    if (std::abs(v0.positiveeta()) > ConfDaughEta || std::abs(v0.negativeeta()) > ConfDaughEta) {
      return false;
    }
    // check TPC tracking properties
    if (posTrackExtra.tpcNClsCrossedRows() < cfgTPCcluster || negTrackExtra.tpcNClsCrossedRows() < cfgTPCcluster) {
      return false;
    }
    if (posTrackExtra.tpcNClsFound() < ConfDaughTPCnclsMin || negTrackExtra.tpcNClsFound() < ConfDaughTPCnclsMin) {
      return false;
    }
    if (posTrackExtra.tpcCrossedRowsOverFindableCls() < rcrfc || negTrackExtra.tpcCrossedRowsOverFindableCls() < rcrfc) {
      return false;
    }
    // check TPC PID
    if (((std::abs(posTrackExtra.tpcNSigmaPi()) > ConfDaughPIDCuts) || (std::abs(negTrackExtra.tpcNSigmaPi()) > ConfDaughPIDCuts))) {
      return false;
    }
    if ((TMath::Abs(v0.dcapostopv()) < cMinV0DCAPi || TMath::Abs(v0.dcanegtopv()) < cMinV0DCAPi)) {
      return false;
    }
    if ((v0.qtarm() / (std::abs(v0.alpha()))) < qtArmenterosMinForK0) {
      return false;
    }
    // if we made it this far, it's good
    return true;
  }

  double GetPhiInRange(double phi)
  {
    double result = RecoDecay::constrainAngle(phi);
    /*
      double result = phi;
      while (result < 0) {
      // result = result + 2. * TMath::Pi();
      result = result + 2. * o2::constants::math::PI;
      }
      while (result > 2. * TMath::Pi()) {
      // result = result - 2. * TMath::Pi();
      result = result - 2. * o2::constants::math::PI;
      }*/
    return result;
  }

  bool shouldReject(bool LambdaTag, bool aLambdaTag,
                    const ROOT::Math::PxPyPzMVector& Lambdadummy,
                    const ROOT::Math::PxPyPzMVector& AntiLambdadummy)
  {
    const double minMass = 1.105;
    const double maxMass = 1.125;
    return (LambdaTag && aLambdaTag &&
            (Lambdadummy.M() > minMass && Lambdadummy.M() < maxMass) &&
            (AntiLambdadummy.M() > minMass && AntiLambdadummy.M() < maxMass));
  }

  void fillHistograms(bool tag1, bool tag2, const ROOT::Math::PxPyPzMVector& particle,
                      const ROOT::Math::PxPyPzMVector& daughter,
                      double psiZDCC, double psiZDCA, double psiZDC, double centrality,
                      double candmass, double candpt, float desbinvalue, double acvalue)
  {
    TRandom3 randPhi(0);

    ROOT::Math::Boost boost{particle.BoostToCM()};
    auto fourVecDauCM = boost(daughter);
    auto phiangle = TMath::ATan2(fourVecDauCM.Py(), fourVecDauCM.Px());
    if (randGrp.doRandomPhi) {
      phiangle = randPhi.Uniform(0, 2 * TMath::Pi());
    }
    auto phiminuspsiC = GetPhiInRange(phiangle - psiZDCC);
    auto phiminuspsiA = GetPhiInRange(phiangle - psiZDCA);
    auto phiminuspsi = GetPhiInRange(phiangle - psiZDC);
    auto cosThetaStar = fourVecDauCM.Pz() / fourVecDauCM.P();
    auto sinThetaStar = TMath::Sqrt(1 - (cosThetaStar * cosThetaStar));
    auto PolC = TMath::Sin(phiminuspsiC);
    auto PolA = TMath::Sin(phiminuspsiA);
    auto Pol = TMath::Sin(phiminuspsi);

    auto sinPhiStar = TMath::Sin(GetPhiInRange(phiangle));
    auto cosPhiStar = TMath::Cos(GetPhiInRange(phiangle));
    // auto sinThetaStarcosphiphiStar = sinThetaStar * TMath::Cos(2 * GetPhiInRange(particle.Phi() - phiangle));
    // auto phiphiStar = GetPhiInRange(particle.Phi() - phiangle);

    acvalue = (4 / 3.14) * acvalue;
    // PolC = PolC / acvalue;
    // PolA = PolA / acvalue;
    // Pol = Pol / acvalue;
    auto Polwgt = Pol / acvalue;

    // Fill histograms using constructed names
    if (tag2) {
      if (needetaaxis) {
        if (usesubdet) {
          histos.fill(HIST("hSparseAntiLambdaCosPsiA"), candmass, candpt, (TMath::Cos(GetPhiInRange(psiZDCA))), centrality, desbinvalue);
          histos.fill(HIST("hSparseAntiLambdaCosPsiC"), candmass, candpt, (TMath::Cos(GetPhiInRange(psiZDCC))), centrality, desbinvalue);
          histos.fill(HIST("hSparseAntiLambdaSinPsiA"), candmass, candpt, (TMath::Sin(GetPhiInRange(psiZDCA))), centrality, desbinvalue);
          histos.fill(HIST("hSparseAntiLambdaSinPsiC"), candmass, candpt, (TMath::Sin(GetPhiInRange(psiZDCC))), centrality, desbinvalue);
        }
        histos.fill(HIST("hSparseAntiLambdaCosPsi"), candmass, candpt, (TMath::Cos(GetPhiInRange(psiZDC))), centrality, desbinvalue);
        histos.fill(HIST("hSparseAntiLambdaSinPsi"), candmass, candpt, (TMath::Sin(GetPhiInRange(psiZDC))), centrality, desbinvalue);
        if (usesubdet) {
          histos.fill(HIST("hSparseAntiLambdaPolA"), candmass, candpt, PolA, centrality, desbinvalue);
          histos.fill(HIST("hSparseAntiLambdaPolC"), candmass, candpt, PolC, centrality, desbinvalue);
        }
        histos.fill(HIST("hSparseAntiLambdaPol"), candmass, candpt, Pol, centrality, desbinvalue);
        histos.fill(HIST("hSparseAntiLambdaPolwgt"), candmass, candpt, Polwgt, centrality, desbinvalue);
        histos.fill(HIST("hSparseAntiLambda_corr1a"), candmass, candpt, sinPhiStar, centrality, desbinvalue);
        histos.fill(HIST("hSparseAntiLambda_corr1b"), candmass, candpt, cosPhiStar, centrality, desbinvalue);
        // histos.fill(HIST("hSparseAntiLambda_corr1c"), candmass, candpt, phiphiStar, centrality, desbinvalue);
        histos.fill(HIST("hSparseAntiLambda_corr2a"), candmass, candpt, sinThetaStar, centrality, desbinvalue);
        // histos.fill(HIST("hSparseAntiLambda_corr2b"), candmass, candpt, sinThetaStarcosphiphiStar, centrality, desbinvalue);
      } else {
        if (usesubdet) {
          histos.fill(HIST("hSparseAntiLambdaCosPsiA"), candmass, candpt, (TMath::Cos(GetPhiInRange(psiZDCA))), centrality);
          histos.fill(HIST("hSparseAntiLambdaCosPsiC"), candmass, candpt, (TMath::Cos(GetPhiInRange(psiZDCC))), centrality);
          histos.fill(HIST("hSparseAntiLambdaSinPsiA"), candmass, candpt, (TMath::Sin(GetPhiInRange(psiZDCA))), centrality);
          histos.fill(HIST("hSparseAntiLambdaSinPsiC"), candmass, candpt, (TMath::Sin(GetPhiInRange(psiZDCC))), centrality);
        }
        histos.fill(HIST("hSparseAntiLambdaCosPsi"), candmass, candpt, (TMath::Cos(GetPhiInRange(psiZDC))), centrality);
        histos.fill(HIST("hSparseAntiLambdaSinPsi"), candmass, candpt, (TMath::Sin(GetPhiInRange(psiZDC))), centrality);
        if (usesubdet) {
          histos.fill(HIST("hSparseAntiLambdaPolA"), candmass, candpt, PolA, centrality);
          histos.fill(HIST("hSparseAntiLambdaPolC"), candmass, candpt, PolC, centrality);
        }
        histos.fill(HIST("hSparseAntiLambdaPol"), candmass, candpt, Pol, centrality);
        histos.fill(HIST("hSparseAntiLambdaPolwgt"), candmass, candpt, Polwgt, centrality);
        histos.fill(HIST("hSparseAntiLambda_corr1a"), candmass, candpt, sinPhiStar, centrality);
        histos.fill(HIST("hSparseAntiLambda_corr1b"), candmass, candpt, cosPhiStar, centrality);
        // histos.fill(HIST("hSparseAntiLambda_corr1c"), candmass, candpt, phiphiStar, centrality);
        histos.fill(HIST("hSparseAntiLambda_corr2a"), candmass, candpt, sinThetaStar, centrality);
        // histos.fill(HIST("hSparseAntiLambda_corr2b"), candmass, candpt, sinThetaStarcosphiphiStar, centrality);
      }
    }
    if (tag1) {
      if (needetaaxis) {
        if (usesubdet) {
          histos.fill(HIST("hSparseLambdaCosPsiA"), candmass, candpt, (TMath::Cos(GetPhiInRange(psiZDCA))), centrality, desbinvalue);
          histos.fill(HIST("hSparseLambdaCosPsiC"), candmass, candpt, (TMath::Cos(GetPhiInRange(psiZDCC))), centrality, desbinvalue);
          histos.fill(HIST("hSparseLambdaSinPsiA"), candmass, candpt, (TMath::Sin(GetPhiInRange(psiZDCA))), centrality, desbinvalue);
          histos.fill(HIST("hSparseLambdaSinPsiC"), candmass, candpt, (TMath::Sin(GetPhiInRange(psiZDCC))), centrality, desbinvalue);
        }
        histos.fill(HIST("hSparseLambdaCosPsi"), candmass, candpt, (TMath::Cos(GetPhiInRange(psiZDC))), centrality, desbinvalue);
        histos.fill(HIST("hSparseLambdaSinPsi"), candmass, candpt, (TMath::Sin(GetPhiInRange(psiZDC))), centrality, desbinvalue);
        if (usesubdet) {
          histos.fill(HIST("hSparseLambdaPolA"), candmass, candpt, PolA, centrality, desbinvalue);
          histos.fill(HIST("hSparseLambdaPolC"), candmass, candpt, PolC, centrality, desbinvalue);
        }
        histos.fill(HIST("hSparseLambdaPol"), candmass, candpt, Pol, centrality, desbinvalue);
        histos.fill(HIST("hSparseLambdaPolwgt"), candmass, candpt, Polwgt, centrality, desbinvalue);
        histos.fill(HIST("hSparseLambda_corr1a"), candmass, candpt, sinPhiStar, centrality, desbinvalue);
        histos.fill(HIST("hSparseLambda_corr1b"), candmass, candpt, cosPhiStar, centrality, desbinvalue);
        // histos.fill(HIST("hSparseLambda_corr1c"), candmass, candpt, phiphiStar, centrality, desbinvalue);
        histos.fill(HIST("hSparseLambda_corr2a"), candmass, candpt, sinThetaStar, centrality, desbinvalue);
        // histos.fill(HIST("hSparseLambda_corr2b"), candmass, candpt, sinThetaStarcosphiphiStar, centrality, desbinvalue);
      } else {
        if (usesubdet) {
          histos.fill(HIST("hSparseLambdaCosPsiA"), candmass, candpt, (TMath::Cos(GetPhiInRange(psiZDCA))), centrality);
          histos.fill(HIST("hSparseLambdaCosPsiC"), candmass, candpt, (TMath::Cos(GetPhiInRange(psiZDCC))), centrality);
          histos.fill(HIST("hSparseLambdaSinPsiA"), candmass, candpt, (TMath::Sin(GetPhiInRange(psiZDCA))), centrality);
          histos.fill(HIST("hSparseLambdaSinPsiC"), candmass, candpt, (TMath::Sin(GetPhiInRange(psiZDCC))), centrality);
        }
        histos.fill(HIST("hSparseLambdaCosPsi"), candmass, candpt, (TMath::Cos(GetPhiInRange(psiZDC))), centrality);
        histos.fill(HIST("hSparseLambdaSinPsi"), candmass, candpt, (TMath::Sin(GetPhiInRange(psiZDC))), centrality);
        if (usesubdet) {
          histos.fill(HIST("hSparseLambdaPolA"), candmass, candpt, PolA, centrality);
          histos.fill(HIST("hSparseLambdaPolC"), candmass, candpt, PolC, centrality);
        }
        histos.fill(HIST("hSparseLambdaPol"), candmass, candpt, Pol, centrality);
        histos.fill(HIST("hSparseLambdaPolwgt"), candmass, candpt, Polwgt, centrality);
        histos.fill(HIST("hSparseLambda_corr1a"), candmass, candpt, sinPhiStar, centrality);
        histos.fill(HIST("hSparseLambda_corr1b"), candmass, candpt, cosPhiStar, centrality);
        // histos.fill(HIST("hSparseLambda_corr1c"), candmass, candpt, phiphiStar, centrality);
        histos.fill(HIST("hSparseLambda_corr2a"), candmass, candpt, sinThetaStar, centrality);
        // histos.fill(HIST("hSparseLambda_corr2b"), candmass, candpt, sinThetaStarcosphiphiStar, centrality);
      }
    }
  }

  ROOT::Math::PxPyPzMVector Lambda, AntiLambda, Lambdadummy, AntiLambdadummy, Proton, Pion, AntiProton, AntiPion, fourVecDauCM, K0sdummy, K0s;
  ROOT::Math::XYZVector threeVecDauCM, threeVecDauCMXY;
  double phiangle = 0.0;
  // double angleLambda=0.0;
  // double angleAntiLambda=0.0;
  double massLambda = o2::constants::physics::MassLambda;
  double massK0s = o2::constants::physics::MassK0Short;
  double massPr = o2::constants::physics::MassProton;
  double massPi = o2::constants::physics::MassPionCharged;

  Filter collisionFilter = nabs(aod::collision::posZ) < cfgCutVertex;
  Filter centralityFilter = (nabs(aod::cent::centFT0C) < cfgCutCentralityMax && nabs(aod::cent::centFT0C) > cfgCutCentralityMin);
  Filter acceptanceFilter = (nabs(aod::track::eta) < cfgCutEta && nabs(aod::track::pt) > cfgCutPT);
  Filter dcaCutFilter = (nabs(aod::track::dcaXY) < cfgCutDCAxy) && (nabs(aod::track::dcaZ) < cfgCutDCAz);

  using EventCandidates = soa::Filtered<soa::Join<aod::Collisions, aod::EvSels, aod::FT0Mults, aod::FV0Mults, aod::TPCMults, aod::CentFV0As, aod::CentFT0Ms, aod::CentFT0Cs, aod::CentFT0As, aod::SPCalibrationTables, aod::Mults>>;
  using AllTrackCandidates = soa::Filtered<soa::Join<aod::Tracks, aod::TracksExtra, aod::TracksDCA, aod::TrackSelection, aod::pidTPCFullPi, aod::pidTPCFullPr, aod::pidTPCFullKa>>;
  using ResoV0s = aod::V0Datas;

  TProfile2D* accprofileL;
  TProfile2D* accprofileAL;
  // int currentRunNumber = -999;
  // int lastRunNumber = -999;

  using BCsRun3 = soa::Join<aod::BCsWithTimestamps, aod::Run3MatchedToBCSparse>;

  void processData(EventCandidates::iterator const& collision, AllTrackCandidates const& tracks, ResoV0s const& V0s, BCsRun3 const&)
  {

    if (!collision.sel8()) {
      return;
    }
    auto centrality = collision.centFT0C();
    // histos.fill(HIST("hCentrality0"), centrality);
    if (!collision.triggereventsp()) {
      return;
    }
    // histos.fill(HIST("hCentrality1"), centrality);

    if (additionalEvSel && (!collision.selection_bit(aod::evsel::kNoSameBunchPileup) || !collision.selection_bit(aod::evsel::kIsGoodZvtxFT0vsPV))) {
      return;
    }
    // histos.fill(HIST("hCentrality2"), centrality);
    // if (additionalEvSel2 && (!collision.selection_bit(o2::aod::evsel::kNoCollInTimeRangeStandard))) {
    if (additionalEvSel2 && (collision.trackOccupancyInTimeRange() > cfgMaxOccupancy || collision.trackOccupancyInTimeRange() < cfgMinOccupancy)) {
      return;
    }
    // histos.fill(HIST("hCentrality3"), centrality);
    if (additionalEvSel3 && (!collision.selection_bit(aod::evsel::kNoTimeFrameBorder) || !collision.selection_bit(aod::evsel::kNoITSROFrameBorder))) {
      return;
    }

    if (additionalEvSel4 && !collision.selection_bit(o2::aod::evsel::kIsGoodITSLayersAll)) {
      return;
    }

    if (rctCut.requireRCTFlagChecker && !rctChecker(collision)) {
      return;
    }
    // currentRunNumber = collision.foundBC_as<BCsRun3>().runNumber();
    auto bc = collision.foundBC_as<BCsRun3>();

    auto qxZDCA = collision.qxZDCA();
    auto qxZDCC = collision.qxZDCC();
    auto qyZDCA = collision.qyZDCA();
    auto qyZDCC = collision.qyZDCC();
    auto psiZDCC = collision.psiZDCC();
    auto psiZDCA = collision.psiZDCA();

    double modqxZDCA;
    double modqyZDCA;
    double modqxZDCC;
    double modqyZDCC;

    if (cqvas) {
      modqxZDCA = TMath::Sqrt((qxZDCA * qxZDCA) + (qyZDCA * qyZDCA)) * TMath::Cos(psiZDCA);
      modqyZDCA = TMath::Sqrt((qxZDCA * qxZDCA) + (qyZDCA * qyZDCA)) * TMath::Sin(psiZDCA);
      modqxZDCC = TMath::Sqrt((qxZDCC * qxZDCC) + (qyZDCC * qyZDCC)) * TMath::Cos(psiZDCC);
      modqyZDCC = TMath::Sqrt((qxZDCC * qxZDCC) + (qyZDCC * qyZDCC)) * TMath::Sin(psiZDCC);
    } else {
      modqxZDCA = qxZDCA;
      modqyZDCA = qyZDCA;
      modqxZDCC = qxZDCC;
      modqyZDCC = qyZDCC;
    }

    auto psiZDC = TMath::ATan2((modqyZDCC - modqyZDCA), (modqxZDCC - modqxZDCA)); // full event plane
    /*if (useonlypsis) {
      psiZDC = psiZDCC - psiZDCA;
      }*/

    histos.fill(HIST("hCentrality"), centrality);
    if (!checkwithpub) {
      // histos.fill(HIST("hVtxZ"), collision.posZ());
      histos.fill(HIST("hpRes"), centrality, (TMath::Cos(GetPhiInRange(psiZDCA - psiZDCC))));
      histos.fill(HIST("hpResSin"), centrality, (TMath::Sin(GetPhiInRange(psiZDCA - psiZDCC))));
      /*histos.fill(HIST("hpCosPsiA"), centrality, (TMath::Cos(GetPhiInRange(psiZDCA))));
      histos.fill(HIST("hpCosPsiC"), centrality, (TMath::Cos(GetPhiInRange(psiZDCC))));
      histos.fill(HIST("hpSinPsiA"), centrality, (TMath::Sin(GetPhiInRange(psiZDCA))));
      histos.fill(HIST("hpSinPsiC"), centrality, (TMath::Sin(GetPhiInRange(psiZDCC))));*/
      /*histos.fill(HIST("hcentQxZDCA"), centrality, qxZDCA);
        histos.fill(HIST("hcentQyZDCA"), centrality, qyZDCA);
        histos.fill(HIST("hcentQxZDCC"), centrality, qxZDCC);
        histos.fill(HIST("hcentQyZDCC"), centrality, qyZDCC);*/
    }

    ///////////checking v1////////////////////////////////
    if (checkwithpub) {

      auto QxtQxp = modqxZDCA * modqxZDCC;
      auto QytQyp = modqyZDCA * modqyZDCC;
      auto Qxytp = QxtQxp + QytQyp;
      auto QxpQyt = modqxZDCA * modqyZDCC;
      auto QxtQyp = modqxZDCC * modqyZDCA;

      histos.fill(HIST("hpQxtQxpvscent"), centrality, QxtQxp);
      histos.fill(HIST("hpQytQypvscent"), centrality, QytQyp);
      histos.fill(HIST("hpQxytpvscent"), centrality, Qxytp);
      histos.fill(HIST("hpQxpQytvscent"), centrality, QxpQyt);
      histos.fill(HIST("hpQxtQypvscent"), centrality, QxtQyp);

      histos.fill(HIST("hpQxpvscent"), centrality, modqxZDCA);
      histos.fill(HIST("hpQxtvscent"), centrality, modqxZDCC);
      histos.fill(HIST("hpQypvscent"), centrality, modqyZDCA);
      histos.fill(HIST("hpQytvscent"), centrality, modqyZDCC);

      for (const auto& track : tracks) {
        if (!selectionTrack(track)) {
          continue;
        }

        float sign = track.sign();
        if (sign == 0.0) // removing neutral particles
          continue;

        auto ux = TMath::Cos(GetPhiInRange(track.phi()));
        auto uy = TMath::Sin(GetPhiInRange(track.phi()));
        // auto py=track.py();

        auto uxQxp = ux * modqxZDCA;
        auto uyQyp = uy * modqyZDCA;
        auto uxyQxyp = uxQxp + uyQyp;
        auto uxQxt = ux * modqxZDCC;
        auto uyQyt = uy * modqyZDCC;
        auto uxyQxyt = uxQxt + uyQyt;
        auto oddv1 = ux * (modqxZDCA - modqxZDCC) + uy * (modqyZDCA - modqyZDCC);
        auto evenv1 = ux * (modqxZDCA + modqxZDCC) + uy * (modqyZDCA + modqyZDCC);
        auto v21 = TMath::Cos(2 * (GetPhiInRange(track.phi()) - psiZDCA - psiZDCC));
        auto v22 = TMath::Cos(2 * (GetPhiInRange(track.phi()) + psiZDCA - psiZDCC));
        auto v23 = TMath::Cos(2 * (GetPhiInRange(track.phi()) - psiZDC));

        auto x2Tx1Ax1C = TMath::Cos(2 * GetPhiInRange(track.phi())) * modqxZDCA * modqxZDCC;
        auto x2Ty1Ay1C = TMath::Cos(2 * GetPhiInRange(track.phi())) * modqyZDCA * modqyZDCC;
        auto y2Tx1Ay1C = TMath::Sin(2 * GetPhiInRange(track.phi())) * modqxZDCA * modqyZDCC;
        auto y2Ty1Ax1C = TMath::Sin(2 * GetPhiInRange(track.phi())) * modqyZDCA * modqxZDCC;
        auto x1Ax1C = modqxZDCA * modqxZDCC;
        auto y1Ay1C = modqyZDCA * modqyZDCC;
        auto x1Ay1C = modqxZDCA * modqyZDCC;
        auto x1Cy1A = modqxZDCC * modqyZDCA;

        // detector acceptance corrections to match v2{ZDC}
        auto x1A = modqxZDCA;
        auto x1C = modqxZDCC;
        auto y1A = modqyZDCA;
        auto y1C = modqyZDCC;
        auto x2T = TMath::Cos(2 * GetPhiInRange(track.phi()));
        auto y2T = TMath::Sin(2 * GetPhiInRange(track.phi()));
        auto x2Tx1A = TMath::Cos(2 * GetPhiInRange(track.phi())) * modqxZDCA;
        auto x2Tx1C = TMath::Cos(2 * GetPhiInRange(track.phi())) * modqxZDCC;
        auto x2Ty1A = TMath::Cos(2 * GetPhiInRange(track.phi())) * modqyZDCA;
        auto x2Ty1C = TMath::Cos(2 * GetPhiInRange(track.phi())) * modqyZDCC;
        auto y2Tx1A = TMath::Sin(2 * GetPhiInRange(track.phi())) * modqxZDCA;
        auto y2Tx1C = TMath::Sin(2 * GetPhiInRange(track.phi())) * modqxZDCC;
        auto y2Ty1A = TMath::Sin(2 * GetPhiInRange(track.phi())) * modqyZDCA;
        auto y2Ty1C = TMath::Sin(2 * GetPhiInRange(track.phi())) * modqyZDCC;

        if (globalpt) {
          // if (sign > 0) {
          histos.fill(HIST("hpuxQxpvscentpteta"), centrality, track.pt(), track.eta(), uxQxp);
          histos.fill(HIST("hpuyQypvscentpteta"), centrality, track.pt(), track.eta(), uyQyp);
          histos.fill(HIST("hpuxQxtvscentpteta"), centrality, track.pt(), track.eta(), uxQxt);
          histos.fill(HIST("hpuyQytvscentpteta"), centrality, track.pt(), track.eta(), uyQyt);

          histos.fill(HIST("hpuxvscentpteta"), centrality, track.pt(), track.eta(), ux);
          histos.fill(HIST("hpuyvscentpteta"), centrality, track.pt(), track.eta(), uy);

          histos.fill(HIST("hpuxyQxytvscentpteta"), centrality, track.pt(), track.eta(), uxyQxyt);
          histos.fill(HIST("hpuxyQxypvscentpteta"), centrality, track.pt(), track.eta(), uxyQxyp);
          histos.fill(HIST("hpoddv1vscentpteta"), centrality, track.pt(), track.eta(), oddv1);
          histos.fill(HIST("hpevenv1vscentpteta"), centrality, track.pt(), track.eta(), evenv1);

          histos.fill(HIST("hpv21"), centrality, track.pt(), track.eta(), v21);
          histos.fill(HIST("hpv22"), centrality, track.pt(), track.eta(), v22);
          histos.fill(HIST("hpv23"), centrality, track.pt(), track.eta(), v23);

          histos.fill(HIST("hpx2Tx1Ax1Cvscentpteta"), centrality, track.pt(), track.eta(), x2Tx1Ax1C);
          histos.fill(HIST("hpx2Ty1Ay1Cvscentpteta"), centrality, track.pt(), track.eta(), x2Ty1Ay1C);
          histos.fill(HIST("hpy2Tx1Ay1Cvscentpteta"), centrality, track.pt(), track.eta(), y2Tx1Ay1C);
          histos.fill(HIST("hpy2Ty1Ax1Cvscentpteta"), centrality, track.pt(), track.eta(), y2Ty1Ax1C);
          histos.fill(HIST("hpx2Tvscentpteta"), centrality, track.pt(), track.eta(), x2T);
          histos.fill(HIST("hpy2Tvscentpteta"), centrality, track.pt(), track.eta(), y2T);
          histos.fill(HIST("hpx2Tx1Avscentpteta"), centrality, track.pt(), track.eta(), x2Tx1A);
          histos.fill(HIST("hpx2Tx1Cvscentpteta"), centrality, track.pt(), track.eta(), x2Tx1C);
          histos.fill(HIST("hpx2Ty1Avscentpteta"), centrality, track.pt(), track.eta(), x2Ty1A);
          histos.fill(HIST("hpx2Ty1Cvscentpteta"), centrality, track.pt(), track.eta(), x2Ty1C);
          histos.fill(HIST("hpy2Tx1Avscentpteta"), centrality, track.pt(), track.eta(), y2Tx1A);
          histos.fill(HIST("hpy2Ty1Cvscentpteta"), centrality, track.pt(), track.eta(), y2Ty1C);
          histos.fill(HIST("hpy2Ty1Avscentpteta"), centrality, track.pt(), track.eta(), y2Ty1A);
          histos.fill(HIST("hpy2Tx1Cvscentpteta"), centrality, track.pt(), track.eta(), y2Tx1C);
          histos.fill(HIST("hpx1Ax1Cvscentpteta"), centrality, track.pt(), track.eta(), x1Ax1C);
          histos.fill(HIST("hpy1Ay1Cvscentpteta"), centrality, track.pt(), track.eta(), y1Ay1C);
          histos.fill(HIST("hpx1Ay1Cvscentpteta"), centrality, track.pt(), track.eta(), x1Ay1C);
          histos.fill(HIST("hpy1Ax1Cvscentpteta"), centrality, track.pt(), track.eta(), x1Cy1A);
          histos.fill(HIST("hpx1Avscentpteta"), centrality, track.pt(), track.eta(), x1A);
          histos.fill(HIST("hpx1Cvscentpteta"), centrality, track.pt(), track.eta(), x1C);
          histos.fill(HIST("hpy1Avscentpteta"), centrality, track.pt(), track.eta(), y1A);
          histos.fill(HIST("hpy1Cvscentpteta"), centrality, track.pt(), track.eta(), y1C);

          /*} else {
            histos.fill(HIST("hpuxQxpvscentptetaneg"), centrality, track.pt(), track.eta(), uxQxp);
            histos.fill(HIST("hpuyQypvscentptetaneg"), centrality, track.pt(), track.eta(), uyQyp);
            histos.fill(HIST("hpuxQxtvscentptetaneg"), centrality, track.pt(), track.eta(), uxQxt);
            histos.fill(HIST("hpuyQytvscentptetaneg"), centrality, track.pt(), track.eta(), uyQyt);

            histos.fill(HIST("hpuxvscentptetaneg"), centrality, track.pt(), track.eta(), ux);
            histos.fill(HIST("hpuyvscentptetaneg"), centrality, track.pt(), track.eta(), uy);

            histos.fill(HIST("hpuxyQxytvscentptetaneg"), centrality, track.pt(), track.eta(), uxyQxyt);
            histos.fill(HIST("hpuxyQxypvscentptetaneg"), centrality, track.pt(), track.eta(), uxyQxyp);
            histos.fill(HIST("hpoddv1vscentptetaneg"), centrality, track.pt(), track.eta(), oddv1);
            histos.fill(HIST("hpevenv1vscentptetaneg"), centrality, track.pt(), track.eta(), evenv1);
            }*/
        } else {
          histos.fill(HIST("hpuxQxpvscentpteta"), centrality, track.tpcInnerParam(), track.eta(), uxQxp);
          histos.fill(HIST("hpuyQypvscentpteta"), centrality, track.tpcInnerParam(), track.eta(), uyQyp);
          histos.fill(HIST("hpuxQxtvscentpteta"), centrality, track.tpcInnerParam(), track.eta(), uxQxt);
          histos.fill(HIST("hpuyQytvscentpteta"), centrality, track.tpcInnerParam(), track.eta(), uyQyt);

          histos.fill(HIST("hpuxvscentpteta"), centrality, track.pt(), track.eta(), ux);
          histos.fill(HIST("hpuyvscentpteta"), centrality, track.pt(), track.eta(), uy);

          histos.fill(HIST("hpuxyQxytvscentpteta"), centrality, track.tpcInnerParam(), track.eta(), uxyQxyt);
          histos.fill(HIST("hpuxyQxypvscentpteta"), centrality, track.tpcInnerParam(), track.eta(), uxyQxyp);
          histos.fill(HIST("hpoddv1vscentpteta"), centrality, track.pt(), track.eta(), oddv1);
          histos.fill(HIST("hpevenv1vscentpteta"), centrality, track.pt(), track.eta(), evenv1);
        }
      }
    } else {
      for (const auto& v0 : V0s) {

        auto postrack = v0.template posTrack_as<AllTrackCandidates>();
        auto negtrack = v0.template negTrack_as<AllTrackCandidates>();

        int LambdaTag = 0;
        int aLambdaTag = 0;

        const auto signpos = postrack.sign();
        const auto signneg = negtrack.sign();

        if (signpos < 0 || signneg > 0) {
          continue;
        }

        if (isSelectedV0Daughter(v0, postrack, 0) && isSelectedV0Daughter(v0, negtrack, 1)) {
          LambdaTag = 1;
        }
        if (isSelectedV0Daughter(v0, negtrack, 0) && isSelectedV0Daughter(v0, postrack, 1)) {
          aLambdaTag = 1;
        }

        if (!LambdaTag && !aLambdaTag)
          continue;

        if (!SelectionV0(collision, v0)) {
          continue;
        }

        if (LambdaTag) {
          Proton = ROOT::Math::PxPyPzMVector(v0.pxpos(), v0.pypos(), v0.pzpos(), massPr);
          AntiPion = ROOT::Math::PxPyPzMVector(v0.pxneg(), v0.pyneg(), v0.pzneg(), massPi);
          Lambdadummy = Proton + AntiPion;
          // angleLambda = calculateAngleBetweenLorentzVectors(Proton, AntiPion);
        }
        if (aLambdaTag) {
          AntiProton = ROOT::Math::PxPyPzMVector(v0.pxneg(), v0.pyneg(), v0.pzneg(), massPr);
          Pion = ROOT::Math::PxPyPzMVector(v0.pxpos(), v0.pypos(), v0.pzpos(), massPi);
          AntiLambdadummy = AntiProton + Pion;
          // angleAntiLambda = calculateAngleBetweenLorentzVectors(AntiProton, Pion);
        }

        if (shouldReject(LambdaTag, aLambdaTag, Lambdadummy, AntiLambdadummy)) {
          continue;
        }

        if (TMath::Abs(v0.eta()) > 0.8)
          continue;

        int taga = LambdaTag;
        int tagb = aLambdaTag;

        // if (useAccCorr && (currentRunNumber != lastRunNumber)) {
        if (useAccCorr) {
          accprofileL = ccdb->getForTimeStamp<TProfile2D>(ConfAccPathL.value, bc.timestamp());
          accprofileAL = ccdb->getForTimeStamp<TProfile2D>(ConfAccPathAL.value, bc.timestamp());
        }

        float desbinvalue = 0.0;
        if (dosystematic) {
          ////////////////////////////////////////////////////
          float LTsys = TMath::Abs(v0.distovertotmom(collision.posX(), collision.posY(), collision.posZ()) * massLambda);
          float CPAsys = v0.v0cosPA();
          float DCADaughsys = TMath::Abs(v0.dcaV0daughters());
          float DCApossys = TMath::Abs(v0.dcapostopv());
          float DCAnegsys = TMath::Abs(v0.dcanegtopv());
          float sysvar = -999.9;
          double syst[10];
          if (sys == 1) {
            double temp[10] = {26, 27, 28, 29, 30, 31, 32, 33, 34, 35};
            std::copy(std::begin(temp), std::end(temp), std::begin(syst));
            sysvar = LTsys;
          }
          if (sys == 2) {
            double temp[10] = {0.992, 0.993, 0.9935, 0.994, 0.9945, 0.995, 0.9955, 0.996, 0.9965, 0.997};
            std::copy(std::begin(temp), std::end(temp), std::begin(syst));
            sysvar = CPAsys;
          }
          if (sys == 3) {
            double temp[10] = {0.8, 0.85, 0.9, 0.95, 1.0, 1.05, 1.1, 1.15, 1.2, 1.25};
            std::copy(std::begin(temp), std::end(temp), std::begin(syst));
            sysvar = DCADaughsys;
          }
          if (sys == 4) {
            double temp[10] = {0.05, 0.07, 0.1, 0.15, 0.18, 0.2, 0.22, 0.25, 0.28, 0.3};
            std::copy(std::begin(temp), std::end(temp), std::begin(syst));
            sysvar = DCApossys;
          }
          if (sys == 5) {
            double temp[10] = {0.05, 0.07, 0.1, 0.15, 0.18, 0.2, 0.22, 0.25, 0.28, 0.3};
            std::copy(std::begin(temp), std::end(temp), std::begin(syst));
            sysvar = DCAnegsys;
          }

          for (int i = 0; i < 10; i++) {
            if (sys == 1 || sys == 3) {
              if (sysvar < syst[i])
                desbinvalue = i + 0.5;
              else
                continue;
            }
            if (sys == 2 || sys == 4 || sys == 5) {
              if (sysvar > syst[i])
                desbinvalue = i + 0.5;
              else
                continue;
            }

            ///////////////////////////////////////////////////
            if (LambdaTag) {
              Lambda = Proton + AntiPion;
              tagb = 0;
              int binx = accprofileL->GetXaxis()->FindBin(v0.eta());
              int biny = accprofileL->GetYaxis()->FindBin(v0.pt());
              double acvalue = accprofileL->GetBinContent(binx, biny);
              fillHistograms(taga, tagb, Lambda, Proton, psiZDCC, psiZDCA, psiZDC, centrality, v0.mLambda(), v0.pt(), desbinvalue, acvalue);
            }

            tagb = aLambdaTag;
            if (aLambdaTag) {
              AntiLambda = AntiProton + Pion;
              taga = 0;
              int binx = accprofileAL->GetXaxis()->FindBin(v0.eta());
              int biny = accprofileAL->GetYaxis()->FindBin(v0.pt());
              double acvalue = accprofileAL->GetBinContent(binx, biny);
              fillHistograms(taga, tagb, AntiLambda, AntiProton, psiZDCC, psiZDCA, psiZDC, centrality, v0.mAntiLambda(), v0.pt(), desbinvalue, acvalue);
            }
          }
        } else {
          if (LambdaTag) {
            Lambda = Proton + AntiPion;
            tagb = 0;
            int binx = accprofileL->GetXaxis()->FindBin(v0.eta());
            int biny = accprofileL->GetYaxis()->FindBin(v0.pt());
            double acvalue = accprofileL->GetBinContent(binx, biny);
            fillHistograms(taga, tagb, Lambda, Proton, psiZDCC, psiZDCA, psiZDC, centrality, v0.mLambda(), v0.pt(), v0.eta(), acvalue);
          }

          tagb = aLambdaTag;
          if (aLambdaTag) {
            AntiLambda = AntiProton + Pion;
            taga = 0;
            int binx = accprofileAL->GetXaxis()->FindBin(v0.eta());
            int biny = accprofileAL->GetYaxis()->FindBin(v0.pt());
            double acvalue = accprofileAL->GetBinContent(binx, biny);
            fillHistograms(taga, tagb, AntiLambda, AntiProton, psiZDCC, psiZDCA, psiZDC, centrality, v0.mAntiLambda(), v0.pt(), v0.eta(), acvalue);
          }
        }
      }
    }
    // lastRunNumber = currentRunNumber;
  }
  PROCESS_SWITCH(lambdapolsp, processData, "Process data", true);

  // process function for derived data - mimics the functionality of the original data
  void processDerivedData(soa::Join<aod::StraCollisions, aod::StraCents, aod::StraEvSels, aod::StraStamps, aod::StraZDCSP>::iterator const& collision, v0Candidates const& V0s, dauTracks const&)
  {
    //___________________________________________________________________________________________________
    // event selection
    if (!collision.sel8()) {
      return;
    }
    auto centrality = collision.centFT0C();
    if (!collision.triggereventsp()) { // provided by StraZDCSP
      return;
    }

    if (rctCut.requireRCTFlagChecker && !rctChecker(collision)) {
      return;
    }

    if (additionalEvSel && (!collision.selection_bit(aod::evsel::kNoSameBunchPileup) || !collision.selection_bit(aod::evsel::kIsGoodZvtxFT0vsPV))) {
      return;
    }
    // histos.fill(HIST("hCentrality2"), centrality);
    //  if (additionalEvSel2 && (!collision.selection_bit(o2::aod::evsel::kNoCollInTimeRangeStandard))) {
    if (additionalEvSel2 && (collision.trackOccupancyInTimeRange() > cfgMaxOccupancy || collision.trackOccupancyInTimeRange() < cfgMinOccupancy)) {
      return;
    }
    // histos.fill(HIST("hCentrality3"), centrality);
    if (additionalEvSel3 && (!collision.selection_bit(aod::evsel::kNoTimeFrameBorder) || !collision.selection_bit(aod::evsel::kNoITSROFrameBorder))) {
      return;
    }

    if (additionalEvSel4 && !collision.selection_bit(o2::aod::evsel::kIsGoodITSLayersAll)) {
      return;
    }

    /*currentRunNumber = collision.foundBC_as<BCsRun3>().runNumber();
    auto bc = collision.foundBC_as<BCsRun3>();

    if (useAccCorr && (currentRunNumber != lastRunNumber)) {
      accprofileL = ccdb->getForTimeStamp<TProfile2D>(ConfAccPathL.value, bc.timestamp());
      accprofileAL = ccdb->getForTimeStamp<TProfile2D>(ConfAccPathAL.value, bc.timestamp());
    }
    */
    //___________________________________________________________________________________________________
    // retrieve further info provided by StraZDCSP
    auto qxZDCA = collision.qxZDCA();
    auto qxZDCC = collision.qxZDCC();
    auto qyZDCA = collision.qyZDCA();
    auto qyZDCC = collision.qyZDCC();
    auto psiZDCC = collision.psiZDCC();
    auto psiZDCA = collision.psiZDCA();
    double modqxZDCA;
    double modqyZDCA;
    double modqxZDCC;
    double modqyZDCC;

    if (cqvas) {
      modqxZDCA = TMath::Sqrt((qxZDCA * qxZDCA) + (qyZDCA * qyZDCA)) * TMath::Cos(psiZDCA);
      modqyZDCA = TMath::Sqrt((qxZDCA * qxZDCA) + (qyZDCA * qyZDCA)) * TMath::Sin(psiZDCA);
      modqxZDCC = TMath::Sqrt((qxZDCC * qxZDCC) + (qyZDCC * qyZDCC)) * TMath::Cos(psiZDCC);
      modqyZDCC = TMath::Sqrt((qxZDCC * qxZDCC) + (qyZDCC * qyZDCC)) * TMath::Sin(psiZDCC);
    } else {
      modqxZDCA = qxZDCA;
      modqyZDCA = qyZDCA;
      modqxZDCC = qxZDCC;
      modqyZDCC = qyZDCC;
    }

    auto psiZDC = TMath::ATan2((modqyZDCC - modqyZDCA), (modqxZDCC - modqxZDCA)); // full event plane

    // fill histograms
    histos.fill(HIST("hCentrality"), centrality);
    if (!checkwithpub) {
      // histos.fill(HIST("hVtxZ"), collision.posZ());
      histos.fill(HIST("hpRes"), centrality, (TMath::Cos(GetPhiInRange(psiZDCA - psiZDCC))));
      histos.fill(HIST("hpResSin"), centrality, (TMath::Sin(GetPhiInRange(psiZDCA - psiZDCC))));
      /*histos.fill(HIST("hpCosPsiA"), centrality, (TMath::Cos(GetPhiInRange(psiZDCA))));
      histos.fill(HIST("hpCosPsiC"), centrality, (TMath::Cos(GetPhiInRange(psiZDCC))));
      histos.fill(HIST("hpSinPsiA"), centrality, (TMath::Sin(GetPhiInRange(psiZDCA))));
      histos.fill(HIST("hpSinPsiC"), centrality, (TMath::Sin(GetPhiInRange(psiZDCC))));*/
    }

    //___________________________________________________________________________________________________
    // loop over V0s as necessary
    for (const auto& v0 : V0s) {

      if (analyzeLambda && analyzeK0s)
        continue;
      if (!analyzeLambda && !analyzeK0s)
        continue;

      bool LambdaTag = isCompatible(v0, 0);
      bool aLambdaTag = isCompatible(v0, 1);

      bool K0sTag = isCompatibleK0s(v0);

      if (analyzeLambda && !LambdaTag && !aLambdaTag)
        continue;

      if (analyzeK0s && !K0sTag)
        continue;

      if (!SelectionV0(collision, v0)) {
        continue;
      }
      if (analyzeLambda) {
        if (LambdaTag) {
          Proton = ROOT::Math::PxPyPzMVector(v0.pxpos(), v0.pypos(), v0.pzpos(), massPr);
          AntiPion = ROOT::Math::PxPyPzMVector(v0.pxneg(), v0.pyneg(), v0.pzneg(), massPi);
          Lambdadummy = Proton + AntiPion;
          // angleLambda = calculateAngleBetweenLorentzVectors(Proton, AntiPion);
        }
        if (aLambdaTag) {
          AntiProton = ROOT::Math::PxPyPzMVector(v0.pxneg(), v0.pyneg(), v0.pzneg(), massPr);
          Pion = ROOT::Math::PxPyPzMVector(v0.pxpos(), v0.pypos(), v0.pzpos(), massPi);
          AntiLambdadummy = AntiProton + Pion;
          // angleAntiLambda = calculateAngleBetweenLorentzVectors(AntiProton, Pion);
        }

        if (shouldReject(LambdaTag, aLambdaTag, Lambdadummy, AntiLambdadummy)) {
          continue;
        }
      }

      if (analyzeK0s) {
        if (K0sTag) {
          Pion = ROOT::Math::PxPyPzMVector(v0.pxpos(), v0.pypos(), v0.pzpos(), massPi);
          AntiPion = ROOT::Math::PxPyPzMVector(v0.pxneg(), v0.pyneg(), v0.pzneg(), massPi);
          K0sdummy = Pion + AntiPion;
        }
      }

      if (TMath::Abs(v0.eta()) > 0.8)
        continue;

      int taga = LambdaTag;
      int tagb = aLambdaTag;
      int tagc = K0sTag;

      float desbinvalue = 0.0;

      if (analyzeK0s && K0sTag) {
        K0s = Pion + AntiPion;
        double acvalue = 1.0;
        fillHistograms(tagc, 0, K0s, Pion, psiZDCC, psiZDCA, psiZDC, centrality, v0.mK0Short(), v0.pt(), v0.eta(), acvalue);
      }

      if (analyzeLambda && dosystematic) {
        ////////////////////////////////////////////////////
        float LTsys = TMath::Abs(v0.distovertotmom(collision.posX(), collision.posY(), collision.posZ()) * massLambda);
        float CPAsys = v0.v0cosPA();
        float DCADaughsys = TMath::Abs(v0.dcaV0daughters());
        float DCApossys = TMath::Abs(v0.dcapostopv());
        float DCAnegsys = TMath::Abs(v0.dcanegtopv());
        float sysvar = -999.9;
        double syst[10];
        if (sys == 1) {
          double temp[10] = {26, 27, 28, 29, 30, 31, 32, 33, 34, 35};
          std::copy(std::begin(temp), std::end(temp), std::begin(syst));
          sysvar = LTsys;
        }
        if (sys == 2) {
          double temp[10] = {0.992, 0.993, 0.9935, 0.994, 0.9945, 0.995, 0.9955, 0.996, 0.9965, 0.997};
          std::copy(std::begin(temp), std::end(temp), std::begin(syst));
          sysvar = CPAsys;
        }
        if (sys == 3) {
          double temp[10] = {0.8, 0.85, 0.9, 0.95, 1.0, 1.05, 1.1, 1.15, 1.2, 1.25};
          std::copy(std::begin(temp), std::end(temp), std::begin(syst));
          sysvar = DCADaughsys;
        }
        if (sys == 4) {
          double temp[10] = {0.05, 0.07, 0.1, 0.15, 0.18, 0.2, 0.22, 0.25, 0.28, 0.3};
          std::copy(std::begin(temp), std::end(temp), std::begin(syst));
          sysvar = DCApossys;
        }
        if (sys == 5) {
          double temp[10] = {0.05, 0.07, 0.1, 0.15, 0.18, 0.2, 0.22, 0.25, 0.28, 0.3};
          std::copy(std::begin(temp), std::end(temp), std::begin(syst));
          sysvar = DCAnegsys;
        }

        for (int i = 0; i < 10; i++) {
          if (sys == 1 || sys == 3) {
            if (sysvar < syst[i])
              desbinvalue = i + 0.5;
            else
              continue;
          }
          if (sys == 2 || sys == 4 || sys == 5) {
            if (sysvar > syst[i])
              desbinvalue = i + 0.5;
            else
              continue;
          }

          ///////////////////////////////////////////////////
          if (analyzeLambda && LambdaTag) {
            Lambda = Proton + AntiPion;
            tagb = 0;
            double acvalue = 1.0;
            fillHistograms(taga, tagb, Lambda, Proton, psiZDCC, psiZDCA, psiZDC, centrality, v0.mLambda(), v0.pt(), desbinvalue, acvalue);
          }

          tagb = aLambdaTag;
          if (analyzeLambda && aLambdaTag) {
            AntiLambda = AntiProton + Pion;
            taga = 0;
            double acvalue = 1.0;
            fillHistograms(taga, tagb, AntiLambda, AntiProton, psiZDCC, psiZDCA, psiZDC, centrality, v0.mAntiLambda(), v0.pt(), desbinvalue, acvalue);
          }
        }
      } else {
        if (analyzeLambda && LambdaTag) {
          Lambda = Proton + AntiPion;
          tagb = 0;
          double acvalue = 1.0;
          fillHistograms(taga, tagb, Lambda, Proton, psiZDCC, psiZDCA, psiZDC, centrality, v0.mLambda(), v0.pt(), v0.eta(), acvalue);
        }

        tagb = aLambdaTag;
        if (analyzeLambda && aLambdaTag) {
          AntiLambda = AntiProton + Pion;
          taga = 0;
          double acvalue = 1.0;
          fillHistograms(taga, tagb, AntiLambda, AntiProton, psiZDCC, psiZDCA, psiZDC, centrality, v0.mAntiLambda(), v0.pt(), v0.eta(), acvalue);
        }
      }
    }
    // lastRunNumber = currentRunNumber;
  }
  PROCESS_SWITCH(lambdapolsp, processDerivedData, "Process derived data", false);

  // Processing Event Mixing
  using BinningType = ColumnBinningPolicy<aod::collision::PosZ, aod::cent::CentFT0C>;
  BinningType colBinning{{meGrp.axisVertex, meGrp.axisMultiplicityClass}, true};
  Preslice<v0Candidates> tracksPerCollisionV0Mixed = o2::aod::v0data::straCollisionId; // for derived data only

  void processDerivedDataMixed(soa::Join<aod::StraCollisions, aod::StraCents, aod::StraEvSels, aod::StraStamps, aod::StraZDCSP> const& collisions, v0Candidates const& V0s, dauTracks const&)
  {
    TRandom3 randGen(0);

    for (auto& [collision1, collision2] : selfCombinations(colBinning, meGrp.nMix, -1, collisions, collisions)) {

      if (collision1.index() == collision2.index()) {
        continue;
      }

      if (!collision1.sel8()) {
        continue;
      }
      if (!collision2.sel8()) {
        continue;
      }

      if (!collision1.triggereventsp()) { // provided by StraZDCSP
        continue;
      }
      if (!collision2.triggereventsp()) { // provided by StraZDCSP
        continue;
      }

      if (rctCut.requireRCTFlagChecker && !rctChecker(collision1)) {
        continue;
      }
      if (rctCut.requireRCTFlagChecker && !rctChecker(collision2)) {
        continue;
      }

      if (additionalEvSel && (!collision1.selection_bit(aod::evsel::kNoSameBunchPileup) || !collision1.selection_bit(aod::evsel::kIsGoodZvtxFT0vsPV))) {
        continue;
      }
      if (additionalEvSel && (!collision2.selection_bit(aod::evsel::kNoSameBunchPileup) || !collision2.selection_bit(aod::evsel::kIsGoodZvtxFT0vsPV))) {
        continue;
      }
      if (additionalEvSel2 && (collision1.trackOccupancyInTimeRange() > cfgMaxOccupancy || collision1.trackOccupancyInTimeRange() < cfgMinOccupancy)) {
        continue;
      }
      if (additionalEvSel2 && (collision2.trackOccupancyInTimeRange() > cfgMaxOccupancy || collision2.trackOccupancyInTimeRange() < cfgMinOccupancy)) {
        continue;
      }
      if (additionalEvSel3 && (!collision1.selection_bit(aod::evsel::kNoTimeFrameBorder) || !collision1.selection_bit(aod::evsel::kNoITSROFrameBorder))) {
        continue;
      }
      if (additionalEvSel3 && (!collision2.selection_bit(aod::evsel::kNoTimeFrameBorder) || !collision2.selection_bit(aod::evsel::kNoITSROFrameBorder))) {
        continue;
      }
      if (additionalEvSel4 && !collision1.selection_bit(o2::aod::evsel::kIsGoodITSLayersAll)) {
        continue;
      }
      if (additionalEvSel4 && !collision2.selection_bit(o2::aod::evsel::kIsGoodITSLayersAll)) {
        continue;
      }

      auto centrality = collision1.centFT0C();
      auto qxZDCA = collision2.qxZDCA();
      auto qxZDCC = collision2.qxZDCC();
      auto qyZDCA = collision2.qyZDCA();
      auto qyZDCC = collision2.qyZDCC();
      auto psiZDCC = collision2.psiZDCC();
      auto psiZDCA = collision2.psiZDCA();
      double modqxZDCA;
      double modqyZDCA;
      double modqxZDCC;
      double modqyZDCC;

      modqxZDCA = TMath::Sqrt((qxZDCA * qxZDCA) + (qyZDCA * qyZDCA)) * TMath::Cos(psiZDCA);
      modqyZDCA = TMath::Sqrt((qxZDCA * qxZDCA) + (qyZDCA * qyZDCA)) * TMath::Sin(psiZDCA);
      modqxZDCC = TMath::Sqrt((qxZDCC * qxZDCC) + (qyZDCC * qyZDCC)) * TMath::Cos(psiZDCC);
      modqyZDCC = TMath::Sqrt((qxZDCC * qxZDCC) + (qyZDCC * qyZDCC)) * TMath::Sin(psiZDCC);

      auto psiZDC = TMath::ATan2((modqyZDCC - modqyZDCA), (modqxZDCC - modqxZDCA)); // full event plane from collision 2
      auto groupV0 = V0s.sliceBy(tracksPerCollisionV0Mixed, collision1.index());

      histos.fill(HIST("hCentrality"), centrality);

      if (randGrp.doRandomPsi) {
        psiZDC = randGen.Uniform(0, 2 * TMath::Pi());
      }
      if (randGrp.doRandomPsiAC) {
        psiZDCA = randGen.Uniform(0, 2 * TMath::Pi());
        psiZDCC = randGen.Uniform(0, 2 * TMath::Pi());
      }

      histos.fill(HIST("hpRes"), centrality, (TMath::Cos(GetPhiInRange(psiZDCA - psiZDCC))));
      histos.fill(HIST("hpResSin"), centrality, (TMath::Sin(GetPhiInRange(psiZDCA - psiZDCC))));

      for (const auto& v0 : groupV0) {

        bool LambdaTag = isCompatible(v0, 0);
        bool aLambdaTag = isCompatible(v0, 1);
        if (!LambdaTag && !aLambdaTag)
          continue;
        if (!SelectionV0(collision1, v0))
          continue;
        if (LambdaTag) {
          Proton = ROOT::Math::PxPyPzMVector(v0.pxpos(), v0.pypos(), v0.pzpos(), massPr);
          AntiPion = ROOT::Math::PxPyPzMVector(v0.pxneg(), v0.pyneg(), v0.pzneg(), massPi);
          Lambdadummy = Proton + AntiPion;
        }
        if (aLambdaTag) {
          AntiProton = ROOT::Math::PxPyPzMVector(v0.pxneg(), v0.pyneg(), v0.pzneg(), massPr);
          Pion = ROOT::Math::PxPyPzMVector(v0.pxpos(), v0.pypos(), v0.pzpos(), massPi);
          AntiLambdadummy = AntiProton + Pion;
        }
        if (shouldReject(LambdaTag, aLambdaTag, Lambdadummy, AntiLambdadummy)) {
          continue;
        }
        if (TMath::Abs(v0.eta()) > 0.8)
          continue;
        int taga = LambdaTag;
        int tagb = aLambdaTag;

        if (LambdaTag) {
          Lambda = Proton + AntiPion;
          tagb = 0;
          double acvalue = 1.0;
          fillHistograms(taga, tagb, Lambda, Proton, psiZDCC, psiZDCA, psiZDC, centrality, v0.mLambda(), v0.pt(), v0.eta(), acvalue);
        }

        tagb = aLambdaTag;
        if (aLambdaTag) {
          AntiLambda = AntiProton + Pion;
          taga = 0;
          double acvalue = 1.0;
          fillHistograms(taga, tagb, AntiLambda, AntiProton, psiZDCC, psiZDCA, psiZDC, centrality, v0.mAntiLambda(), v0.pt(), v0.eta(), acvalue);
        }
      }
    }
  }
  PROCESS_SWITCH(lambdapolsp, processDerivedDataMixed, "Process mixed event using derived data", false);

  void processDerivedDataMixed2(soa::Join<aod::StraCollisions, aod::StraCents, aod::StraEvSels, aod::StraStamps, aod::StraZDCSP> const& collisions, v0Candidates const& V0s, dauTracks const&)
  {
    TRandom3 randGen(0);

    for (auto& [collision1, collision2] : selfCombinations(colBinning, meGrp.nMix, -1, collisions, collisions)) {

      if (collision1.index() == collision2.index()) {
        continue;
      }

      if (!collision1.sel8()) {
        continue;
      }
      if (!collision2.sel8()) {
        continue;
      }

      if (!collision1.triggereventsp()) { // provided by StraZDCSP
        continue;
      }
      if (!collision2.triggereventsp()) { // provided by StraZDCSP
        continue;
      }

      if (rctCut.requireRCTFlagChecker && !rctChecker(collision1)) {
        continue;
      }
      if (rctCut.requireRCTFlagChecker && !rctChecker(collision2)) {
        continue;
      }

      if (additionalEvSel && (!collision1.selection_bit(aod::evsel::kNoSameBunchPileup) || !collision1.selection_bit(aod::evsel::kIsGoodZvtxFT0vsPV))) {
        continue;
      }
      if (additionalEvSel && (!collision2.selection_bit(aod::evsel::kNoSameBunchPileup) || !collision2.selection_bit(aod::evsel::kIsGoodZvtxFT0vsPV))) {
        continue;
      }
      if (additionalEvSel2 && (collision1.trackOccupancyInTimeRange() > cfgMaxOccupancy || collision1.trackOccupancyInTimeRange() < cfgMinOccupancy)) {
        continue;
      }
      if (additionalEvSel2 && (collision2.trackOccupancyInTimeRange() > cfgMaxOccupancy || collision2.trackOccupancyInTimeRange() < cfgMinOccupancy)) {
        continue;
      }
      if (additionalEvSel3 && (!collision1.selection_bit(aod::evsel::kNoTimeFrameBorder) || !collision1.selection_bit(aod::evsel::kNoITSROFrameBorder))) {
        continue;
      }
      if (additionalEvSel3 && (!collision2.selection_bit(aod::evsel::kNoTimeFrameBorder) || !collision2.selection_bit(aod::evsel::kNoITSROFrameBorder))) {
        continue;
      }
      if (additionalEvSel4 && !collision1.selection_bit(o2::aod::evsel::kIsGoodITSLayersAll)) {
        continue;
      }
      if (additionalEvSel4 && !collision2.selection_bit(o2::aod::evsel::kIsGoodITSLayersAll)) {
        continue;
      }

      auto centrality = collision1.centFT0C();
      auto qxZDCA = collision1.qxZDCA();
      auto qxZDCC = collision1.qxZDCC();
      auto qyZDCA = collision1.qyZDCA();
      auto qyZDCC = collision1.qyZDCC();
      auto psiZDCC = collision1.psiZDCC();
      auto psiZDCA = collision1.psiZDCA();
      double modqxZDCA;
      double modqyZDCA;
      double modqxZDCC;
      double modqyZDCC;

      modqxZDCA = TMath::Sqrt((qxZDCA * qxZDCA) + (qyZDCA * qyZDCA)) * TMath::Cos(psiZDCA);
      modqyZDCA = TMath::Sqrt((qxZDCA * qxZDCA) + (qyZDCA * qyZDCA)) * TMath::Sin(psiZDCA);
      modqxZDCC = TMath::Sqrt((qxZDCC * qxZDCC) + (qyZDCC * qyZDCC)) * TMath::Cos(psiZDCC);
      modqyZDCC = TMath::Sqrt((qxZDCC * qxZDCC) + (qyZDCC * qyZDCC)) * TMath::Sin(psiZDCC);

      auto psiZDC = TMath::ATan2((modqyZDCC - modqyZDCA), (modqxZDCC - modqxZDCA)); // full event plane from collision 2

      histos.fill(HIST("hCentrality"), centrality);
      histos.fill(HIST("hpRes"), centrality, (TMath::Cos(GetPhiInRange(psiZDCA - psiZDCC))));
      histos.fill(HIST("hpResSin"), centrality, (TMath::Sin(GetPhiInRange(psiZDCA - psiZDCC))));

      // V0s from collision1 to match kinematics
      auto v0sCol1 = V0s.sliceBy(tracksPerCollisionV0Mixed, collision1.index());
      // V0s from collision2 to test
      auto v0sCol2 = V0s.sliceBy(tracksPerCollisionV0Mixed, collision2.index());

      for (const auto& v0_2 : v0sCol2) {

        bool LambdaTag = isCompatible(v0_2, 0);
        bool aLambdaTag = isCompatible(v0_2, 1);
        if (!LambdaTag && !aLambdaTag)
          continue;
        if (!SelectionV0(collision2, v0_2))
          continue;
        if (LambdaTag) {
          Proton = ROOT::Math::PxPyPzMVector(v0_2.pxpos(), v0_2.pypos(), v0_2.pzpos(), massPr);
          AntiPion = ROOT::Math::PxPyPzMVector(v0_2.pxneg(), v0_2.pyneg(), v0_2.pzneg(), massPi);
          Lambdadummy = Proton + AntiPion;
        }
        if (aLambdaTag) {
          AntiProton = ROOT::Math::PxPyPzMVector(v0_2.pxneg(), v0_2.pyneg(), v0_2.pzneg(), massPr);
          Pion = ROOT::Math::PxPyPzMVector(v0_2.pxpos(), v0_2.pypos(), v0_2.pzpos(), massPi);
          AntiLambdadummy = AntiProton + Pion;
        }
        if (shouldReject(LambdaTag, aLambdaTag, Lambdadummy, AntiLambdadummy)) {
          continue;
        }
        if (TMath::Abs(v0_2.eta()) > 0.8)
          continue;

        // Check if lambda kinematics from collision2 matches with collision1
        bool matched = false;
        for (const auto& v0_1 : v0sCol1) {
          bool LambdaTag1 = isCompatible(v0_1, 0);
          bool aLambdaTag1 = isCompatible(v0_1, 1);
          if (!LambdaTag1 && !aLambdaTag1)
            continue;
          if (!SelectionV0(collision1, v0_1))
            continue;
          if (TMath::Abs(v0_1.eta()) > 0.8)
            continue;

          double deta = std::abs(v0_1.eta() - v0_2.eta());
          double dpt = std::abs(v0_1.pt() - v0_2.pt());
          double dphi = RecoDecay::constrainAngle(v0_1.phi() - v0_2.phi(), 0.0);
          if (deta < randGrp.etaMix && dpt < randGrp.ptMix && dphi < randGrp.phiMix && ((v0_1.eta() * v0_2.eta()) > 0.0)) {
            matched = true;
            break;
          }
        }
        if (!matched)
          continue;

        int taga = LambdaTag;
        int tagb = aLambdaTag;

        if (LambdaTag) {
          Lambda = Proton + AntiPion;
          tagb = 0;
          double acvalue = 1.0;
          fillHistograms(taga, tagb, Lambda, Proton, psiZDCC, psiZDCA, psiZDC, centrality, v0_2.mLambda(), v0_2.pt(), v0_2.eta(), acvalue);
        }

        tagb = aLambdaTag;
        if (aLambdaTag) {
          AntiLambda = AntiProton + Pion;
          taga = 0;
          double acvalue = 1.0;
          fillHistograms(taga, tagb, AntiLambda, AntiProton, psiZDCC, psiZDCA, psiZDC, centrality, v0_2.mAntiLambda(), v0_2.pt(), v0_2.eta(), acvalue);
        }
      }
    }
  }
  PROCESS_SWITCH(lambdapolsp, processDerivedDataMixed2, "Process mixed event2 using derived data", false);

  void processDerivedDataMixedFIFO(soa::Join<aod::StraCollisions, aod::StraCents, aod::StraEvSels, aod::StraStamps, aod::StraZDCSP> const& collisions, v0Candidates const& V0s, dauTracks const&)
  {

    auto nBins = colBinning.getAllBinsCount();
    std::vector<std::deque<int>> eventPools(nBins); // Pool per bin holding just event indices

    for (auto& collision1 : collisions) {

      if (!collision1.sel8()) {
        continue;
      }
      if (!collision1.triggereventsp()) { // provided by StraZDCSP
        continue;
      }
      if (rctCut.requireRCTFlagChecker && !rctChecker(collision1)) {
        continue;
      }

      if (additionalEvSel && (!collision1.selection_bit(aod::evsel::kNoSameBunchPileup) || !collision1.selection_bit(aod::evsel::kIsGoodZvtxFT0vsPV))) {
        continue;
      }
      if (additionalEvSel2 && (collision1.trackOccupancyInTimeRange() > cfgMaxOccupancy || collision1.trackOccupancyInTimeRange() < cfgMinOccupancy)) {
        continue;
      }
      if (additionalEvSel3 && (!collision1.selection_bit(aod::evsel::kNoTimeFrameBorder) || !collision1.selection_bit(aod::evsel::kNoITSROFrameBorder))) {
        continue;
      }
      if (additionalEvSel4 && !collision1.selection_bit(o2::aod::evsel::kIsGoodITSLayersAll)) {
        continue;
      }

      int bin = colBinning.getBin(std::make_tuple(collision1.posZ(), collision1.centFT0C()));
      auto groupV0_evt1 = V0s.sliceBy(tracksPerCollisionV0Mixed, collision1.index());
      float centrality = collision1.centFT0C();
      auto qxZDCA = collision1.qxZDCA();
      auto qxZDCC = collision1.qxZDCC();
      auto qyZDCA = collision1.qyZDCA();
      auto qyZDCC = collision1.qyZDCC();
      auto psiZDCC = collision1.psiZDCC();
      auto psiZDCA = collision1.psiZDCA();
      double modqxZDCA;
      double modqyZDCA;
      double modqxZDCC;
      double modqyZDCC;

      if (bin < 0)
        continue;
      modqxZDCA = TMath::Sqrt((qxZDCA * qxZDCA) + (qyZDCA * qyZDCA)) * TMath::Cos(psiZDCA);
      modqyZDCA = TMath::Sqrt((qxZDCA * qxZDCA) + (qyZDCA * qyZDCA)) * TMath::Sin(psiZDCA);
      modqxZDCC = TMath::Sqrt((qxZDCC * qxZDCC) + (qyZDCC * qyZDCC)) * TMath::Cos(psiZDCC);
      modqyZDCC = TMath::Sqrt((qxZDCC * qxZDCC) + (qyZDCC * qyZDCC)) * TMath::Sin(psiZDCC);

      auto psiZDC = TMath::ATan2((modqyZDCC - modqyZDCA), (modqxZDCC - modqxZDCA)); // full event plane from collision

      histos.fill(HIST("hCentrality"), centrality);
      histos.fill(HIST("hpRes"), centrality, (TMath::Cos(GetPhiInRange(psiZDCA - psiZDCC))));

      // For deduplication of (v0_evt1, v0_evt2) pairs per mixed event
      std::unordered_map<int, std::set<std::pair<int, int>>> seenMap;

      // Loop over Λ candidates in collision1 (keep psi from here)

      for (auto& v0_evt1 : groupV0_evt1) {
        if (!SelectionV0(collision1, v0_evt1))
          continue;
        bool LambdaTag1 = isCompatible(v0_evt1, 0);
        bool aLambdaTag1 = isCompatible(v0_evt1, 1);
        ROOT::Math::PxPyPzMVector proton1, pion1, antiproton1, antipion1, LambdaTag1dummy, AntiLambdaTag1dummy;
        if (LambdaTag1) {
          proton1 = {v0_evt1.pxpos(), v0_evt1.pypos(), v0_evt1.pzpos(), massPr};
          antipion1 = {v0_evt1.pxneg(), v0_evt1.pyneg(), v0_evt1.pzneg(), massPi};
          LambdaTag1dummy = proton1 + antipion1;
        }
        if (aLambdaTag1) {
          antiproton1 = {v0_evt1.pxneg(), v0_evt1.pyneg(), v0_evt1.pzneg(), massPr};
          pion1 = {v0_evt1.pxpos(), v0_evt1.pypos(), v0_evt1.pzpos(), massPi};
          AntiLambdaTag1dummy = antiproton1 + pion1;
        }
        if (shouldReject(LambdaTag1, aLambdaTag1, LambdaTag1dummy, AntiLambdaTag1dummy)) {
          continue;
        }
        if (TMath::Abs(v0_evt1.eta()) > 0.8)
          continue;

        // Loop over all FIFO pool events (mixed events) for this centrality bin
        int nMixedEvents = 0;
        for (auto it = eventPools[bin].rbegin(); it != eventPools[bin].rend() && nMixedEvents < meGrp.nMix; ++it, ++nMixedEvents) {
          int collision2idx = *it;
          if (collision1.index() == collision2idx)
            continue;
          auto groupV0_evt2 = V0s.sliceBy(tracksPerCollisionV0Mixed, collision2idx);

          // Now loop over Λ candidates in collision2 to randomize proton phi* (randomize decay angle)
          for (auto& v0_evt2 : groupV0_evt2) {
            if (!SelectionV0(collision1, v0_evt2))
              continue;
            bool LambdaTag2 = isCompatible(v0_evt2, 0);
            bool aLambdaTag2 = isCompatible(v0_evt2, 1);
            if (!LambdaTag2 && !aLambdaTag2)
              continue;

            // Deduplicate (v0_evt1, v0_evt2) pairs per collision2idx
            auto key = std::make_pair(v0_evt1.index(), v0_evt2.index());
            if (!seenMap[collision2idx].insert(key).second)
              continue;

            ROOT::Math::PxPyPzMVector proton_mix, antiproton_mix, pion_mix, antipion_mix, LambdaTag2dummy, AntiLambdaTag2dummy;
            if (LambdaTag2) {
              proton_mix = {v0_evt2.pxpos(), v0_evt2.pypos(), v0_evt2.pzpos(), massPr};
              antipion_mix = {v0_evt2.pxneg(), v0_evt2.pyneg(), v0_evt2.pzneg(), massPi};
              LambdaTag2dummy = proton_mix + antipion_mix;
            }
            if (aLambdaTag2) {
              antiproton_mix = {v0_evt2.pxneg(), v0_evt2.pyneg(), v0_evt2.pzneg(), massPr};
              pion_mix = {v0_evt2.pxpos(), v0_evt2.pypos(), v0_evt2.pzpos(), massPi};
              AntiLambdaTag2dummy = antiproton_mix + pion_mix;
            }
            if (shouldReject(LambdaTag2, aLambdaTag2, LambdaTag2dummy, AntiLambdaTag2dummy)) {
              continue;
            }
            if (TMath::Abs(v0_evt2.eta()) > 0.8)
              continue;
            if (LambdaTag1) {
              double acvalue = 1.0;
              fillHistograms(1, 0, LambdaTag1dummy, proton_mix, psiZDCC, psiZDCA, psiZDC, centrality, v0_evt1.mLambda(), v0_evt1.pt(), v0_evt1.eta(), acvalue);
            }
            if (aLambdaTag1) {
              double acvalue = 1.0;
              fillHistograms(0, 1, AntiLambdaTag1dummy, antiproton_mix, psiZDCC, psiZDCA, psiZDC, centrality, v0_evt1.mAntiLambda(), v0_evt1.pt(), v0_evt1.eta(), acvalue);
            }
          }
        }
      }
      // After processing all mixes, add current event V0s to pool for future mixing
      eventPools[bin].push_back(collision1.index());
      // Keep only N last events in FIFO queue
      if (static_cast<int>(eventPools[bin].size()) > meGrp.nMix) {
        eventPools[bin].pop_front();
      }
    }
  }
  PROCESS_SWITCH(lambdapolsp, processDerivedDataMixedFIFO, "Process mixed event using derived data with FIFO method", false);
};
WorkflowSpec defineDataProcessing(ConfigContext const& cfgc)
{
  return WorkflowSpec{
    adaptAnalysisTask<lambdapolsp>(cfgc, TaskName{"lambdapolsp"})};
}
