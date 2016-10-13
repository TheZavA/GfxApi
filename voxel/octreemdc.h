#ifndef		HAS_OCTREE_H_BEEN_INCLUDED1
#define		HAS_OCTREE_H_BEEN_INCLUDED1
//* This is a 3D Manifold Dual Contouring implementation
//* It's a big mess with debug and testing code everywhere, along with comments that don't match.
//* But it's the only published implementation out there at this time, so it's better than nothing.
//* As of now, everything is working. Full vertex clustering with and without manifold criterion!
//*
//* Current issues:
//* - NONE! :D
//*
//* TODO is cleanup and comment
//*/
//
#include <stdint.h>
#include <boost/make_shared.hpp>
#include "mgl/MathGeoLib.h"
#include "qef.h"
#include "utilities.h"

enum NodeType : uint8_t
{
   None,
   Internal,
   Leaf,
   Collapsed
};

struct VertexPositionNormal
{
   float3 position;
   float3 normal;
};

class Vertex
{
public:
   boost::shared_ptr<Vertex> m_pParent;
   int index;
   bool collapsible;
   QefSolver qef;
   float3 normal;
   float3 position;
   int surface_index;
   //Vector3 Position{ get{ if( qef != null ) return qef.Solve( 1e-6f, 4, 1e-6f ); return Vector3.Zero; } }
   float error;
   int euler;
   int* eis;
   int in_cell;
   bool face_prop2;

   Vertex()
   {
      m_pParent = nullptr;
      index = -1;
      collapsible = true;
      normal = float3::zero;
      surface_index = -1;
      eis = nullptr;
   }
};



class OctreeNodeMdc
{
public:
   int index;
   float3 position;
   int size;
   boost::shared_ptr< OctreeNodeMdc > children[8];
   NodeType type;
   std::vector< boost::shared_ptr< Vertex > > m_vertices;
   //Vertex** vertices;
   uint8_t corners;
   int child_index;

   static bool EnforceManifold;

   float sphereFunction( float x, float y, float z )
   {
      return ( x*x + y*y + z*z ) - 110.1f;
   }

   OctreeNodeMdc()
   {
      corners = 0;
      position = float3::zero;
      size = 0;
   }

   OctreeNodeMdc( float3 inPosition, int inSize, NodeType inType )
      : position( inPosition )
      , index( 0 )
      , size( inSize )
      , type( inType )
      , corners( 0 )
   {
   }

   void GenerateVertexBuffer( std::vector<VertexPositionNormal>& vertices )
   {
      if( type != NodeType::Leaf )
      {
         for( int i = 0; i < 8; i++ )
         {
            if( children[i] != nullptr )
               children[i]->GenerateVertexBuffer( vertices );
         }
      }

      if( /*vertices == null ||*/ m_vertices.size() == 0 )
         return;

      for( int i = 0; i < m_vertices.size(); i++ )
      {
         if( m_vertices[i] == nullptr )
            continue;
         m_vertices[i]->index = vertices.size();
         //            Vector3 nc = this.vertices[i].normal * 0.5f + Vector3.One * 0.5f;
         //            nc.Normalize();
         //            Color c = new Color( nc );
         Vec3 outPos;
         m_vertices[i]->qef.solve( outPos, 1e-6f, 4, 1e-6f );
         VertexPositionNormal vertex;
         vertex.position.x = outPos.x;
         vertex.position.y = outPos.y;
         vertex.position.z = outPos.z;
         vertex.normal.x = m_vertices[i]->normal.x;
         vertex.normal.y = m_vertices[i]->normal.y;
         vertex.normal.z = m_vertices[i]->normal.z;

         vertices.push_back( vertex );

      }

   }

   void ConstructBase( int size, float error, const std::vector<Vertex>& vertices )
   {
      index = 0;
      position = float3::zero;
      this->size = size;
      type = NodeType::Internal;
      child_index = 0;
      int n_index = 1;
      ConstructNodes( n_index, 1 );
   }

