// diarbin/ivectorTest.cc
#include <vector>
#include <string>
#include "util/common-utils.h"
#include "base/kaldi-common.h"
#include "ivector/ivector-extractor.h"
#include "diar/ilp.h"
#include "gmm/am-diag-gmm.h"
#include "hmm/posterior.h"
#include "diar/diar-utils.h"
#include "diar/segment.h"


int main(int argc, char *argv[]) {
	typedef kaldi::int32 int32;
	using namespace kaldi;

	const char *usage = "Ivector Test \n";

    kaldi::ParseOptions po(usage);
	po.Read(argc, argv);

	if (po.NumArgs() != 4) {
        po.PrintUsage();
        exit(1);
    }

    std::string label_rspecifier = po.GetArg(1),
                feature_rspecifier = po.GetArg(2),
                posterior_rspecifier = po.GetArg(3),
                ivector_extractor_rxfilename = po.GetArg(4);

    SequentialBaseFloatVectorReader label_reader(label_rspecifier);
    SequentialBaseFloatMatrixReader feature_reader(feature_rspecifier);
    RandomAccessPosteriorReader posterior_reader(posterior_rspecifier);
    IvectorExtractor extractor;
    ReadKaldiObject(ivector_extractor_rxfilename, &extractor);

    BaseFloat true_score=0.0;
    int32 true_count=0;
    BaseFloat false_score=0.0;
    int32 false_count=0;
    size_t loop_max = 50;

    for (; !label_reader.Done(); label_reader.Next()) {
        string uttid = label_reader.Key();
        SegmentCollection all_segments(label_reader.Value(), uttid);
        SegmentCollection speech_segments = all_segments.GetSpeechSegments().GetLargeSegments(50);
        
        std::vector< Vector<double> > ivector_collect;
        // set ivector for each segment
        for (size_t k = 0; k<speech_segments.Size(); k++) {
            Segment* kth_segment = speech_segments.KthSegment(k);
            kth_segment->SetIvector(feature_reader.Value(), 
                                    posterior_reader.Value(uttid), 
                                    extractor);

            /*
            // perform length normalization
            BaseFloat norm = kth_segment->Ivector().Norm(2.0);
            BaseFloat ratio = norm / sqrt(kth_segment->Ivector().Dim()); // how much larger it is
                                                    // than it would be, in
                                                    // expectation, if normally
                                                    // distributed.
            if (ratio == 0.0) {
                KALDI_WARN << "Zero iVector";
            } else {
                Vector<double> kth_ivector = kth_segment->Ivector();
                kth_ivector.Scale(1.0 / ratio);
                kth_segment->SetIvector(kth_ivector);
            }
            */

            ivector_collect.push_back(kth_segment->Ivector());
        }

        // compute mean of ivectors
        Vector<double> total_mean;
        computeMean(ivector_collect, total_mean);

        // Mean Normalization & length Normalization
        for (size_t k = 0; k<speech_segments.Size(); k++) {
            // mean normalization
            Segment* kth_segment = speech_segments.KthSegment(k);
            Vector<double> kth_ivector = kth_segment->Ivector();
            kth_ivector.AddVec(-1, total_mean);

            // length normalization
            BaseFloat norm = kth_ivector.Norm(2.0);
            BaseFloat ratio = norm / sqrt(kth_ivector.Dim()); // how much larger it is
            kth_ivector.Scale(1.0 / ratio);

            kth_segment->SetIvector(kth_ivector);
        }

        SpMatrix<double> total_cov;
        ComputeCovariance(ivector_collect, total_cov);
 
        // cross comparison 
        for (size_t i=0; i<loop_max;i++){
            Segment* ith_segment = speech_segments.KthSegment(i);
            Vector<double> i_ivector = ith_segment->Ivector();
            for (size_t j=0; j<loop_max;j++){
                Segment* jth_segment = speech_segments.KthSegment(j); 
                Vector<double> j_ivector = jth_segment->Ivector();

                std::string i_label = ith_segment->Label();
                std::string j_label = jth_segment->Label();

                if (i != j && (i_label == j_label) && i_label != "nonspeech" &&
                         i_label != "overlap" && j_label != "nonspeech" && j_label != "overlap") {
                    BaseFloat dot_product = 1 - cosineDistance(i_ivector, j_ivector);
                    BaseFloat distance = mahalanobisDistance(i_ivector, j_ivector, total_cov);
                    true_score += dot_product; true_count++;
                    KALDI_LOG << "TRUE Mahalanobis scores: " << distance;
                    KALDI_LOG << "TRUE Cosine scores: " << dot_product;
                }
                if (i != j && i_label != j_label && i_label != "nonspeech" &&
                        i_label != "overlap" && j_label != "nonspeech" && j_label != "overlap") {
                    BaseFloat dot_product =  1 - cosineDistance(i_ivector, j_ivector);
                    BaseFloat distance = mahalanobisDistance(i_ivector, j_ivector, total_cov);
                    false_score += dot_product; false_count++;
                    KALDI_LOG << "FALSE Mahalanobis scores: " << distance;
                    KALDI_LOG << "FALSE Cosine scores: " << dot_product;
                }
            }
        }
        //feature_reader.Next();
    }
    KALDI_LOG << "Total Sum Of TRUE Target Score: " << true_score/true_count;
    KALDI_LOG << "Total Sum Of False Detection Score: " << false_score/false_count;
    KALDI_LOG << "Count of TRUE Target: " << true_count;
    KALDI_LOG << "Count Of FALSE Target: " << false_count;
}