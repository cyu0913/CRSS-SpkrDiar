#ifndef KALDI_SRC_DIAR_CLUSTERING_H_
#define KALDI_SRC_DIAR_CLUSTERING_H_

#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include "util/common-utils.h"
#include "matrix/matrix-lib.h"
#include "diar-utils.h"


namespace kaldi{

class Cluster {
public:
	Cluster(Segment one_segment);
	void AddSegment(Segment new_segment);
	std::vector<Segment> AllSegments() const;
	std::string Label() const;
	int32 NumFrames() const;
	int32 NumSegments() const;
	Segment KthSegment(int32 k) const;
	BaseFloat LogDet(const Matrix<BaseFloat> &feats) const; 
	Cluster* prev;
	Cluster* next;

	static int id_generator;
	static string prefix;

private:
	std::vector<Segment> list_;
	std::string label_;
	int32 frames_;
};


class ClusterCollection {
public:
	ClusterCollection();
	string UttID();
	void InitFromNonLabeledSegments(SegmentCollection non_clustered_segmemts);
	//InitFromLabeledSegments(SegmentCollection);
	void BottomUpClustering(const Matrix<BaseFloat> &feats, int32 target_cluster_num);
	void FindMinDistClusters(const Matrix<BaseFloat> &feats, std::vector<Cluster*> &min_dist_clusters);
	static void MergeClusters(Cluster* clust1, Cluster* clust2);
	BaseFloat DistanceOfTwoClusters(const Matrix<BaseFloat> &feats, const Cluster* cluster1, const Cluster* cluster2);
	void Write(const std::string& segment_dir);
	Cluster* Head();

private:
	string uttid_;
	int32 num_clusters_;
	Cluster* head_cluster_;
};


template <typename T>
std::string ToString(T val)
{
    std::stringstream stream;
    stream << val;
    return stream.str();
}

}

#endif