   bool ConstructNodes( int& n_index, int threaded = 0 )
   {
      if( size == 1 )
      {
         return ConstructLeaf( n_index );
      }

      type = NodeType::Internal;
      int child_size = size / 2;
      bool has_children = false;

      for( int i = 0; i < 8; i++ )
      {
         index = n_index++;
         float3 child_pos = float3( Utilities::TCornerDeltas[i][0],
                                    Utilities::TCornerDeltas[i][1],
                                    Utilities::TCornerDeltas[i][2] );

         children[i] = boost::make_shared<OctreeNodeMdc>( position + child_pos * ( float ) child_size,
                                                          child_size,
                                                          NodeType::Internal );
         children[i]->child_index = i;

         int index = i;
         if( children[i]->ConstructNodes( n_index, 0 ) )
            has_children = true;
         else
         {
            children[i].reset();
         }

      }

      return has_children;
   }

   bool ConstructLeaf( int& index )
   {
      if( size != 1 )
         return false;

      this->index = index++;
      type = NodeType::Leaf;
      int corners1 = 0;
      float samples[8];
      for( int i = 0; i < 8; i++ )
      {
         float3 pos;
         pos.x = position.x + Utilities::TCornerDeltas[i][0];
         pos.y = position.y + Utilities::TCornerDeltas[i][1];
         pos.z = position.z + Utilities::TCornerDeltas[i][2];
         if( ( samples[i] = sphereFunction( pos.x, pos.y, pos.z ) ) < 0 )
            corners1 |= 1 << i;
      }
      this->corners = ( uint8_t ) corners1;

      if( corners1 == 0 || corners1 == 255 )
         return false;

      int8_t v_edges[5][13];

      memset( v_edges, -1, 5 * 13 );

      m_vertices.reserve( Utilities::TransformedVerticesNumberTable[corners1] );

      int v_index = 0;
      int e_index = 0;

      for( int e = 0; e < 16; e++ )
      {
         int code = Utilities::TransformedEdgesTable[corners1][e];
         if( code == -2 )
         {
            v_index++;
            break;
         }
         if( code == -1 )
         {
            v_index++;
            e_index = 0;
            continue;
         }

         v_edges[v_index][e_index++] = code;
      }

      for( int i = 0; i < v_index; i++ )
      {
         int k = 0;
         auto pVertex = boost::make_shared<Vertex>();

         float3 normal = float3::zero;
         int ei[12];
         while( v_edges[i][k] != -1 )
         {
            ei[v_edges[i][k]] = 1;
            float3 a = position + float3( Utilities::TCornerDeltas[Utilities::TEdgePairs[v_edges[i][k]][0]][0],
                                          Utilities::TCornerDeltas[Utilities::TEdgePairs[v_edges[i][k]][0]][1],
                                          Utilities::TCornerDeltas[Utilities::TEdgePairs[v_edges[i][k]][0]][2] ) * size;

            float3 b = position + float3( Utilities::TCornerDeltas[Utilities::TEdgePairs[v_edges[i][k]][1]][0],
                                          Utilities::TCornerDeltas[Utilities::TEdgePairs[v_edges[i][k]][1]][1],
                                          Utilities::TCornerDeltas[Utilities::TEdgePairs[v_edges[i][k]][1]][2] ) * size;

            // right, for now just take the avg
            float3 intersection = ( a + b ) / 2;
            float H = 0.001;
            float3 xyz1 = intersection + ( float3 ) ( H, 0.f, 0.f );
            float3 xyz1_1 = intersection - ( float3 ) ( H, 0.f, 0.f );

            float3 xyz2 = intersection + ( float3 ) ( 0, H, 0.f );
            float3 xyz2_1 = intersection - ( float3 ) ( 0, H, 0.f );

            float3 xyz3 = intersection + ( float3 ) ( 0, 0, H );
            float3 xyz3_1 = intersection - ( float3 ) ( 0, 0, H );

            const float dx = sphereFunction( xyz1.x, xyz1.y, xyz1.z ) - sphereFunction( xyz1_1.x, xyz1_1.y, xyz1_1.z );
            const float dy = sphereFunction( xyz2.x, xyz2.y, xyz2.z ) - sphereFunction( xyz2_1.x, xyz2_1.y, xyz2_1.z );
            const float dz = sphereFunction( xyz3.x, xyz3.y, xyz3.z ) - sphereFunction( xyz3_1.x, xyz3_1.y, xyz3_1.z );

            float3 n( dx, dy, dz );

            //float3 n = a.Normalized();
            normal += n;
            pVertex->qef.add( intersection.x, intersection.y, intersection.z, n.x, n.y, n.z );

            //            Vector3 intersection = Sampler.GetIntersection( a, b, samples[Utilities.TEdgePairs[v_edges[i][k], 0]], samples[Utilities.TEdgePairs[v_edges[i][k], 1]] );
            //            Vector3 n = Sampler.GetNormal( intersection );
            //            normal += n;
            //            this.vertices[i].qef.Add( intersection, n );
            k++;
         }

         normal /= ( float ) k;
         normal.Normalize();
         pVertex->index = 0;
         pVertex->m_pParent = nullptr;
         pVertex->collapsible = true;
         pVertex->normal = normal;
         pVertex->euler = 1;
         pVertex->eis = ei;
         pVertex->in_cell = child_index;
         pVertex->face_prop2 = true;
         m_vertices.push_back( pVertex );

      }

      return true;
   }

