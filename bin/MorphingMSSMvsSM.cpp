#include "CombineHarvester/CombinePdfs/interface/MorphFunctions.h"
#include "CombineHarvester/CombinePdfs/interface/CMSHistFuncFactory.h"
#include "CombineHarvester/CombineTools/interface/Algorithm.h"
#include "CombineHarvester/CombineTools/interface/AutoRebin.h"
#include "CombineHarvester/CombineTools/interface/BinByBin.h"
#include "CombineHarvester/CombineTools/interface/CardWriter.h"
#include "CombineHarvester/CombineTools/interface/CombineHarvester.h"
#include "CombineHarvester/CombineTools/interface/Observation.h"
#include "CombineHarvester/CombineTools/interface/Process.h"
#include "CombineHarvester/CombineTools/interface/Systematics.h"
#include "CombineHarvester/CombineTools/interface/Utilities.h"
#include "CombineHarvester/MSSMvsSMRun2Legacy/interface/HttSystematics_MSSMvsSMRun2.h"
#include "CombineHarvester/MSSMvsSMRun2Legacy/interface/BinomialBinByBin.h"
#include "RooRealVar.h"
#include "RooWorkspace.h"
#include "TF1.h"
#include "TH2.h"
#include "boost/algorithm/string/predicate.hpp"
#include "boost/algorithm/string/split.hpp"
#include "boost/lexical_cast.hpp"
#include "boost/program_options.hpp"
#include "boost/regex.hpp"
#include <cstdlib>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <math.h>

using namespace std;
using boost::starts_with;
namespace po = boost::program_options;

