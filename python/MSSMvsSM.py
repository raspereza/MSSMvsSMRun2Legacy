from HiggsAnalysis.CombinedLimit.PhysicsModel import *
from CombineHarvester.MSSMvsSMRun2Legacy.mssm_xs_tools import mssm_xs_tools

import os
import ROOT
import sys
import json
import pprint
import numpy as np
import itertools
import re
from collections import defaultdict
from array import array

class MSSMvsSMHiggsModel(PhysicsModel):
    def __init__(self):
        PhysicsModel.__init__(self)
        self.filePrefix = ''
        self.modelFile = ''
        self.filename = ''
        self.scenario = ''
        self.energy = ''
        self.ggHatNLO = ''
        self.mssm_inputs = None
        self.sm_predictions = None
        self.minTemplateMass = None
        self.maxTemplateMass = None
        self.quantity_map = {
            "mass"             : {"method" :     "mass", "name" : "m{HIGGS}",                 "access" : "{HIGGS}"},
            "width"            : {"method" :    "width", "name" : "w{HIGGS}",                 "access" : "{HIGGS}"},
            "width_SM"         : {"method" :    "width", "name" : "w{HIGGS}_SM",              "access" : "{HIGGS}_SM"},
            "br"               : {"method" :       "br", "name" : "br_{HIGGS}tautau",         "access" : "{HIGGS}->tautau"},
            "br_SM"            : {"method" :       "br", "name" : "br_{HIGGS}tautau_SM",      "access" : "{HIGGS}->tautau_SM"},
            "xsec"             : {"method" :     "xsec", "name" : "xs_{PROD}{HIGGS}",         "access" : "{PROD}->{HIGGS}"},
            "xsec_SM"          : {"method" :     "xsec", "name" : "xs_{PROD}{HIGGS}_SM",      "access" : "{PROD}->{HIGGS}_SM"},
            "interference"     : {"method" :     "xsec", "name" : "int_{PROD}{HIGGS}_tautau", "access" : "int_{PROD}_tautau_{HIGGS}"},
            "yukawa_top"       : {"method" : "coupling", "name" : "Yt_MSSM_{HIGGS}",          "access" : "gt_{HIGGS}"},
            "yukawa_bottom"    : {"method" : "coupling", "name" : "Yb_MSSM_{HIGGS}",          "access" : "gb_{HIGGS}"},
            "yukawa_deltab"    : {"method" : "coupling", "name" : "Ydeltab_MSSM",             "access" : "deltab"},
            "yukawa_im_deltab" : {"method" : "coupling", "name" : "Yimdeltab_MSSM",           "access" : "im_deltab"},
        }
        self.uncertainty_map = {
            "ggscale" : "::scale{VAR}",
            "ggpdfas" : "::pdfas{VAR}",
            "bbtotal" : "::{VAR}",
        }
        self.binning =  {
            "mh125": {
                "tanb" : np.arange(0.5, 60.1, 0.1),
                "mA" :   np.arange(70.0, 2601.0, 1.0),
            },
            "mh125EFT": {
                "tanb" : np.arange(1.0, 10.25, 0.25),
                "mA" :   np.arange(70.0, 3001.0, 1.0),
            },
            "mh125_lc": {
                "tanb" : np.arange(0.5, 60.1, 0.1),
                "mA" :   np.arange(70.0, 2601.0, 1.0),
            },
            "mh125EFT_lc": {
                "tanb" : np.arange(1.0, 10.25, 0.25),
                "mA" :   np.arange(70.0, 3001.0, 1.0),
            },
            "mh125_ls": {
                "tanb" : np.arange(0.5, 60.1, 0.1),
                "mA" :   np.arange(70.0, 2601.0, 1.0),
            },
            "mh125_align": {
                "tanb" : np.arange(1.0, 20.25, 0.25),
                "mA" :   np.arange(120.0, 1001.0, 1.0),
            },
            "mh125_muneg_1": {
                "tanb" : np.arange(0.5, 60.1, 0.1),
                "mA" :   np.arange(70.0, 2601.0, 1.0),
            },
            "mh125_muneg_2": {
                "tanb" : np.arange(0.5, 60.1, 0.1),
                "mA" :   np.arange(70.0, 2601.0, 1.0),
            },
            "mh125_muneg_3": {
                "tanb" : np.arange(0.5, 60.1, 0.1),
                "mA" :   np.arange(70.0, 2601.0, 1.0),
            },
            "mHH125": {
                "tanb" : np.arange(5.0, 6.0, 0.005),
                "mHp" :   np.arange(150.0, 200.2, 0.2),
            },
            "mh1125_CPV": {
                "tanb" : np.arange(1.0, 20.25, 0.25),
                "mHp" :   np.arange(130.0, 1501.0, 1.0),
            },
        }
        self.PROC_SETS = []
        self.SYST_DICT = defaultdict(list)
        self.NUISANCES = set()
        self.scaleforh = 1.0
        self.bsmscalar = ""
        self.smlike = "h"
        self.massparameter = "mA"
        self.replace_with_sm125 = True
        self.cpv_template_pairs = {"H1" : "h", "H2" : "H", "H3" : "A"}

    def setPhysicsOptions(self,physOptions):
        for po in physOptions:
            if po.startswith('filePrefix='):
                self.filePrefix = po.replace('filePrefix=', '')
                print 'Set file prefix to: %s' % self.filePrefix

            if po.startswith('modelFile='):
                cfg= po.replace('modelFile=', '')
                cfgSplit = cfg.split(',')
                if len(cfgSplit) != 3:
                    raise RuntimeError, 'Model file argument %s should be in the format ENERGY,ERA,FILE' % cfg
                self.energy = cfgSplit[0]
                self.era = cfgSplit[1]
                self.modelFile = cfgSplit[2]
                self.scenario = self.modelFile.replace('.root','').replace('_%s'%self.energy,'')
                print "Importing scenario '%s' for sqrt(s) = '%s TeV' and '%s' data-taking period from '%s'"%(self.scenario, self.energy, self.era, self.modelFile)

                if self.scenario == "mHH125":
                    self.smlike = "H"
                    self.bsmscalar = "h"
                    self.massparameter = "mHp"
                elif self.scenario != "mh1125_CPV":
                    self.smlike = "h"
                    self.bsmscalar = "H"
                    self.massparameter = "mA"
                else:
                    self.smlike = "H1"
                    self.bsmscalar = ""
                    self.massparameter = "mHp"
                print "Chosen model-specific settings:"
                print "SM-like Higgs boson:",self.smlike
                print "BSM scalar Higgs boson:",self.bsmscalar
                print "Mass parameter in the plane:",self.massparameter

            if po.startswith('MSSM-NLO-Workspace='):
                self.ggHatNLO = po.replace('MSSM-NLO-Workspace=', '')
                print "Using %s for MSSM ggH NLO reweighting"%self.ggHatNLO

            if po.startswith('replace-with-SM125='):
                self.replace_with_sm125 = bool(int(po.replace('replace-with-SM125=', ''))) # use either 1 or 0 for the choice
                print "Replacing with SM 125?",self.replace_with_sm125

            if po.startswith('sm-predictions='):
                sm_pred_path = po.replace('sm-predictions=','')
                self.sm_predictions = json.load(open(sm_pred_path,'r'))
                print "Using %s for SM predictions"%sm_pred_path

            if po.startswith('minTemplateMass='):
                self.minTemplateMass = float(po.replace('minTemplateMass=', ''))
                print "Lower limit for mass histograms: {MINMASS}".format(MINMASS=self.minTemplateMass)

            if po.startswith('maxTemplateMass='):
                self.maxTemplateMass = float(po.replace('maxTemplateMass=', ''))
                print "Upper limit for mass histograms: {MAXMASS}".format(MAXMASS=self.maxTemplateMass)

            if po.startswith('scaleforh='):
                self.scaleforh = float(po.replace('scaleforh=',''))
                print "Additional scale for the light scalar h: {SCALE}".format(SCALE=self.scaleforh)

        self.filename = os.path.join(self.filePrefix, self.modelFile)

    def setModelBuilder(self, modelBuilder):
        # First call the parent class implementation
        PhysicsModel.setModelBuilder(self, modelBuilder)
        # Function to implement the histograms of (mA, tanb) dependent quantities
        self.buildModel()

    def doHistFunc(self, name, hist, varlist):
        dh = ROOT.RooDataHist('dh_%s'%name, 'dh_%s'%name, ROOT.RooArgList(*varlist), ROOT.RooFit.Import(hist))
        hfunc = ROOT.RooHistFunc(name, name, ROOT.RooArgSet(*varlist), dh)
        self.modelBuilder.out._import(hfunc, ROOT.RooFit.RecycleConflictNodes())
        return self.modelBuilder.out.function(name)

    def doHistFuncFromXsecTools(self, higgs, quantity, varlist, production=None):
        # Translator mssm_xs_tools -> TH1D -> RooDataHist
        name  = self.quantity_map[quantity]['name']
        accesskey = self.quantity_map[quantity]['access']
        method = self.quantity_map[quantity]['method']
        if production and higgs:
            name = name.format(HIGGS=higgs, PROD=production)
            accesskey = accesskey.format(HIGGS=higgs, PROD=production)
        elif higgs:
            name = name.format(HIGGS=higgs)
            accesskey = accesskey.format(HIGGS=higgs)
        print "Doing histFunc '%s' with '%s' key for quantity '%s' from mssm_xs_tools..." %(name, accesskey, quantity)

        x_parname = varlist[0].GetName()
        x_binning = self.binning[self.scenario][x_parname]

        y_parname = varlist[1].GetName()
        y_binning = self.binning[self.scenario][y_parname]

        hist = ROOT.TH2D(name, name, len(x_binning)-1, x_binning, len(y_binning)-1, y_binning)
        for i_x, x in enumerate(x_binning):
            for i_y, y in enumerate(y_binning):
                value = getattr(self.mssm_inputs, method)(accesskey, x, y)
                if quantity == 'mass' and self.minTemplateMass:
                    if value < self.minTemplateMass:
                        print "[WARNING]: Found a value for {MH} below lower mass limit: {VALUE} < {MINMASS} for {XNAME} = {XVALUE}, {YNAME} = {YVALUE}. Setting it to limit".format(
                            MH=name,
                            VALUE=value,
                            MINMASS=self.minTemplateMass,
                            XNAME=x_parname,
                            XVALUE=x,
                            YNAME=y_parname,
                            YVALUE=y)
                        value = self.minTemplateMass
                if quantity == 'mass' and self.maxTemplateMass:
                    if value > self.maxTemplateMass:
                        print "[WARNING]: Found a value for {MH} above upper mass limit: {VALUE} > {MINMASS} for {XNAME} = {XVALUE}, {YNAME} = {YVALUE}. Setting it to limit".format(
                            MH=name,
                            VALUE=value,
                            MINMASS=self.maxTemplateMass,
                            XNAME=x_parname,
                            XVALUE=x,
                            YNAME=y_parname,
                            YVALUE=y)
                        value = self.maxTemplateMass
                hist.SetBinContent(i_x+1, i_y+1, value)
        return self.doHistFunc(name, hist, varlist)

    def doHistFuncForQQH(self, varlist):
        # Computing scaling function for qqh contribution (SM-like Higgs) in context of MSSM
        # Assuming qqphi is already scaled to an appropriate SM 125.4 cross-section
        name  = "sf_qqphi_MSSM"

        accesskey = None
        if self.scenario != "mh1125_CPV":
            accesskey = self.quantity_map['yukawa_top']['access'].format(HIGGS='H')
        accesskey_br = self.quantity_map['br']['access'].format(HIGGS=self.smllike)
        accesskey_br_SM = self.quantity_map['br_SM']['access'].format(HIGGS=self.smllike)

        print "Computing 'qqphi' scaling function from xsec tools"

        x_parname = varlist[0].GetName()
        x_binning = self.binning[self.scenario][x_parname]

        y_parname = varlist[1].GetName()
        y_binning = self.binning[self.scenario][y_parname]

        hist = ROOT.TH2D(name, name, len(x_binning)-1, x_binning, len(y_binning)-1, y_binning)
        for i_x, x in enumerate(x_binning):
            for i_y, y in enumerate(y_binning):

                beta = np.arctan(y)

                g_Htt = 1.0 # No Yukawa scale factors for H1 in mh1125_CPV scenario
                value = 1.0 # Initial value

                br_htautau = getattr(self.mssm_inputs, self.quantity_map['br']['method'])(accesskey_br, x, y)
                br_htautau_SM = getattr(self.mssm_inputs, self.quantity_map['br_SM']['method'])(accesskey_br_SM, x, y)

                if accesskey:
                    g_Htt = getattr(self.mssm_inputs, self.quantity_map['yukawa_top']['method'])(accesskey, x, y)
                    sin_alpha = g_Htt * np.sin(beta)
                    if abs(sin_alpha) > 1:
                        sin_alpha  = np.sign(sin_alpha)
                    alpha = np.arcsin(sin_alpha)
                    if self.smlike == 'h':
                        value = np.sin(beta-alpha)**2
                    elif self.smlike == 'H':
                        value = np.cos(beta-alpha)**2

                value *= br_htautau / br_htautau_SM # (g_HVV)**2 * br_htautau(mh) / br_htautau_SM(mh), correcting for mass dependence mh vs. 125.4 GeV
                value *= self.scaleforh # additional manual rescaling of light scalar h (default is 1.0)
                hist.SetBinContent(i_x+1, i_y+1, value)

        return self.doHistFunc(name, hist, varlist)

    def doHistFuncForGGH(self, varlist):
        # Computing scaling function for ggphi contribution (SM-like Higgs) in context of MSSM
        # Assuming ggphi is already scaled to an appropriate SM 125.4 cross-section
        name  = "sf_ggphi_MSSM"

        accesskey_xs = self.quantity_map['xsec']['access'].format(HIGGS=self.smlike,PROD='gg')
        accesskey_xs_SM = self.quantity_map['xsec_SM']['access'].format(HIGGS=self.smlike,PROD='gg')
        accesskey_br = self.quantity_map['br']['access'].format(HIGGS=self.smlike)
        accesskey_br_SM = self.quantity_map['br_SM']['access'].format(HIGGS=self.smlike)

        print "Computing 'ggphi' scaling function from xsec tools"

        x_parname = varlist[0].GetName()
        x_binning = self.binning[self.scenario][x_parname]

        y_parname = varlist[1].GetName()
        y_binning = self.binning[self.scenario][y_parname]

        hist = ROOT.TH2D(name, name, len(x_binning)-1, x_binning, len(y_binning)-1, y_binning)
        for i_x, x in enumerate(x_binning):
            for i_y, y in enumerate(y_binning):

                xs_ggh = getattr(self.mssm_inputs, self.quantity_map['xsec']['method'])(accesskey_xs, x, y)
                xs_ggh_SM = getattr(self.mssm_inputs, self.quantity_map['xsec_SM']['method'])(accesskey_xs_SM, x, y)

                br_htautau = getattr(self.mssm_inputs, self.quantity_map['br']['method'])(accesskey_br, x, y)
                br_htautau_SM = getattr(self.mssm_inputs, self.quantity_map['br_SM']['method'])(accesskey_br_SM, x, y)

                value =  xs_ggh / xs_ggh_SM * br_htautau / br_htautau_SM # xs(mh) * BR(mh) / (xs_SM(mh) * BR_SM(mh)) correcting for mass dependence mh vs. 125.4 GeV
                value *= self.scaleforh # additional manual rescaling of light scalar h (default is 1.0)
                hist.SetBinContent(i_x+1, i_y+1, value)

        return self.doHistFunc(name, hist, varlist)

    def doHistFuncForBBH(self, varlist):
        # Computing scaling function for bbphi contribution (SM-like Higgs) in context of MSSM
        # Assuming bbphi is **NOT** scaled to an appropriate SM 125.4 cross-section & BR, but to 1 pb
        name  = "sf_bbphi_MSSM"

        accesskey_xs = self.quantity_map['xsec']['access'].format(HIGGS=self.smlike,PROD='bb')
        accesskey_xs_SM = self.quantity_map['xsec_SM']['access'].format(HIGGS=self.smlike,PROD='bb')
        accesskey_br = self.quantity_map['br']['access'].format(HIGGS=self.smlike)
        accesskey_br_SM = self.quantity_map['br_SM']['access'].format(HIGGS=self.smlike)

        xs_bbh_SM125 = self.sm_predictions["xs_bb_SMH125"]
        br_htautau_SM125 = self.sm_predictions["br_SMH125_tautau"]

        print "Computing 'bbphi' scaling function from xsec tools"

        x_parname = varlist[0].GetName()
        x_binning = self.binning[self.scenario][x_parname]

        y_parname = varlist[1].GetName()
        y_binning = self.binning[self.scenario][y_parname]

        hist = ROOT.TH2D(name, name, len(x_binning)-1, x_binning, len(y_binning)-1, y_binning)
        for i_x, x in enumerate(x_binning):
            for i_y, y in enumerate(y_binning):

                xs_bbh = getattr(self.mssm_inputs, self.quantity_map['xsec']['method'])(accesskey_xs, x, y)
                xs_bbh_SM = getattr(self.mssm_inputs, self.quantity_map['xsec_SM']['method'])(accesskey_xs_SM, x, y)

                br_htautau = getattr(self.mssm_inputs, self.quantity_map['br']['method'])(accesskey_br, x, y)
                br_htautau_SM = getattr(self.mssm_inputs, self.quantity_map['br_SM']['method'])(accesskey_br_SM, x, y)

                # xs(mh) * (xs_SM(125.4)/xs_SM(mh)) * BR(mh) * (BR_SM(125.4)/BR_SM(mh)) correcting for mass dependence mh vs. 125.4 GeV
                value =  xs_bbh * (xs_bbh_SM125 / xs_bbh_SM) * br_htautau * (br_htautau_SM125 / br_htautau_SM)
                value *= self.scaleforh # additional manual rescaling of light scalar h (default is 1.0)
                hist.SetBinContent(i_x+1, i_y+1, value)

        return self.doHistFunc(name, hist, varlist)

    def doAsymPowSystematic(self, higgs, quantity, varlist, production, uncertainty):
        # Translator mssm_xs_tools -> TH1D -> RooDataHist -> Systematic
        name  = self.quantity_map[quantity]['name'].format(HIGGS=higgs, PROD=production)
        accesskey = self.quantity_map[quantity]['access'].format(HIGGS=higgs, PROD=production)
        uncertaintykey = self.uncertainty_map[production+uncertainty]
        method = self.quantity_map[quantity]['method']

        # create AsymPow rate scaler given two TH2 inputs corresponding to kappa_hi and kappa_lo
        param = name + "_MSSM_" + uncertainty
        self.modelBuilder.doVar('%s[0,-7,7]'%param)
        param_var = self.modelBuilder.out.var(param)
        systname = "systeff_%s"%param

        x_parname = varlist[0].GetName()
        x_binning = self.binning[self.scenario][x_parname]

        y_parname = varlist[1].GetName()
        y_binning = self.binning[self.scenario][y_parname]

        hist_hi = ROOT.TH2D(systname+"_hi", systname+"_hi", len(x_binning)-1, x_binning, len(y_binning)-1, y_binning)
        hist_lo = ROOT.TH2D(systname+"_lo", systname+"_lo", len(x_binning)-1, x_binning, len(y_binning)-1, y_binning)
        for i_x, x in enumerate(x_binning):
            for i_y, y in enumerate(y_binning):
                nominal  = getattr(self.mssm_inputs, method)(accesskey, x, y)
                value_hi = getattr(self.mssm_inputs, method)(accesskey+uncertaintykey.format(VAR='up'), x, y)
                value_lo = getattr(self.mssm_inputs, method)(accesskey+uncertaintykey.format(VAR='down'), x, y)
                if nominal == 0:
                    hist_hi.SetBinContent(i_x+1, i_y+1, 1.0)
                    hist_lo.SetBinContent(i_x+1, i_y+1, 1.0)
                else:
                    hist_hi.SetBinContent(i_x+1, i_y+1, (nominal+value_hi)/nominal)
                    hist_lo.SetBinContent(i_x+1, i_y+1, (nominal+value_lo)/nominal)
        print "Doing AsymPow systematic '%s' with '%s' key for quantity '%s' from mssm_xs_tools..." %(param, accesskey+uncertaintykey.format(VAR='up/down'), quantity)

        self.NUISANCES.add(param)
        hi = self.doHistFunc('%s_hi'%systname, hist_hi, varlist)
        lo = self.doHistFunc('%s_lo'%systname, hist_lo, varlist)
        asym = ROOT.AsymPow(systname, '', lo, hi, param_var)
        self.modelBuilder.out._import(asym)
        return self.modelBuilder.out.function(systname)

    def add_ggH_at_NLO(self, name, X):

        template_X = X

        if self.scenario == "mh1125_CPV":
            fractions_sm = ROOT.TFile.Open(self.ggHatNLO, 'read')
            w_sm = fractions_sm.Get("w")
            mPhi = w_sm.var("m{HIGGS}".format(HIGGS=self.cpv_template_pairs[X]))
            mPhi.SetName("m{HIGGS}".format(HIGGS=X))

            template_X = self.cpv_template_pairs[X]

        importstring = os.path.expandvars(self.ggHatNLO)+":w:gg{X}_{LC}_MSSM_frac" #import t,b,i fraction of xsec at NLO
        for loopcontrib in ['t','b','i']:
            getattr(self.modelBuilder.out, 'import')(importstring.format(X=template_X, LC=loopcontrib), ROOT.RooFit.RecycleConflictNodes())
            self.modelBuilder.out.factory('prod::%s(%s,%s)' % (name.format(X=X, LC="_"+loopcontrib), name.format(X=X, LC=""), "gg%s_%s_MSSM_frac" % (template_X,loopcontrib))) #multiply t,b,i fractions with xsec at NNLO

    def preProcessNuisances(self,nuisances):
        doParams = set()
        for bin in self.DC.bins:
            for proc in self.DC.exp[bin].keys():
                if self.DC.isSignal[proc]:
                    scaling = 'scaling_%s' % proc
                    params = self.modelBuilder.out.function(scaling).getParameters(ROOT.RooArgSet()).contentsString().split(',')
                    for param in params:
                        if param in self.NUISANCES:
                            doParams.add(param)
        for param in doParams:
            print 'Add nuisance parameter %s to datacard' % param
            nuisances.append((param,False, "param", [ "0", "1"], [] ) )

    def doParametersOfInterest(self):
        """Create POI and other parameters, and define the POI set."""
        self.modelBuilder.doVar("r[1,0,20]")

        #MSSMvsSM
        self.modelBuilder.doVar("x[1,0,1]")
        self.modelBuilder.out.var('x').setConstant(True)
        self.modelBuilder.factory_("expr::not_x(\"(1-@0)\", x)")
        self.sigNorms = { True:'x', False:'not_x' }

        self.modelBuilder.doSet('POI', 'r')

        # We don't intend on actually floating these in any fits...
        self.modelBuilder.out.var(self.massparameter).setConstant(True) # either mA or mHp
        self.modelBuilder.out.var('tanb').setConstant(True)

        bsm_proc_match = "(gg(A|H|h|H3|H2|H1)_(t|i|b)|bb(A|H|h|H3|H2|H1))^"
        if self.replace_with_sm125:
            bsm_proc_match = "(gg(A|{BSMSCALAR}|H3|H2)_(t|i|b)|bb(A|{BSMSCALAR}|H3|H2))^".format(BSMSCALAR=self.bsmscalar).replace("||","|") # need the last fix in case BSMSCALAR=""

        for proc in self.PROC_SETS:
            if re.match(bsm_proc_match, proc): # not SM-like BSMSCALAR: either h or H
                X = proc.split('_')[0].replace('gg','').replace('bb','')
                terms = ['xs_%s' %proc, 'br_%stautau'%X]
                terms += ['r']
                terms += [self.sigNorms[True]]
            elif re.match('(qq{SMLIKE}|Z{SMLIKE}|W{SMLIKE})^'.format(SMLIKE=self.smlike), proc): # always done
                terms = [self.sigNorms[True], 'r', 'sf_qqphi_MSSM']
            elif re.match('gg{SMLIKE}^'.format(SMLIKE=self.smlike), proc): # considered, in case it is not in the first 'if' case
                terms = [self.sigNorms[True], 'r', 'sf_ggphi_MSSM']
            elif re.match('bb{SMLIKE}^'.format(SMLIKE=self.smlike), proc): # considered, in case it is not in the first 'if' case
                terms = [self.sigNorms[True], 'r', 'sf_bbphi_MSSM']
            elif "125" in proc:
                terms = [self.sigNorms[False]]

            if self.scenario == "mh1125_CPV" and X in ['H2', 'H3']:
                for xx in ['bb', 'gg']:
                    if xx in proc:
                        terms.append('expr::interference_{PROD}_{HIGGS}(\"1.0 + @0\", int_{PROD}_tautau_{HIGGS})'.format(PROD=xx, HIGGS=X))

            # Now scan terms and add theory uncerts
            extra = []
            for term in terms:
                if term in self.SYST_DICT:
                    extra += self.SYST_DICT[term]
            terms += extra
            self.modelBuilder.factory_('prod::scaling_%s(%s)'%(proc,','.join(terms)))
            self.modelBuilder.out.function('scaling_%s'%proc).Print('')

    def getYieldScale(self,bin,process):
        if self.DC.isSignal[process]:
            scaling = 'scaling_%s' % process
            print 'Scaling %s/%s as %s' % (bin, process, scaling)
            return scaling
        else:
            return 1

    def buildModel(self):
        mass = ROOT.RooRealVar(self.massparameter, 'm_{A} [GeV]' if self.massparameter == 'mA' else 'm_{H^{+}} [GeV]', 160.) # the other case would be 'mHp'
        tanb = ROOT.RooRealVar('tanb', 'tan#beta', 5.5)
        pars = [mass, tanb]

        self.mssm_inputs = mssm_xs_tools(self.filename, True, 1) # syntax: model filename, Flag for interpolation ('True' or 'False'), verbosity level

        # qqphi, Zphi and Wphi  added always in this setup
        self.doHistFuncForQQH(pars)
        self.PROC_SETS.extend(['qq'+self.smlike, 'Z'+self.smlike, 'W'+self.smlike])

        # adding ggphi & bbphi as 125 templates only if requested
        if self.replace_with_sm125:

            self.doHistFuncForGGH(pars)
            self.PROC_SETS.append('gg'+self.smlike)

            self.doHistFuncForBBH(pars)
            self.PROC_SETS.append('bb'+self.smlike)

        procs = ['H1', 'H2', 'H3'] if self.scenario == "mh1125_CPV" else ['h', 'H', 'A']

        for X in procs:
            if self.massparameter.replace('m','') == X: # don't create histogram for 'A' in cases, where its mass is a model-parameter
                continue
            self.doHistFuncFromXsecTools(X, "mass", pars) # syntax: Higgs-Boson, mass attribute, parameters

        for X in procs:
            self.doHistFuncFromXsecTools(X, "yukawa_top", pars)
            self.doHistFuncFromXsecTools(X, "yukawa_bottom", pars)

            self.doHistFuncFromXsecTools(X, "br", pars) # syntax: Higgs-Boson, xsec attribute, parameters, production mode

            self.doHistFuncFromXsecTools(X, "xsec", pars, production="gg") # syntax: Higgs-Boson, xsec attribute, parameters, production mode
            self.doHistFuncFromXsecTools(X, "xsec", pars, production="bb") # syntax: Higgs-Boson, xsec attribute, parameters, production mode

            if self.scenario == "mh1125_CPV":
                self.doHistFuncFromXsecTools(X, "interference", pars, production="gg")
                self.doHistFuncFromXsecTools(X, "interference", pars, production="bb")

            self.add_ggH_at_NLO('xs_gg{X}{LC}', X)

            # ggH scale uncertainty
            self.doAsymPowSystematic(X, "xsec", pars, "gg", "scale")
            # ggH pdf+alpha_s uncertainty
            self.doAsymPowSystematic(X, "xsec", pars, "gg", "pdfas")
            # bbH total uncertainty
            self.doAsymPowSystematic(X, "xsec", pars, "bb", "total")

            for loopcontrib in ['t','b','i']:
                self.SYST_DICT['xs_gg%s_%s' % (X, loopcontrib)].append('systeff_xs_gg%s_MSSM_scale' %X)
                self.SYST_DICT['xs_gg%s_%s' % (X, loopcontrib)].append('systeff_xs_gg%s_MSSM_pdfas' %X)

            self.SYST_DICT['xs_bb%s' %X].append('systeff_xs_bb%s_MSSM_total' %X)

            # Make a note of what we've built, will be used to create scaling expressions later
            self.PROC_SETS.append('bb%s'%X)
            self.PROC_SETS.extend(['gg%s_t'%X, 'gg%s_b'%X, 'gg%s_i'%X])

        # Add BSM systematic also in case SM125 templates are used for ggphi and bbphi
        if self.replace_with_sm125:
             self.SYST_DICT["sf_ggphi_MSSM"].append('systeff_xs_gg%s_MSSM_scale' %self.smlike)
             self.SYST_DICT["sf_ggphi_MSSM"].append('systeff_xs_gg%s_MSSM_pdfas' %self.smlike)
             self.SYST_DICT["sf_bbphi_MSSM"].append('systeff_xs_bb%s_MSSM_total' %self.smlike)

        # And the SM terms
        self.PROC_SETS.extend(['ggH125', 'qqH125', 'ZH125', 'WH125', 'bbH125'])


MSSMvsSM = MSSMvsSMHiggsModel()