   void ProcessCell( std::vector<int>& indexes, std::vector<int>& tri_count, float threshold )
   {
      if( type == NodeType::Internal )
      {
         for( int i = 0; i < 8; i++ )
         {
            if( children[i] != nullptr )
               children[i]->ProcessCell( indexes, tri_count, threshold );
         }

         for( int i = 0; i < 12; i++ )
         {
            boost::shared_ptr< OctreeNodeMdc > face_nodes[2];

            int c1 = Utilities::TEdgePairs[i][0];
            int c2 = Utilities::TEdgePairs[i][1];

            face_nodes[0] = children[c1];
            face_nodes[1] = children[c2];

            ProcessFace( face_nodes, Utilities::TEdgePairs[i][2], indexes, tri_count, threshold );
         }

         for( int i = 0; i < 6; i++ )
         {
            boost::shared_ptr< OctreeNodeMdc > edge_nodes[4] =
            {
               children[Utilities::TCellProcEdgeMask[i][0]],
               children[Utilities::TCellProcEdgeMask[i][1]],
               children[Utilities::TCellProcEdgeMask[i][2]],
               children[Utilities::TCellProcEdgeMask[i][3]]
            };

            ProcessEdge( edge_nodes, Utilities::TCellProcEdgeMask[i][4], indexes, tri_count, threshold );
         }
      }
   }

   void ProcessFace( boost::shared_ptr< OctreeNodeMdc > nodes[2], int direction, std::vector<int>& indexes, std::vector<int>& tri_count, float threshold )
   {
      if( nodes[0] == nullptr || nodes[1] == nullptr )
         return;

      if( nodes[0]->type != NodeType::Leaf || nodes[1]->type != NodeType::Leaf )
      {
         for( int i = 0; i < 4; i++ )
         {
            boost::shared_ptr< OctreeNodeMdc > face_nodes[2];

            for( int j = 0; j < 2; j++ )
            {
               if( nodes[j]->type == NodeType::Leaf )
                  face_nodes[j] = nodes[j];
               else
                  face_nodes[j] = nodes[j]->children[Utilities::TFaceProcFaceMask[direction][i][j]];
            }

            ProcessFace( face_nodes, Utilities::TFaceProcFaceMask[direction][i][2], indexes, tri_count, threshold );
         }

         int orders[2][4] =
         {
            { 0, 0, 1, 1 },
            { 0, 1, 0, 1 },
         };

         for( int i = 0; i < 4; i++ )
         {
            boost::shared_ptr< OctreeNodeMdc > edge_nodes[4];

            for( int j = 0; j < 4; j++ )
            {
               if( nodes[orders[Utilities::TFaceProcEdgeMask[direction][i][0]][j]]->type == NodeType::Leaf )
                  edge_nodes[j] = nodes[orders[Utilities::TFaceProcEdgeMask[direction][i][0]][j]];
               else
                  edge_nodes[j] = nodes[orders[Utilities::TFaceProcEdgeMask[direction][i][0]][j]]->children[Utilities::TFaceProcEdgeMask[direction][i][1 + j]];
            }
            ProcessEdge( edge_nodes, Utilities::TFaceProcEdgeMask[direction][i][5], indexes, tri_count, threshold );
         }
      }
   }

