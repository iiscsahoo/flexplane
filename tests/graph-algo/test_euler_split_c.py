'''
Created on October 23, 2013

@author: aousterh
'''
import sys
import unittest

sys.path.insert(0, '../../bindings/graph-algo')
sys.path.insert(0, '../../src/graph-algo')

import networkx as nx
from graph_util import graph_util
import random
import eulersplit
import graph

class Test(unittest.TestCase):

    def test_keeps_edge_set(self):
        generator = graph_util()
        for deg in xrange(2, 11, 2):
            for n_side in xrange(2*deg+4,33,7):
                g_p = generator.generate_random_regular_bipartite(n_side, deg)
                
                # Create the graph in C
                g_c = graph.create_graph_test(n_side);
                g_c_copy = graph.create_graph_test(n_side)
                for edge in g_p.edges_iter():
                    graph.add_edge_test(g_c, edge[0], edge[1])
                    graph.add_edge_test(g_c_copy, edge[0], edge[1])
                self.assertEqual(graph.get_max_degree_test(g_c), deg)
                self.assertEqual(graph.get_max_degree_test(g_c_copy), deg)
 
                # Create the output graphs
                g1_c = graph.create_graph_test(n_side)
                g2_c = graph.create_graph_test(n_side)

                eulersplit.split_test(g_c_copy, g1_c, g2_c)

                # Check that the input graph is now empty
                self.assertEqual(graph.get_max_degree_test(g_c_copy), 0)
                for node in xrange(2 * n_side):
                    self.assertEqual(graph.get_degree_test(g_c_copy, node), 0)

                # Check that graphs have the correct degree
                self.assertEqual(graph.get_max_degree_test(g1_c), deg / 2)
                self.assertEqual(graph.get_max_degree_test(g2_c), deg / 2)

                # Check that vertices have the correct degree
                for node in xrange(2 * n_side):
                    self.assertEqual(graph.get_degree_test(g1_c, node), deg / 2)
                    self.assertEqual(graph.get_degree_test(g2_c, node), deg / 2)

                # Check that the combination of the two graphs equals the original graph
                graph.add_graph_test(g1_c, g2_c)
                self.assertTrue(graph.are_equal_test(g_c, g1_c))
                   
                graph.destroy_graph_test(g_c)
                graph.destroy_graph_test(g_c_copy)
                graph.destroy_graph_test(g1_c)
                graph.destroy_graph_test(g2_c)

        pass


    def test_irregular_graphs(self):
        generator = graph_util()
        for max_deg in xrange(2, 11, 2):
            for n_side in xrange(2 * max_deg + 4,33,7):
                for e in xrange(4, n_side * max_deg - 4, 10):
                    g_p = generator.generate_random_even_degree_bipartite(n_side, max_deg, e)

                    # Create the graph in C
                    g_c = graph.create_graph_test(n_side);
                    g_c_copy = graph.create_graph_test(n_side)
                    for edge in g_p.edges_iter():
                        graph.add_edge_test(g_c, edge[0], edge[1])
                        graph.add_edge_test(g_c_copy, edge[0], edge[1])
                    # Create the output graphs
                    g1_c = graph.create_graph_test(n_side)
                    g2_c = graph.create_graph_test(n_side)

                    eulersplit.split_test(g_c_copy, g1_c, g2_c)

                    # Check that the input graph is now empty
                    self.assertEqual(graph.get_max_degree_test(g_c_copy), 0)
                    for node in xrange(2 * n_side):
                        self.assertEqual(graph.get_degree_test(g_c_copy, node), 0)

                    # Check that graphs have the correct max_degree
                    self.assertEqual(graph.get_max_degree_test(g1_c), graph.get_max_degree_test(g_c) / 2)
                    self.assertEqual(graph.get_max_degree_test(g2_c), graph.get_max_degree_test(g_c) / 2)

                    # Check that the combination of the two graphs equals the original graph
                    graph.add_graph_test(g1_c, g2_c)
                    self.assertTrue(graph.are_equal_test(g_c, g1_c))
                   
                    graph.destroy_graph_test(g_c)
                    graph.destroy_graph_test(g_c_copy)
                    graph.destroy_graph_test(g1_c)
                    graph.destroy_graph_test(g2_c)

        pass

if __name__ == "__main__":
    unittest.main()
