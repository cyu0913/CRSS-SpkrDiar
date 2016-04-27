#!/bin/bash

log_start(){
  echo "#####################################################################"
  echo "Spawning *** $1 *** on" `date` `hostname`
  echo ---------------------------------------------------------------------
}

log_end(){
  echo ---------------------------------------------------------------------
  echo "Done *** $1 *** on" `date` `hostname`
  echo "#####################################################################"
}

. cmd.sh
. path.sh

set -e # exit on error

run_mfcc(){
    mfccdir=mfcc
    for x in toy; do
      steps/make_mfcc.sh --cmd "$train_cmd" --nj 1 data/$x exp/make_mfcc/$x $mfccdir || exit 1;
      steps/compute_cmvn_stats.sh data/$x exp/make_mfcc/$x $mfccdir || exit 1;
      #utils/fix_data_dir.sh data/$x
    done
}
#run_mfcc

run_vad(){

    log_start "Doing VAD"	

    vaddir=exp/vad	
    sid/compute_vad_decision.sh --nj 1 data/toy $vaddir/log $vaddir 	

    log_end "Finish VAD"	
}
#run_vad

test_changedetection() {
    changeDetectBIC scp:data/toy/feats.scp ark:local/label.ark ark,scp,t:./tmp.ark,./tmp.scp
}
#test_changedetection

test_ivectors(){
    sid/test_ivector_score.sh --nj 1 exp/extractor_1024 data/toy local/label.ark exp/test_seg_ivector
}
#test_ivectors;

IvectorExtract(){
    sid/extract_segment_ivector.sh --nj 1 exp/extractor_1024 data/toy local/label.ark exp/segment_ivectors
}
#IvectorExtract

glpk_dir=exp/glpk
test_glpkIlpTemplate(){
   mkdir -p $glpk_dir; rm -rf $glpk_dir/*; mkdir -p exp/segment.true; rm -f exp/segment.true/*

   labelToSegment ark:local/label.ark exp/segment.true	

   sid/generate_ILP_template.sh --nj 1 --delta 25 exp/extractor_1024 data/toy exp/segment.true/segments.scp $glpk_dir
   glpsol --lp $glpk_dir/ilp.template -o $glpk_dir/glp.sol
}
test_glpkIlpTemplate

rttm_dir_true=exp/rttm.true
rttm_dir_est=exp/rttm.est
test_DER(){
   mkdir -p $rttm_dir_true; rm -f $rttm_dir_true/*
   mkdir -p $rttm_dir_est; rm -f $rttm_dir_est/*

   labelToRTTM ark:local/label.ark $rttm_dir_est	
   glpkToRTTM $glpk_dir/glp.sol exp/segment.true/segments.scp $rttm_dir_true	
   perl local/md-eval-v21.pl -r $rttm_dir_true/IS1000b.Mix-Headset.rttm -s $rttm_dir_est/IS1000b.Mix-Headset.rttm	
}
test_DER