   void ProcessEdge( boost::shared_ptr< OctreeNodeMdc > nodes[4], int direction, std::vector<int>& indexes, std::vector<int>& tri_count, float threshold )
   {
      if( nodes[0] == nullptr || nodes[1] == nullptr || nodes[2] == nullptr || nodes[3] == nullptr )
         return;

      if( nodes[0]->type == NodeType::Leaf && nodes[1]->type == NodeType::Leaf && nodes[2]->type == NodeType::Leaf && nodes[3]->type == NodeType::Leaf )
      {
         ProcessIndexes( nodes, direction, indexes, tri_count, threshold );
      }
      else
      {
         for( int i = 0; i < 2; i++ )
         {
            boost::shared_ptr< OctreeNodeMdc > edge_nodes[4];

            for( int j = 0; j < 4; j++ )
            {
               if( nodes[j]->type == NodeType::Leaf )
                  edge_nodes[j] = nodes[j];
               else
                  edge_nodes[j] = nodes[j]->children[Utilities::TEdgeProcEdgeMask[direction][i][j]];
            }

            ProcessEdge( edge_nodes, Utilities::TEdgeProcEdgeMask[direction][i][4], indexes, tri_count, threshold );
         }
      }
   }

   void ProcessIndexes( boost::shared_ptr< OctreeNodeMdc > nodes[4], int direction, std::vector<int>& indexes, std::vector<int>& tri_count, float threshold )
   {
      int min_size = 10000000;
      int min_index = 0;
      int indices[4] = { -1, -1, -1, -1 };
      bool flip = false;
      bool sign_changed = false;
      int v_count = 0;

      for( int i = 0; i < 4; i++ )
      {
         int edge = Utilities::TProcessEdgeMask[direction][i];
         int c1 = Utilities::TEdgePairs[edge][0];
         int c2 = Utilities::TEdgePairs[edge][1];

         int m1 = ( nodes[i]->corners >> c1 ) & 1;
         int m2 = ( nodes[i]->corners >> c2 ) & 1;

         if( nodes[i]->size < min_size )
         {
            min_size = nodes[i]->size;
            min_index = i;
            flip = m1 == 1;
            sign_changed = ( ( m1 == 0 && m2 != 0 ) || ( m1 != 0 && m2 == 0 ) );
         }

         //find the vertex index
         int index = 0;
         bool skip = false;

         for( int k = 0; k < 16; k++ )
         {
            int e = Utilities::TransformedEdgesTable[nodes[i]->corners][k];
            if( e == -1 )
            {
               index++;
               continue;
            }
            if( e == -2 )
            {
               skip = true;
               break;
            }
            if( e == edge )
               break;
         }

         if( skip )
            continue;

         v_count++;
         // not sure i ported this correctly
         if( index >= nodes[i]->m_vertices.size() )
            return;
         boost::shared_ptr<Vertex> v = nodes[i]->m_vertices[index];
         boost::shared_ptr<Vertex> highest = v;
         while( highest->m_pParent != nullptr )
         {
            if( highest->m_pParent->collapsible
                || ( highest->m_pParent->error <= threshold
                     && ( !true || ( highest->m_pParent->euler == 1 && highest->m_pParent->face_prop2 ) ) ) )
            {
               highest = highest->m_pParent;
               v = highest;
            }
            else
               highest = highest->m_pParent;
         }

         indices[i] = v->index;

      }

      /*
      * Next generate the triangles.
      * Because we're generating from the finest levels that were collapsed, many triangles will collapse to edges or vertices.
      * That's why we check if the indices are different and discard the triangle, as mentioned in the paper.
      */
      if( sign_changed )
      {
         int count = 0;
         if( !flip )
         {
            if( indices[0] != -1 &&
                indices[1] != -1 &&
                indices[2] != -1 &&
                indices[0] != indices[1] &&
                indices[1] != indices[3] )
            {
               indexes.push_back( indices[0] );
               indexes.push_back( indices[1] );
               indexes.push_back( indices[3] );
               count++;
            }

            if( indices[0] != -1 &&
                indices[2] != -1 &&
                indices[3] != -1 &&
                indices[0] != indices[2] &&
                indices[2] != indices[3] )
            {
               indexes.push_back( indices[0] );
               indexes.push_back( indices[3] );
               indexes.push_back( indices[2] );
               count++;
            }
         }
         else
         {
            if( indices[0] != -1 &&
                indices[3] != -1 &&
                indices[1] != -1 &&
                indices[0] != indices[1] &&
                indices[1] != indices[3] )
            {
               indexes.push_back( indices[0] );
               indexes.push_back( indices[3] );
               indexes.push_back( indices[1] );
               count++;
            }

            if( indices[0] != -1 &&
                indices[2] != -1 &&
                indices[3] != -1 &&
                indices[0] != indices[2] &&
                indices[2] != indices[3] )
            {
               indexes.push_back( indices[0] );
               indexes.push_back( indices[2] );
               indexes.push_back( indices[3] );
               count++;
            }
         }

         if( count > 0 )
            tri_count.push_back( count );
      }
   }