int main(int argc, char **argv) {
  typedef vector<string> VString;
  typedef vector<pair<int, string>> Categories;
  using ch::syst::bin_id;
  using ch::JoinStr;

  // Define program options
  string output_folder = "output_MSSMvsSM_Run2";
  string base_path = string(getenv("CMSSW_BASE")) + "/src/CombineHarvester/MSSMvsSMRun2Legacy/shapes/";
  string sm_gg_fractions = string(getenv("CMSSW_BASE")) + "/src/CombineHarvester/MSSMvsSMRun2Legacy/data/higgs_pt_v3.root";
  string chan = "all";
  string category = "all";
  string variable = "m_sv_puppi";
  bool auto_rebin = false;
  bool real_data = false;
  bool binomial_bbb = false;
  bool verbose = false;
  string analysis = "sm"; // "sm", "sm_nobtag", "sm_nobtag_new",  "mssm", "mssm_btag", "mssm_vs_sm_standard", "mssm_vs_sm_qqh", "gof"
  int era = 2016; // 2016, 2017 or 2018
  po::variables_map vm;
  po::options_description config("configuration");
  config.add_options()
      ("base_path", po::value<string>(&base_path)->default_value(base_path))
      ("sm_gg_fractions", po::value<string>(&sm_gg_fractions)->default_value(sm_gg_fractions))
      ("variable", po::value<string>(&variable)->default_value(variable))
      ("channel", po::value<string>(&chan)->default_value(chan))
      ("category", po::value<string>(&category)->default_value(category))
      ("auto_rebin", po::value<bool>(&auto_rebin)->default_value(auto_rebin))
      ("real_data", po::value<bool>(&real_data)->default_value(real_data))
      ("verbose", po::value<bool>(&verbose)->default_value(verbose))
      ("output_folder", po::value<string>(&output_folder)->default_value(output_folder))
      ("analysis", po::value<string>(&analysis)->default_value(analysis))
      ("binomial_bbb", po::value<bool>(&binomial_bbb)->default_value(binomial_bbb))
      ("era", po::value<int>(&era)->default_value(era));
  po::store(po::command_line_parser(argc, argv).options(config).run(), vm);
  po::notify(vm);

  // Define the location of the "auxiliaries" directory where we can
  // source the input files containing the datacard shapes
  output_folder = output_folder + "_" + analysis + "_" + variable;
  std::map<string, string> input_dir;
  input_dir["mt"] = base_path;
  input_dir["et"] = base_path;
  input_dir["tt"] = base_path;
  input_dir["em"] = base_path;

  // Define channels
  VString chns;
  if (chan.find("mt") != std::string::npos)
    chns.push_back("mt");
  if (chan.find("et") != std::string::npos)
    chns.push_back("et");
  if (chan.find("tt") != std::string::npos)
    chns.push_back("tt");
  if (chan.find("em") != std::string::npos)
    chns.push_back("em");
  if (chan == "all")
    chns = {"mt", "et", "tt", "em"};

  // Define restriction to the channel defined by '--category' option
  if(category != "all"){
    std::vector<std::string> category_split;
    boost::split(category_split, category, boost::is_any_of("_"));
    chns = {category_split.at(0)};
  }

  // Define background and signal processes
  map<string, VString> bkg_procs;
  VString bkgs, bkgs_em, sm_signals, main_sm_signals, mssm_ggH_signals, mssm_bbH_signals;

  sm_signals = {"WH125", "ZH125", "ttH125"};
  main_sm_signals = {"ggH125", "qqH125"};

  mssm_ggH_signals = {"ggh_t", "ggh_b", "ggh_i", "ggH_t", "ggH_b", "ggH_i", "ggA_t", "ggA_b", "ggA_i"};
  mssm_bbH_signals = {"bbA", "bbH", "bbh"};
  if(analysis == "mssm" || analysis == "mssm_btag")
  {
    mssm_ggH_signals = {"ggh_t", "ggh_b", "ggh_i"};
    mssm_bbH_signals = {"bbh"};
  }

  bkgs = {"EMB", "ZL", "TTL", "VVL", "jetFakes", "ggHWW125", "qqHWW125"};
  bkgs_em = {"EMB", "W", "QCD", "ZL", "TTL", "VVL", "ggHWW125", "qqHWW125"};

  VString SUSYggH_masses = {"100", "110", "120", "130", "140", "180", "200", "250", "300", "350", "400", "450", "600", "700", "800", "900", "1200", "1400", "1500", "1600", "1800", "2000", "2300", "2600", "2900", "3200"};
  VString SUSYbbH_masses = {"90", "110", "120", "125", "130", "140", "160", "180", "200", "250", "350", "400", "450", "500", "600", "700", "800", "900", "1000", "1200", "1400", "1800", "2000", "2300", "2600", "3200"};

  std::cout << "[INFO] Considering the following processes as main backgrounds:\n";

  if (chan.find("em") != std::string::npos || chan.find("all") != std::string::npos) {
    std::cout << "For em channel : \n";
    for (unsigned int i=0; i < bkgs_em.size(); i++) std::cout << bkgs_em[i] << std::endl;
  }
  if (chan.find("mt") != std::string::npos || chan.find("et") != std::string::npos || chan.find("tt") != std::string::npos || chan.find("all") != std::string::npos) {
    std::cout << "For et,mt,tt channels : \n";
    for (unsigned int i=0; i < bkgs.size(); i++) std::cout << bkgs[i] << std::endl;
  }
  bkg_procs["et"] = bkgs;
  bkg_procs["mt"] = bkgs;
  bkg_procs["tt"] = bkgs;
  bkg_procs["em"] = bkgs_em;

  if(analysis == "sm" || analysis == "sm_nobtag" || analysis == "sm_nobtag_new"){
    for(auto chn : chns){
        bkg_procs[chn] = JoinStr({bkg_procs[chn],sm_signals});
    }
  }
  else if(analysis == "mssm" || analysis == "mssm_btag"){
    for(auto chn : chns){
        bkg_procs[chn] = JoinStr({bkg_procs[chn],sm_signals,main_sm_signals});
    }
  }

  // Define MSSM model-dependent mass parameters mA, mH, mh
  RooRealVar mA("mA", "mA", 125., 90., 4000.);
  RooRealVar mH("mH", "mH", 125., 90., 4000.);
  RooRealVar mh("mh", "mh", 125., 90., 4000.);
  mA.setConstant(true);

  // Define MSSM model-independent mass parameter MH
  RooRealVar MH("MH", "MH", 125., 90., 4000.);
  MH.setConstant(true);

  // Define categories
  map<string, Categories> cats;
  // STXS stage 0 categories (optimized on ggH and VBF)
  if(analysis == "sm_nobtag"){
    cats["et"] = {
        { 1, "et_wjets_control"},
        { 2, "et_nobtag_boosted"},
        { 3, "et_nobtag_0jet"},
        { 4, "et_nobtag_1jetlowpt"},
        { 5, "et_nobtag_1jethighpt"},
        { 6, "et_nobtag_2jetlowmjj"},
        { 7, "et_nobtag_2jethighmjjlowpt"},
        { 8, "et_nobtag_2jethighmjjhighpt"},
    };
    cats["mt"] = {
        { 1, "mt_wjets_control"},
        { 2, "mt_nobtag_boosted"},
        { 3, "mt_nobtag_0jet"},
        { 4, "mt_nobtag_1jetlowpt"},
        { 5, "mt_nobtag_1jethighpt"},
        { 6, "mt_nobtag_2jetlowmjj"},
        { 7, "mt_nobtag_2jethighmjjlowpt"},
        { 8, "mt_nobtag_2jethighmjjhighpt"},
    };
    cats["tt"] = {
        { 2, "tt_nobtag_boosted"},
        { 3, "tt_nobtag_0jet"},
        { 4, "tt_nobtag_1jetlowpt"},
        { 5, "tt_nobtag_1jethighpt"},
        { 6, "tt_nobtag_2jetlowmjj"},
        { 7, "tt_nobtag_2jethighmjjlowpt"},
        { 8, "tt_nobtag_2jethighmjjhighpt"},
    };
    cats["em"] = {
        { 1, "em_ttbar_control"},
        { 2, "em_nobtag_boosted"},
        { 3, "em_nobtag_0jet"},
        { 4, "em_nobtag_1jetlowpt"},
        { 5, "em_nobtag_1jethighpt"},
        { 6, "em_nobtag_2jetlowmjj"},
        { 7, "em_nobtag_2jethighmjjlowpt"},
        { 8, "em_nobtag_2jethighmjjhighpt"},
    };
  }
  else if(analysis == "sm_nobtag_new"){
    cats["et"] = {
        { 1, "et_wjets_control"},

        {10, "et_nobtag_0jet"},

        {11, "et_nobtag_1jet_lowpt_lowdeltar"},
        {12, "et_nobtag_1jet_lowpt_highdeltar"},
        {13, "et_nobtag_1jet_mediumpt"},
        {14, "et_nobtag_1jet_highpt"},

        {15, "et_nobtag_2jet_lowmjj_lowdeltar"},
        {16, "et_nobtag_2jet_lowmjj_highdeltar"},
        {17, "et_nobtag_2jet_mediummjj_lowdeltar_lowjdeta"},
        {18, "et_nobtag_2jet_mediummjj_lowdeltar_highjdeta"},
        {19, "et_nobtag_2jet_mediummjj_highdeltar"},
        {20, "et_nobtag_2jet_highmjj"},
    };
    cats["mt"] = {
        { 1, "mt_wjets_control"},

        {10, "mt_nobtag_0jet_lowpt"},
        {11, "mt_nobtag_0jet_highpt"},

        {12, "mt_nobtag_1jet_lowpt_lowdeltar"},
        {13, "mt_nobtag_1jet_lowpt_highdeltar"},
        {14, "mt_nobtag_1jet_lowmediumpt_lowdeltar"},
        {15, "mt_nobtag_1jet_highmediumpt_lowdeltar"},
        {16, "mt_nobtag_1jet_mediumpt_highdeltar"},
        {17, "mt_nobtag_1jet_highpt"},

        {18, "mt_nobtag_2jet_lowmjj_lowdeltar"},
        {19, "mt_nobtag_2jet_lowmjj_highdeltar"},
        {20, "mt_nobtag_2jet_mediummjj_lowdeltar_lowjdeta"},
        {21, "mt_nobtag_2jet_mediummjj_lowdeltar_highjdeta"},
        {22, "mt_nobtag_2jet_mediummjj_highdeltar"},
        {23, "mt_nobtag_2jet_highmjj"},
    };
    cats["tt"] = {
        {10, "tt_nobtag_0jet_lowdeltar"},
        {11, "tt_nobtag_0jet_highdeltar"},

        {12, "tt_nobtag_1jet_lowpt_lowdeltar"},
        {13, "tt_nobtag_1jet_lowpt_mediumdeltar"},
        {14, "tt_nobtag_1jet_lowpt_highdeltar"},
        {15, "tt_nobtag_1jet_mediumpt"},
        {16, "tt_nobtag_1jet_highpt"},

        {17, "tt_nobtag_2jet_lowmjj_lowdeltar"},
        {18, "tt_nobtag_2jet_lowmjj_highdeltar"},
        {19, "tt_nobtag_2jet_mediummjj_lowdeltar_lowjdeta"},
        {20, "tt_nobtag_2jet_mediummjj_lowdeltar_highjdeta"},
        {21, "tt_nobtag_2jet_mediummjj_highdeltar"},
        {22, "tt_nobtag_2jet_highmjj"},
    };
    cats["em"] = {
        { 1, "em_ttbar_control"},

        {10, "em_nobtag_0jet_lowpt"},
        {11, "em_nobtag_0jet_highpt"},

        {12, "em_nobtag_1jet_lowpt"},
        {13, "em_nobtag_1jet_lowmediumpt"},
        {14, "em_nobtag_1jet_highmediumpt"},
        {15, "em_nobtag_1jet_highpt"},

        {16, "em_nobtag_2jet_lowmjj"},
        {17, "em_nobtag_2jet_mediummjj_lowjdeta"},
        {18, "em_nobtag_2jet_mediummjj_highjdeta"},
        {19, "em_nobtag_2jet_highmjj"},
    };
  }
  else if(analysis == "mssm" || analysis == "mssm_vs_sm_standard"){
    cats["et"] = {
        {  1, "et_wjets_control"},
        { 32, "et_nobtag_tightmt"},
        { 33, "et_nobtag_loosemt"},
        { 35, "et_btag_tightmt"},
        { 36, "et_btag_loosemt"},
    };
    cats["mt"] = {
        {  1, "mt_wjets_control"},
        { 32, "mt_nobtag_tightmt"},
        { 33, "mt_nobtag_loosemt"},
        { 35, "mt_btag_tightmt"},
        { 36, "mt_btag_loosemt"},
    };
    cats["tt"] = {
        { 32, "tt_nobtag"},
        { 35, "tt_btag"},
    };
    cats["em"] = {
        {  1, "em_ttbar_control"},
        { 32, "em_nobtag_highdzeta"},
        { 33, "em_nobtag_mediumdzeta"},
        { 34, "em_nobtag_lowdzeta"},
        { 35, "em_btag_highdzeta"},
        { 36, "em_btag_mediumdzeta"},
        { 37, "em_btag_lowdzeta"},
    };
  }
  else if(analysis == "mssm_vs_sm_qqh"){
    cats["et"] = {
        {  1, "et_wjets_control"},

        { 10, "et_nobtag_0jet"},

        { 11, "et_nobtag_1jet_lowpt_lowdeltar"},
        { 12, "et_nobtag_1jet_lowpt_highdeltar"},
        { 13, "et_nobtag_1jet_mediumpt"},
        { 14, "et_nobtag_1jet_highpt"},

        { 15, "et_nobtag_2jet_lowmjj_lowdeltar"},
        { 16, "et_nobtag_2jet_lowmjj_highdeltar"},
        { 17, "et_nobtag_2jet_mediummjj_lowdeltar_lowjdeta"},
        { 18, "et_nobtag_2jet_mediummjj_lowdeltar_highjdeta"}, // TODO: not filled by MSSM signal properly
        { 19, "et_nobtag_2jet_mediummjj_highdeltar"},
        { 20, "et_nobtag_2jet_highmjj"},

        { 35, "et_btag_tightmt"},
        { 36, "et_btag_loosemt"},
    };
    cats["mt"] = {
        {  1, "mt_wjets_control"},

        { 10, "mt_nobtag_0jet_lowpt"},
        { 11, "mt_nobtag_0jet_highpt"},

        { 12, "mt_nobtag_1jet_lowpt_lowdeltar"}, // TODO: not filled by MSSM signal properly 
        { 13, "mt_nobtag_1jet_lowpt_highdeltar"},
        { 14, "mt_nobtag_1jet_lowmediumpt_lowdeltar"}, // TODO: not filled by MSSM signal properly
        { 15, "mt_nobtag_1jet_highmediumpt_lowdeltar"}, // TODO: not filled by MSSM signal properly
        { 16, "mt_nobtag_1jet_mediumpt_highdeltar"},
        { 17, "mt_nobtag_1jet_highpt"},

        { 18, "mt_nobtag_2jet_lowmjj_lowdeltar"},
        { 19, "mt_nobtag_2jet_lowmjj_highdeltar"},
        { 20, "mt_nobtag_2jet_mediummjj_lowdeltar_lowjdeta"},
        { 21, "mt_nobtag_2jet_mediummjj_lowdeltar_highjdeta"}, // TODO: not filled by MSSM signal properly
        { 22, "mt_nobtag_2jet_mediummjj_highdeltar"},
        { 23, "mt_nobtag_2jet_highmjj"},

        { 35, "mt_btag_tightmt"},
        { 36, "mt_btag_loosemt"},
    };
    cats["tt"] = {
        { 10, "tt_nobtag_0jet_lowdeltar"},
        { 11, "tt_nobtag_0jet_highdeltar"},

        { 12, "tt_nobtag_1jet_lowpt_lowdeltar"},
        { 13, "tt_nobtag_1jet_lowpt_mediumdeltar"},
        { 14, "tt_nobtag_1jet_lowpt_highdeltar"},
        { 15, "tt_nobtag_1jet_mediumpt"},
        { 16, "tt_nobtag_1jet_highpt"}, // TODO: not filled by MSSM signal properly

        { 17, "tt_nobtag_2jet_lowmjj_lowdeltar"},
        { 18, "tt_nobtag_2jet_lowmjj_highdeltar"},
        { 19, "tt_nobtag_2jet_mediummjj_lowdeltar_lowjdeta"},
        { 20, "tt_nobtag_2jet_mediummjj_lowdeltar_highjdeta"}, // TODO: not filled by MSSM signal properly
        { 21, "tt_nobtag_2jet_mediummjj_highdeltar"},
        { 22, "tt_nobtag_2jet_highmjj"}, // TODO: not filled by MSSM signal properly

        { 35, "tt_btag"},
    };
    cats["em"] = {
        {  1, "em_ttbar_control"},

        { 10, "em_nobtag_0jet_lowpt"},
        { 11, "em_nobtag_0jet_highpt"},

        { 12, "em_nobtag_1jet_lowpt"},
        { 13, "em_nobtag_1jet_lowmediumpt"},
        { 14, "em_nobtag_1jet_highmediumpt"},
        { 15, "em_nobtag_1jet_highpt"},

        { 16, "em_nobtag_2jet_lowmjj"},
        { 17, "em_nobtag_2jet_mediummjj_lowjdeta"},
        { 18, "em_nobtag_2jet_mediummjj_highjdeta"},
        { 19, "em_nobtag_2jet_highmjj"},

        { 35, "em_btag_highdzeta"},
        { 36, "em_btag_mediumdzeta"},
        { 37, "em_btag_lowdzeta"},
    };
  }
  else throw std::runtime_error("Given categorization is not known.");

  // Create combine harverster object
  ch::CombineHarvester cb;

  // Add observations and processes
  std::string era_tag;
  if (era == 2016) era_tag = "Run2016";
  else if (era == 2017) era_tag = "Run2017";
  else if (era == 2018) era_tag = "Run2018";

  else std::runtime_error("Given era is not implemented.");

  for (auto chn : chns) {
    cb.AddObservations({"*"}, {"htt"}, {era_tag}, {chn}, cats[chn]);
    cb.AddProcesses({"*"}, {"htt"}, {era_tag}, {chn}, bkg_procs[chn], cats[chn], false);
    if(analysis == "sm" || analysis == "sm_nobtag" || analysis == "sm_nobtag_new"){
      cb.AddProcesses({""}, {"htt"}, {era_tag}, {chn}, main_sm_signals, cats[chn], true);
    }
    else if(analysis == "mssm" || analysis == "mssm_btag" || analysis == "mssm_vs_sm_standard" || analysis == "mssm_vs_sm_qqh"){
      cb.AddProcesses(SUSYggH_masses, {"htt"}, {era_tag}, {chn}, mssm_ggH_signals, cats[chn], true);
      cb.AddProcesses(SUSYbbH_masses, {"htt"}, {era_tag}, {chn}, mssm_bbH_signals, cats[chn], true);
      if(analysis == "mssm_vs_sm_standard" || "mssm_vs_sm_qqh"){
        cb.AddProcesses({""}, {"htt"}, {era_tag}, {chn}, ch::JoinStr({main_sm_signals, sm_signals}), cats[chn], true);
      }
      if(analysis == "mssm_vs_sm_qqh"){
        cb.AddProcesses({""}, {"htt"}, {era_tag}, {chn}, {"qqh"}, cats[chn], true);
      }
    }
  }

  // Add systematics
  ch::AddMSSMvsSMRun2Systematics(cb, true, true, true, true, era);

  // Define restriction to the desired category
  if(category != "all"){
    cb = cb.bin({category});
  }

  // Extract shapes from input ROOT files
  for (string chn : chns) {
    if(analysis == "mssm_vs_sm_qqh")
    {
      std::vector<int> nobtag_categories = {2, 3, 4, 5, 6, 7, 8, 9,10,
                                        11,12,13,14,15,16,17,18,19,20,
                                        21,22,23,24,25,26,27,28,29,30,
                                        31,32,33,34}; // SM and MSSM no-btag categories
      std::vector<int> remaining_categories = {1,35,36,37};

      // Backgrounds
      cb.cp().channel({chn}).bin_id(nobtag_categories).backgrounds().ExtractShapes(
        input_dir[chn] + "htt_" + chn + ".inputs-mssm-vs-sm-" + era_tag + "-m_sv_puppi.root", "$BIN/$PROCESS", "$BIN/$PROCESS_$SYSTEMATIC");
      cb.cp().channel({chn}).bin_id(remaining_categories).backgrounds().ExtractShapes(
        input_dir[chn] + "htt_" + chn + ".inputs-mssm-vs-sm-" + era_tag + "-mt_tot_puppi.root", "$BIN/$PROCESS", "$BIN/$PROCESS_$SYSTEMATIC");

      // ggH MSSM signals
      cb.cp().channel({chn}).bin_id(nobtag_categories).process(mssm_ggH_signals).ExtractShapes(
        input_dir[chn] + "htt_" + chn + ".inputs-mssm-vs-sm-" + era_tag + "-m_sv_puppi.root", "$BIN/$PROCESS_$MASS", "$BIN/$PROCESS_$MASS_$SYSTEMATIC");
      cb.cp().channel({chn}).bin_id(remaining_categories).process(mssm_ggH_signals).ExtractShapes(
        input_dir[chn] + "htt_" + chn + ".inputs-mssm-vs-sm-" + era_tag + "-mt_tot_puppi.root", "$BIN/$PROCESS_$MASS", "$BIN/$PROCESS_$MASS_$SYSTEMATIC");

      // bbH MSSM signals
      cb.cp().channel({chn}).bin_id(nobtag_categories).process(mssm_bbH_signals).ExtractShapes(
        input_dir[chn] + "htt_" + chn + ".inputs-mssm-vs-sm-" + era_tag + "-m_sv_puppi.root", "$BIN/bbH_$MASS", "$BIN/bbH_$MASS_$SYSTEMATIC");
      cb.cp().channel({chn}).bin_id(remaining_categories).process(mssm_bbH_signals).ExtractShapes(
        input_dir[chn] + "htt_" + chn + ".inputs-mssm-vs-sm-" + era_tag + "-mt_tot_puppi.root", "$BIN/bbH_$MASS", "$BIN/bbH_$MASS_$SYSTEMATIC");

      // SM signals
      cb.cp().channel({chn}).bin_id(nobtag_categories).process(ch::JoinStr({sm_signals,main_sm_signals})).ExtractShapes(
        input_dir[chn] + "htt_" + chn + ".inputs-mssm-vs-sm-" + era_tag + "-m_sv_puppi.root", "$BIN/$PROCESS$MASS", "$BIN/$PROCESS$MASS_$SYSTEMATIC");
      cb.cp().channel({chn}).bin_id(remaining_categories).process(ch::JoinStr({sm_signals,main_sm_signals})).ExtractShapes(
        input_dir[chn] + "htt_" + chn + ".inputs-mssm-vs-sm-" + era_tag + "-mt_tot_puppi.root", "$BIN/$PROCESS$MASS", "$BIN/$PROCESS$MASS_$SYSTEMATIC");

      // qqH MSSM signals
      cb.cp().channel({chn}).bin_id(nobtag_categories).process({"qqh"}).ExtractShapes(
        input_dir[chn] + "htt_" + chn + ".inputs-mssm-vs-sm-" + era_tag + "-m_sv_puppi.root", "$BIN/qqH125$MASS", "$BIN/qqH125$MASS_$SYSTEMATIC");
      cb.cp().channel({chn}).bin_id(remaining_categories).process({"qqh"}).ExtractShapes(
        input_dir[chn] + "htt_" + chn + ".inputs-mssm-vs-sm-" + era_tag + "-mt_tot_puppi.root", "$BIN/qqH125$MASS", "$BIN/qqH125$MASS_$SYSTEMATIC");
    }
    else{
      cb.cp().channel({chn}).backgrounds().ExtractShapes(
        input_dir[chn] + "htt_" + chn + ".inputs-mssm-vs-sm-" + era_tag + "-" + variable + ".root", "$BIN/$PROCESS", "$BIN/$PROCESS_$SYSTEMATIC");
      if(analysis == "sm" || analysis == "sm_nobtag" || analysis == "sm_nobtag_new"){
        cb.cp().channel({chn}).process(main_sm_signals).ExtractShapes(
          input_dir[chn] + "htt_" + chn + ".inputs-mssm-vs-sm-" + era_tag + "-" + variable + ".root", "$BIN/$PROCESS$MASS", "$BIN/$PROCESS$MASS_$SYSTEMATIC");
      }
      else if(analysis == "mssm" || analysis == "mssm_btag" || analysis == "mssm_vs_sm_standard"){
        cb.cp().channel({chn}).process(mssm_ggH_signals).ExtractShapes(
          input_dir[chn] + "htt_" + chn + ".inputs-mssm-vs-sm-" + era_tag + "-" + variable + ".root", "$BIN/$PROCESS_$MASS", "$BIN/$PROCESS_$MASS_$SYSTEMATIC");
        cb.cp().channel({chn}).process(mssm_bbH_signals).ExtractShapes(
          input_dir[chn] + "htt_" + chn + ".inputs-mssm-vs-sm-" + era_tag + "-" + variable + ".root", "$BIN/bbH_$MASS", "$BIN/bbH_$MASS_$SYSTEMATIC");
        if(analysis == "mssm_vs_sm_standard"){
          cb.cp().channel({chn}).process(ch::JoinStr({sm_signals,main_sm_signals})).ExtractShapes(
            input_dir[chn] + "htt_" + chn + ".inputs-mssm-vs-sm-" + era_tag + "-" + variable + ".root", "$BIN/$PROCESS$MASS", "$BIN/$PROCESS$MASS_$SYSTEMATIC");
        }
      }
    }
  }

  // Delete processes with 0 yield
  cb.FilterProcs([&](ch::Process *p) {
    bool null_yield = !(p->rate() > 0.0);
    if (null_yield) {
      std::cout << "[WARNING] Removing process with null yield: \n ";
      std::cout << ch::Process::PrintHeader << *p << "\n";
      cb.FilterSysts([&](ch::Systematic *s) {
        bool remove_syst = (MatchingProcess(*p, *s));
        return remove_syst;
      });
    }
    return null_yield;
  });

  // Delete systematics with 0 yield since these result in a bogus norm error in combine
  cb.FilterSysts([&](ch::Systematic *s) {
    if (s->type() == "shape") {
      if (s->shape_u()->Integral() == 0.0) {
        std::cout << "[WARNING] Removing systematic with null yield in up shift:" << std::endl;
        std::cout << ch::Systematic::PrintHeader << *s << "\n";
        return true;
      }
      if (s->shape_d()->Integral() == 0.0) {
        std::cout << "[WARNING] Removing systematic with null yield in down shift:" << std::endl;
        std::cout << ch::Systematic::PrintHeader << *s << "\n";
        return true;
      }
    }
    return false;
  });

  // Replacing observation with the sum of the backgrounds (Asimov data)
  // useful to be able to check this, so don't do the replacement
  // for these
  if (!real_data) {
    for (auto b : cb.cp().bin_set()) {
      std::cout << "[INFO] Replacing data with asimov in bin " << b << "\n";
      auto background_shape = cb.cp().bin({b}).backgrounds().GetShape();
      auto signal_shape = cb.cp().bin({b}).signals().GetShape();
      auto total_procs_shape = cb.cp().bin({b}).data().GetShape();
      total_procs_shape.Scale(0.0);
      if(analysis == "sm" || analysis == "sm_nobtag" || analysis == "sm_nobtag_new"){
        bool no_signal = (signal_shape.GetNbinsX() == 1 && signal_shape.Integral() == 0.0);
        bool no_background = (background_shape.GetNbinsX() == 1 && background_shape.Integral() == 0.0);
        if(no_signal && no_background)
        {
          std::cout << "\t[WARNING] No signal and no background available in bin " << b << std::endl;
        }
        else if(no_background)
        {
          std::cout << "\t[WARNING] No background available in bin " << b << std::endl;
          total_procs_shape = total_procs_shape + signal_shape;
        }
        else if(no_signal)
        {
          std::cout << "\t[WARNING] No signal available in bin " << b << std::endl;
          total_procs_shape = total_procs_shape + background_shape;
        }
        else
        {
          total_procs_shape = total_procs_shape + background_shape + signal_shape;
        }
        cb.cp().bin({b}).ForEachObs([&](ch::Observation *obs) {
          obs->set_shape(total_procs_shape,true);
        });
      }
      else if(analysis == "mssm" || analysis == "mssm_btag" || analysis == "mssm_vs_sm_standard" || analysis == "mssm_vs_sm_qqh"){
        bool no_background = (background_shape.GetNbinsX() == 1 && background_shape.Integral() == 0.0);
        if(no_background)
        {
          std::cout << "\t[WARNING] No background available in bin " << b << std::endl;
        }
        else
        {
          total_procs_shape = total_procs_shape + background_shape;
        }
        cb.cp().bin({b}).ForEachObs([&](ch::Observation *obs) {
          obs->set_shape(total_procs_shape,true);
        });
      }
    }
  }

  // At this point we can fix the negative bins
  std::cout << "[INFO] Fixing negative bins.\n";
  cb.ForEachProc([](ch::Process *p) {
    if (ch::HasNegativeBins(p->shape())) {
      auto newhist = p->ClonedShape();
      ch::ZeroNegativeBins(newhist.get());
      p->set_shape(std::move(newhist), false);
    }
  });

  cb.ForEachSyst([](ch::Systematic *s) {
    if (s->type().find("shape") == std::string::npos)
      return;
    if (ch::HasNegativeBins(s->shape_u()) ||
        ch::HasNegativeBins(s->shape_d())) {
      auto newhist_u = s->ClonedShapeU();
      auto newhist_d = s->ClonedShapeD();
      ch::ZeroNegativeBins(newhist_u.get());
      ch::ZeroNegativeBins(newhist_d.get());
      s->set_shapes(std::move(newhist_u), std::move(newhist_d), nullptr);
    }
  });

  // Perform auto-rebinning
  if (auto_rebin) {
    std::cout << "[INFO] Performing auto-rebinning.\n";
    auto rebin = ch::AutoRebin().SetBinThreshold(1.0).SetBinUncertFraction(0.9).SetRebinMode(1).SetPerformRebin(true).SetVerbosity(1);
    rebin.Rebin(cb, cb);
  }

  // Merge bins and set bin-by-bin uncertainties if no autoMCStats is used.
  if (binomial_bbb) {
    std::cout << "[INFO] Adding binomial bbb.\n";
    auto bbb = ch::BinomialBinByBinFactory()
                   .SetPattern("CMS_$ANALYSIS_$CHANNEL_$BIN_$ERA_$PROCESS_binomial_bin_$#")
                   .SetBinomialP(0.022)
                   .SetBinomialN(1000.0)
                   .SetFixNorm(false);
    bbb.AddBinomialBinByBin(cb.cp().channel({"em"}).process({"EMB"}), cb);
  }

  // This function modifies every entry to have a standardised bin name of
  // the form: {analysis}_{channel}_{bin_id}_{era}
  ch::SetStandardBinNames(cb, "$ANALYSIS_$CHANNEL_$BINID_$ERA");

  // Adding bin-by-bin uncertainties
  std::cout << "[INFO] Adding bin-by-bin uncertainties.\n";
  cb.SetAutoMCStats(cb, 0.0);

  // Setup morphed mssm signals for model-independent case
  RooWorkspace ws("htt", "htt");

  std::map<std::string, RooAbsReal *> mass_var = {
    {"ggh_t", &mh}, {"ggh_b", &mh}, {"ggh_i", &mh},
    {"ggH_t", &mH}, {"ggH_b", &mH}, {"ggH_i", &mH},
    {"ggA_t", &mA}, {"ggA_b", &mA}, {"ggA_i", &mA},
    {"bbh", &mh},
    {"bbH", &mH},
    {"bbA", &mA}
  };

  std::map<std::string, std::string> process_norm_map = {
    {"ggh_t", "prenorm"}, {"ggh_b", "prenorm"}, {"ggh_i", "prenorm"},
    {"ggH_t", "prenorm"}, {"ggH_b", "prenorm"}, {"ggH_i", "prenorm"},
    {"ggA_t", "prenorm"}, {"ggA_b", "prenorm"}, {"ggA_i", "prenorm"},
    {"bbh", "norm"},
    {"bbH", "norm"},
    {"bbA", "norm"}
  };

  if(analysis == "mssm" || analysis == "mssm_btag")
  {
    mass_var = {
      {"ggh_t", &MH}, {"ggh_b", &MH}, {"ggh_i", &MH},
      {"bbh", &MH}
    };

    process_norm_map = {
      {"ggh_t", "prenorm"}, {"ggh_b", "prenorm"}, {"ggh_i", "prenorm"},
      {"bbh", "norm"}
    };

    std::cout << "[INFO] Adding aditional terms for mssm ggh NLO reweighting.\n";
    // Assuming sm fractions of t, b and i contributions of 'ggh' in model-independent analysis
    TFile fractions_sm(sm_gg_fractions.c_str());
    RooWorkspace *w_sm = (RooWorkspace*)fractions_sm.Get("w");
    w_sm->var("mh")->SetName("MH");
    RooAbsReal *t_frac = w_sm->function("ggh_t_SM_frac");
    RooAbsReal *b_frac = w_sm->function("ggh_b_SM_frac");
    RooAbsReal *i_frac = w_sm->function("ggh_i_SM_frac");
    t_frac->SetName("ggh_t_frac");
    b_frac->SetName("ggh_b_frac");
    i_frac->SetName("ggh_i_frac");
    ws.import(MH);
    ws.import(*t_frac, RooFit::RecycleConflictNodes());
    ws.import(*b_frac, RooFit::RecycleConflictNodes());
    ws.import(*i_frac, RooFit::RecycleConflictNodes());
    fractions_sm.Close();
  }


  if(analysis == "mssm" || analysis == "mssm_btag" || analysis == "mssm_vs_sm_standard" || analysis == "mssm_vs_sm_qqh")
  {
    TFile morphing_demo(("htt_mssm_morphing_" + category+ "_demo.root").c_str(), "RECREATE");

    // Perform morphing
    auto mssm_signals = ch::JoinStr({mssm_ggH_signals,mssm_bbH_signals});
    std::cout << "[INFO] Performing template morphing for mssm ggh and bbh.\n";
    auto morphFactory = ch::CMSHistFuncFactory();
    morphFactory.SetHorizontalMorphingVariable(mass_var);
    morphFactory.Run(cb, ws, process_norm_map);

    if(analysis == "mssm" || analysis == "mssm_btag"){
      // Adding 'norm' terms into workspace according to desired signals
      for (auto bin : cb.cp().bin_set())
      {
        for (auto proc : mssm_ggH_signals)
        {
          std::string prenorm_name = bin + "_" + proc + "_morph_prenorm";
          std::string norm_name = bin + "_" + proc + "_morph_norm";
          ws.factory(TString::Format("expr::%s('@0*@1',%s, %s_frac)", norm_name.c_str(), prenorm_name.c_str(), proc.c_str()));
        }
      }
    }

    // Saving workspace with morphed signals
    morphing_demo.cd();
    if (verbose)
      ws.Print();
    ws.Write();
    morphing_demo.Close();
    cb.AddWorkspace(ws);
    cb.cp().process(mssm_signals).ExtractPdfs(cb, "htt", "$BIN_$PROCESS_morph");
    std::cout << "[INFO] Finished template morphing for mssm ggh and bbh.\n";
  }

  std::cout << "[INFO] Writing datacards to " << output_folder << std::endl;

  // Decide, how to write out the datacards depending on --category option
  if(category == "all") {
      // Write out datacards. Naming convention important for rest of workflow. We
      // make one directory per chn-cat, one per chn and cmb. In this code we only
      // store the individual datacards for each directory to be combined later.
      ch::CardWriter writer(output_folder + "/" + era_tag + "/$TAG/$BIN.txt",
                            output_folder + "/" + era_tag + "/$TAG/common/htt_input_" + era_tag + ".root");

      // We're not using mass as an identifier - which we need to tell the
      // CardWriter
      // otherwise it will see "*" as the mass value for every object and skip it
      writer.SetWildcardMasses({});

      // Set verbosity
      if (verbose)
        writer.SetVerbosity(1);

      // Write datacards combined and per channel
      writer.WriteCards("cmb", cb);

      for (auto chn : chns) {
        writer.WriteCards(chn, cb.cp().channel({chn}));
      }
  }
  else {
      // Write out datacards. Naming convention important for rest of workflow. We
      // make one directory per chn-cat, one per chn and cmb. In this code we only
      // store the individual datacards for each directory to be combined later.
      ch::CardWriter writer(output_folder + "/" + era_tag + "/$BIN/$BIN.txt",
                            output_folder + "/" + era_tag + "/$BIN/common/$BIN_input_" + era_tag + ".root");

      // We're not using mass as an identifier - which we need to tell the
      // CardWriter
      // otherwise it will see "*" as the mass value for every object and skip it
      writer.SetWildcardMasses({});

      // Set verbosity
      if (verbose)
        writer.SetVerbosity(1);

      writer.WriteCards("", cb);
  }

  if (verbose)
    cb.PrintAll();

  std::cout << "[INFO] Done producing datacards.\n";
}
