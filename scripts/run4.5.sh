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

eval_data="12_meeting" # data for diarization
dev_data="ami_full"  # dev_data is for UBM, TV matrix training for i-vector

run_mfcc(){
    log_start "Extract MFCC features"

    mfccdir=mfcc
    for x in $data; do
      steps/make_mfcc.sh --cmd "$train_cmd" --nj 1 data/$x exp/make_mfcc/$x $mfccdir || exit 1;
      steps/compute_cmvn_stats.sh data/$x exp/make_mfcc/$x $mfccdir || exit 1;
    done

    log_end "Extract MFCC features"
}
#run_mfcc 


run_vad(){
    log_start "Doing VAD"
    for x in $dev_data; do
     vaddir=exp/vad/$x
     diar/compute_vad_decision.sh --nj 1 data/$x $vaddir/log $vaddir
    done
    log_end "Finish VAD"
}
#run_vad


make_ref(){
    log_start "Generate Reference Segments/Labels/RTTM files"

    ami_annotated_segment=/home/chengzhu/work/SpeechCorpus/ami_dir/segments

    local/make_ami_ref.sh data/$data $ami_annotated_segment exp/ref/$eval_data

    log_end "Generate Reference Segments/Labels/RTTM files"
}
#make_ref 

train_extractor(){
    ubmdim=256
    ivdim=32

    sid/train_diag_ubm.sh --parallel-opts "" --nj 1 --apply-cmvn-utterance false --apply-cmvn-sliding false \
    	 		--cmd "$train_cmd" data/$dev_data ${ubmdim} exp/diag_ubm_${ubmdim} || exit 1;

    sid/train_full_ubm.sh --nj 1 --apply-cmvn-utterance false --apply-cmvn-sliding false \
			--cmd "$train_cmd" data/$dev_data exp/diag_ubm_${ubmdim} exp/full_ubm_${ubmdim} || exit 1;

    sid/train_ivector_extractor.sh --nj 1 --cmd "$train_cmd" --num-gselect 15 \
      --ivector-dim $ivdim --num-iters 5 exp/full_ubm_${ubmdim}/final.ubm data/$dev_data \
      exp/extractor_$ubmdim || exit 1;
}
#train_extractor

bottom_up_clustering(){
    log_start "Bottom Up Clustering With Ivector"

    diar/segment_clustering_ivector.sh --nj 1 --use-segment-label false --apply-cmvn-utterance false --apply-cmvn-sliding false \
       --ivector-dist-stop 0.4 exp/ref/$eval_data/segments exp/extractor_256 data/$eval_data exp/clustering_ivector/$eval_data

    log_end "Bottom Up Clustering With Ivector"
}
#bottom_up_clustering

ilp_clustering(){
    log_start "ILP Clustering With Ivector"

    diar/construct_ilp_problem.sh --nj 1 --use-segment-label true --delta 1.8 --apply-cmvn-utterance false --apply-cmvn-sliding false \
	exp/clustering_ivector/$eval_data/segments exp/extractor_256 data/$eval_data exp/clustering_ilp/$eval_data	

    diar/ilp_clustering.sh --use-segment-label true exp/clustering_ivector/$eval_data/segments exp/clustering_ilp/$eval_data	

    log_end "ILP Clustering With Ivector"
}
ilp_clustering

compute_der(){
	
    diar/compute_DER.sh --sanity_check false exp/ref/$eval_data/rttms exp/clustering_ilp/$eval_data/rttms exp/result_DER/$eval_data	
    grep OVERALL exp/result_DER/$eval_data/*.der && grep OVERALL exp/result_DER/$eval_data/*.der | awk '{ sum += $7; n++ } END { if (n > 0) print "Avergage: " sum / n; }'		

}
compute_der