   void ClusterCellBase( float error )
   {
      if( type != NodeType::Internal )
         return;

      for( int i = 0; i < 8; i++ )
      {
         if( children[i] == nullptr )
            continue;

         children[i]->ClusterCell( error );
      }
   }

   /*
   * Cell stage
   */
   void ClusterCell( float error )
   {
      if( type != NodeType::Internal )
         return;

      // First cluster all the children nodes
      int signs[8] = { -1, -1, -1, -1, -1, -1, -1, -1 };
      int mid_sign = -1;

      bool is_collapsible = true;
      for( int i = 0; i < 8; i++ )
      {
         if( children[i] == nullptr )
            continue;

         children[i]->ClusterCell( error );
         if( children[i]->type == NodeType::Internal ) //Can't cluster if the child has children
            is_collapsible = false;
         else
         {
            mid_sign = ( children[i]->corners >> ( 7 - i ) ) & 1;
            signs[i] = ( children[i]->corners >> i ) & 1;
         }
      }

      corners = 0;
      for( int i = 0; i < 8; i++ )
      {
         if( signs[i] == -1 )
            corners |= ( uint8_t ) ( mid_sign << i );
         else
            corners |= ( uint8_t ) ( signs[i] << i );
      }

      int surface_index = 0;
      std::vector < boost::shared_ptr< Vertex > > collected_vertices;
      std::vector < boost::shared_ptr< Vertex > > new_vertices;

      /*
      * Find all the surfaces inside the children that cross the 6 Euclidean edges and the vertices that connect to them
      */
      for( int i = 0; i < 12; i++ )
      {
         boost::shared_ptr< OctreeNodeMdc> face_nodes[2];

         int c1 = Utilities::TEdgePairs[i][0];
         int c2 = Utilities::TEdgePairs[i][1];
         face_nodes[0] = children[c1];
         face_nodes[1] = children[c2];

         ClusterFace( face_nodes, Utilities::TEdgePairs[i][2], surface_index, collected_vertices );
      }

      for( int i = 0; i < 6; i++ )
      {
         boost::shared_ptr< OctreeNodeMdc> edge_nodes[4] =
         {
            children[Utilities::TCellProcEdgeMask[i][0]],
            children[Utilities::TCellProcEdgeMask[i][1]],
            children[Utilities::TCellProcEdgeMask[i][2]],
            children[Utilities::TCellProcEdgeMask[i][3]]
         };

         ClusterEdge( edge_nodes, Utilities::TCellProcEdgeMask[i][4], surface_index, collected_vertices );
      }

      int highest_index = surface_index;

      if( highest_index == -1 )
         highest_index = 0;

      /*
      * Gather the stray vertices
      */
      for( int i = 0; i < 8; i++ )
      {
         auto n = children[i];
         if( n == nullptr )
            continue;
         for( auto v : m_vertices )
         {
            if( v == nullptr )
               continue;
            if( v->surface_index == -1 )
            {
               v->surface_index = highest_index++;
               collected_vertices.push_back( v );
            }
         }
      }

      int clustered_count = 0;
      if( collected_vertices.size() > 0 )
      {
         for( int i = 0; i <= highest_index; i++ )
         {
            QefSolver qef;
            float3 normal = float3( 0, 0, 0 );
            int count = 0;
            int edges[12];
            int euler = 0;
            int e = 0;
            for( auto v : collected_vertices )
            {
               if( v->surface_index == i )
               {
                  /* Calculate ei(Sv) */
                  for( int k = 0; k < 3; k++ )
                  {
                     int edge = Utilities::TExternalEdges[v->in_cell][k];
                     edges[edge] += v->eis[edge];
                  }
                  /* Calculate e(Svk) */
                  for( int k = 0; k < 9; k++ )
                  {
                     int edge = Utilities::TInternalEdges[v->in_cell][k];
                     e += v->eis[edge];
                  }
                  euler += v->euler;
                  qef.add( v->qef.getData() );
                  normal += v->normal;
                  count++;
               }
            }

            /*
            * One vertex might have an error greater than the threshold, preventing simplification.
            * When it's just one, we can ignore the error and proceed.
            */

            if( count == 0 )
            {
               continue;
            }

            bool face_prop2 = true;
            for( int f = 0; f < 6 && face_prop2; f++ )
            {
               int intersections = 0;
               for( int ei = 0; ei < 4; ei++ )
               {
                  intersections += edges[Utilities::TFaces[f][ei]];
               }
               if( !( intersections == 0 || intersections == 2 ) )
                  face_prop2 = false;
            }

            boost::shared_ptr< Vertex > new_vertex = boost::make_shared< Vertex >();
            normal /= ( float ) count;
            normal.Normalize();
            new_vertex->normal = normal;
            new_vertex->qef.add( qef.getData() );
            new_vertex->eis = edges;
            new_vertex->euler = euler - e / 4;

            new_vertex->in_cell = child_index;
            new_vertex->face_prop2 = face_prop2;

            new_vertices.push_back( new_vertex );
            Vec3 outVec;

            qef.solve( outVec, 1e-6f, 4, 1e-6f );
            new_vertex->position.x = outVec.x;
            new_vertex->position.y = outVec.y;
            new_vertex->position.z = outVec.z;
            /*  new_vertex->position.y = outVec.y;
            new_vertex->position.z = outVec.z;*/
            float err = qef.getError();
            new_vertex->collapsible = err <= error;
            new_vertex->error = err;
            clustered_count++;

            for( auto v : collected_vertices )
            {
               if( v->surface_index == i )
               {
                  if( v->position.x != new_vertex->position.x ||
                      v->position.y != new_vertex->position.y ||
                      v->position.z != new_vertex->position.z )
                     v->m_pParent = new_vertex;
                  else
                     v->m_pParent = nullptr;
               }
            }
         }
      }
      else
      {
         return;
      }

      for( auto v2 : collected_vertices )
      {
         v2->surface_index = -1;
      }
      m_vertices = new_vertices;
   }

