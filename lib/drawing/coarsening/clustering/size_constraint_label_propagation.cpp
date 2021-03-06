/******************************************************************************
 * size_constraint_label_propagation.cpp 
 *
 * Source of KaDraw -- Karlsruhe Graph Drawing 
 ******************************************************************************
 * Copyright (C) 2015 Christian Schulz <christian.schulz@kit.edu>
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 *****************************************************************************/

#include <unordered_map>
#include <sstream>
#include "data_structure/union_find.h"
#include "node_ordering.h"
#include "tools/random_functions.h"
#include "io/graph_io.h"

#include "size_constraint_label_propagation.h"

size_constraint_label_propagation::size_constraint_label_propagation() {
                
}

size_constraint_label_propagation::~size_constraint_label_propagation() {
                
}

void size_constraint_label_propagation::match(const Config & config, 
                                              graph_access & G, 
                                              Matching & _matching, 
                                              CoarseMapping & coarse_mapping, 
                                              NodeID & no_of_coarse_vertices,
                                              NodePermutationMap & permutation) {
        permutation.resize(G.number_of_nodes());
        coarse_mapping.resize(G.number_of_nodes());
        no_of_coarse_vertices = 0;

        match_internal(config, G, _matching, coarse_mapping, no_of_coarse_vertices, permutation);
        G.set_partition_count(no_of_coarse_vertices);
}

void size_constraint_label_propagation::match_internal(const Config & config, 
                                              graph_access & G, 
                                              Matching & _matching, 
                                              CoarseMapping & coarse_mapping, 
                                              NodeID & no_of_coarse_vertices,
                                              NodePermutationMap & permutation) {

        std::vector<NodeWeight> cluster_id(G.number_of_nodes());
        NodeWeight block_upperbound = ceil(config.upper_bound_partition);

        label_propagation( config, G, block_upperbound, cluster_id, no_of_coarse_vertices);
        create_coarsemapping( config, G, cluster_id, coarse_mapping);
}

void size_constraint_label_propagation::label_propagation(const Config & config, 
                                                         graph_access & G, 
                                                         std::vector<NodeWeight> & cluster_id, 
                                                         NodeID & no_of_blocks ) {
        NodeWeight block_upperbound = ceil(config.upper_bound_partition);

        label_propagation( config, G, block_upperbound, cluster_id, no_of_blocks);
}

void size_constraint_label_propagation::label_propagation(const Config & config, 
                                                         graph_access & G, 
                                                         const NodeWeight & block_upperbound,
                                                         std::vector<NodeWeight> & cluster_id,  
                                                         NodeID & no_of_blocks) {
        // in this case the _matching paramter is not used 
        // coarse_mappng stores cluster id and the mapping (it is identical)
        std::vector<PartitionID> hash_map(G.number_of_nodes(),0);
        std::vector<NodeID> permutation(G.number_of_nodes());
        std::vector<NodeWeight> cluster_sizes(G.number_of_nodes());
        cluster_id.resize(G.number_of_nodes());

        forall_nodes(G, node) {
                cluster_sizes[node] = G.getNodeWeight(node);
                cluster_id[node]    = node;
        } endfor
        
        node_ordering n_ordering;
        n_ordering.order_nodes(config, G, permutation);

        for( int j = 0; j < config.label_iterations; j++) {
                unsigned int change_counter = 0;
                forall_nodes(G, i) {
                        NodeID node = permutation[i];
                        //now move the node to the cluster that is most common in the neighborhood

                        forall_out_edges(G, e, node) {
                                NodeID target = G.getEdgeTarget(e);
                                hash_map[cluster_id[target]]+=G.getEdgeWeight(e);
                        } endfor

                        //second sweep for finding max and resetting array
                        PartitionID max_block = cluster_id[node];
                        PartitionID my_block  = cluster_id[node];

                        PartitionID max_value = 0;
                        forall_out_edges(G, e, node) {
                                NodeID target             = G.getEdgeTarget(e);
                                PartitionID cur_block     = cluster_id[target];
                                PartitionID cur_value     = hash_map[cur_block];
                                if((cur_value > max_value  || (cur_value == max_value && random_functions::nextBool())) 
                                && (cluster_sizes[cur_block] + G.getNodeWeight(node) <= block_upperbound || cur_block == my_block))
                                {
                                        max_value = cur_value;
                                        max_block = cur_block;
                                }

                                hash_map[cur_block] = 0;
                        } endfor

                        cluster_sizes[cluster_id[node]]  -= G.getNodeWeight(node);
                        cluster_sizes[max_block]         += G.getNodeWeight(node);
                        change_counter                   += (cluster_id[node] != max_block);
                        cluster_id[node]                  = max_block;
                } endfor
        }

        remap_cluster_ids( config, G, cluster_id, no_of_blocks);
}



void size_constraint_label_propagation::create_coarsemapping(const Config & config, 
                                                             graph_access & G,
                                                             std::vector<NodeWeight> & cluster_id,
                                                             CoarseMapping & coarse_mapping) {
        forall_nodes(G, node) {
                coarse_mapping[node] = cluster_id[node];
        } endfor
}

void size_constraint_label_propagation::remap_cluster_ids(const Config & config, 
                                                          graph_access & G,
                                                          std::vector<NodeWeight> & cluster_id,
                                                          NodeID & no_of_coarse_vertices, bool apply_to_graph) {

        PartitionID cur_no_clusters = 0;
        std::unordered_map<PartitionID, PartitionID> remap;
        forall_nodes(G, node) {
                PartitionID cur_cluster = cluster_id[node];
                //check wether we already had that
                if( remap.find( cur_cluster ) == remap.end() ) {
                        remap[cur_cluster] = cur_no_clusters++;
                }

                cluster_id[node] = remap[cur_cluster];
        } endfor

        if( apply_to_graph ) {
                forall_nodes(G, node) {
                        G.setPartitionIndex(node, cluster_id[node]);
                } endfor
                G.set_partition_count(cur_no_clusters);
        }

        no_of_coarse_vertices = cur_no_clusters;
}