   void GatherVertices( boost::shared_ptr< OctreeNodeMdc> n, std::vector<boost::shared_ptr<Vertex>>& dest, int surface_index )
   {
      if( n == nullptr )
         return;
      if( n->size > 1 )
      {
         for( int i = 0; i < 8; i++ )
            GatherVertices( n->children[i], dest, surface_index );
      }
      else
      {
         for( auto v : n->m_vertices )
         {
            if( v->surface_index == -1 )
            {
               v->surface_index = surface_index++;
               dest.push_back( v );
            }
         }
      }
   }

   void ClusterFace( boost::shared_ptr< OctreeNodeMdc > nodes[2], int direction, int surface_index, std::vector < boost::shared_ptr< Vertex > >& collected_vertices )
   {
      if( nodes[0] == nullptr || nodes[1] == nullptr )
         return;

      if( nodes[0]->type != NodeType::Leaf || nodes[1]->type != NodeType::Leaf )
      {
         for( int i = 0; i < 4; i++ )
         {
            boost::shared_ptr< OctreeNodeMdc > face_nodes[2];

            for( int j = 0; j < 2; j++ )
            {
               if( nodes[j] == nullptr )
                  continue;
               if( nodes[j]->type != NodeType::Internal )
                  face_nodes[j] = nodes[j];
               else
                  face_nodes[j] = nodes[j]->children[Utilities::TFaceProcFaceMask[direction][i][j]];
            }
            ClusterFace( face_nodes, Utilities::TFaceProcFaceMask[direction][i][2], surface_index, collected_vertices );
         }
      }

      int orders[2][4] =
      {
         { 0, 0, 1, 1 },
         { 0, 1, 0, 1 },
      };

      for( int i = 0; i < 4; i++ )
      {
         boost::shared_ptr< OctreeNodeMdc > edge_nodes[4];
         for( int j = 0; j < 4; j++ )
         {
            if( nodes[orders[Utilities::TFaceProcEdgeMask[direction][i][0]][j]] == nullptr )
               continue;
            if( nodes[orders[Utilities::TFaceProcEdgeMask[direction][i][0]][j]]->type != NodeType::Internal )
               edge_nodes[j] = nodes[orders[Utilities::TFaceProcEdgeMask[direction][i][0]][j]];
            else
               edge_nodes[j] = nodes[orders[Utilities::TFaceProcEdgeMask[direction][i][0]][j]]->children[Utilities::TFaceProcEdgeMask[direction][i][1 + j]];
         }
         ClusterEdge( edge_nodes, Utilities::TFaceProcEdgeMask[direction][i][5], surface_index, collected_vertices );
      }
   }

   void ClusterEdge( boost::shared_ptr< OctreeNodeMdc > nodes[4], int direction, int surface_index, std::vector < boost::shared_ptr< Vertex > >& collected_vertices )
   {
      if( ( nodes[0] == nullptr ||
            nodes[0]->type != NodeType::Internal ) &&
            ( nodes[1] == nullptr ||
              nodes[1]->type != NodeType::Internal ) &&
              ( nodes[2] == nullptr ||
                nodes[2]->type != NodeType::Internal ) &&
                ( nodes[3] == nullptr ||
                  nodes[3]->type != NodeType::Internal ) )
      {
         ClusterIndexes( nodes, direction, surface_index, collected_vertices );
      }
      else
      {
         for( int i = 0; i < 2; i++ )
         {
            boost::shared_ptr< OctreeNodeMdc > edge_nodes[4];

            for( int j = 0; j < 4; j++ )
            {
               if( nodes[j] == nullptr )
                  continue;
               if( nodes[j]->type == NodeType::Leaf )
                  edge_nodes[j] = nodes[j];
               else
                  edge_nodes[j] = nodes[j]->children[Utilities::TEdgeProcEdgeMask[direction][i][j]];
            }
            ClusterEdge( edge_nodes, Utilities::TEdgeProcEdgeMask[direction][i][4], surface_index, collected_vertices );
         }
      }
   }

   void ClusterIndexes( boost::shared_ptr< OctreeNodeMdc > nodes[4], int direction, int max_surface_index, std::vector < boost::shared_ptr< Vertex > >& collected_vertices )
   {
      if( nodes[0] == nullptr &&
          nodes[1] == nullptr &&
          nodes[2] == nullptr &&
          nodes[3] == nullptr )
         return;

      boost::shared_ptr< Vertex > vertices[4];
      int v_count = 0;
      int node_count = 0;

      for( int i = 0; i < 4; i++ )
      {
         if( nodes[i] == nullptr )
            continue;
         node_count++;

         int edge = Utilities::TProcessEdgeMask[direction][i];
         int c1 = Utilities::TEdgePairs[edge][0];
         int c2 = Utilities::TEdgePairs[edge][1];

         int m1 = ( nodes[i]->corners >> c1 ) & 1;
         int m2 = ( nodes[i]->corners >> c2 ) & 1;

         //find the vertex index
         int index = 0;
         bool skip = false;
         for( int k = 0; k < 16; k++ )
         {
            int e = Utilities::TransformedEdgesTable[nodes[i]->corners][k];
            if( e == -1 )
            {
               index++;
               continue;
            }
            if( e == -2 )
            {
               if( !( ( m1 == 0 && m2 != 0 ) || ( m1 != 0 && m2 == 0 ) ) )
                  skip = true;
               break;
            }
            if( e == edge )
               break;
         }

         if( !skip && index < nodes[i]->m_vertices.size() )
         {
            vertices[i] = nodes[i]->m_vertices[index];
            while( vertices[i]->m_pParent != nullptr )
               vertices[i] = vertices[i]->m_pParent;
            v_count++;
         }
      }

      if( v_count == 0 )
         return;

      int surface_index = -1;

      for( int i = 0; i < 4; i++ )
      {

         if( vertices[i] == nullptr )
            continue;
         if( vertices[i]->surface_index != -1 )
         {
            if( surface_index != -1 && surface_index != vertices[i]->surface_index )
            {
               AssignSurface( collected_vertices, vertices[i]->surface_index, surface_index );
            }
            else if( surface_index == -1 )
               surface_index = vertices[i]->surface_index;
         }
      }

      if( surface_index == -1 )
         surface_index = max_surface_index++;

      for( int i = 0; i < 4; i++ )
      {

         if( vertices[i] == nullptr )
            continue;

         if( vertices[i]->surface_index == -1 )
         {
            collected_vertices.push_back( vertices[i] );
         }
         vertices[i]->surface_index = surface_index;
      }
   }

   void AssignSurface( std::vector < boost::shared_ptr< Vertex > >& vertices, int from, int to )
   {
      for( auto v : vertices )
      {
         if( v != nullptr && v->surface_index == from )
            v->surface_index = to;
      }
   }
};
#endif