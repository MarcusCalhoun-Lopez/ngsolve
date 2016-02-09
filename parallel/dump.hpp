#ifndef FILE_NGS_DUMP
#define FILE_NGS_DUMP

/*********************************************************************/
/* File:   dump.hpp                                                  */
/* Author: Lukas Kogler                                              */
/* Date:   Sep. 2014                                                 */
/*********************************************************************/

namespace ngstd
{

  template <NODE_TYPE NT>
  class key_trait { };

  template <>
  class key_trait<NT_VERTEX> 
  {
  public:
    typedef int TKEY;
  };

  template <>
  class key_trait<NT_EDGE> 
  { 
  public:
    typedef INT<2> TKEY;
  };

  template <>
  class key_trait<NT_FACE> 
  { 
  public:
    typedef INT<3> TKEY;
  };

  template <>
  class key_trait<NT_CELL> 
  { 
  public:
    typedef INT<4> TKEY;
  };


  template <int N>
  bool operator < (INT<N> a, INT<N> b)
  {
    for (int i = 0; i < N; i++)
      {
	if (a[i] < b[i]) return true;
	if (a[i] > b[i]) return false;
      }
    return false;
  }

  /*
    bool operator < (INT<2> & nodea, INT<2> & nodeb)
    {
    if(nodea[0] < nodeb[0])
    return true;
    else if(nodea[0] == nodeb[0] && nodea[1] < nodeb[1])
    return true;
    return false;
    }

    bool operator < (INT<3> & nodea, INT<3> & nodeb)
    {
    if(nodea[0] < nodeb[0])
    return true;
    else if(nodea[0] == nodeb[0])
    {
    if(nodea[1] < nodeb[1])
    return true;
    else if(nodea[1] == nodeb[1])
    if(nodea[2] < nodeb[2])
    return true;
    }
    return false;
    }

    bool operator < (INT<4> & nodea, INT<4> & nodeb)
    {
    if(nodea[0] < nodeb[0])
    return true;
    else if(nodea[0] == nodeb[0])
    {
    if(nodea[1] < nodeb[1])
    return true;
    else if(nodea[1] == nodeb[1])
    {
    if(nodea[2] < nodeb[2])
    return true;
    else if(nodea[2] == nodeb[2])
    {
    if(nodea[3] < nodeb[3]) 
    return true;
    }
    }
    }
    return false;
    }
  */



  /*
    template <NODE_TYPE NT>
    auto GetGlobalNodeId (const MeshAccess & ma, int nr) -> typename key_trait<NT>::TKEY { ; }
  */

  template <NODE_TYPE NT>
  inline auto GetGlobalNodeId (const MeshAccess & ma, int nr) -> typename key_trait<NT>::TKEY
  { 
    cout << "called base GetGlobalNodeId!!" << endl;
    return 1;
  }

  template <>
  inline auto GetGlobalNodeId<NT_VERTEX> (const MeshAccess & ma, int nr) 
    -> typename key_trait<NT_VERTEX>::TKEY 
  { 
    return ma.GetGlobalNodeNum (Node(NT_VERTEX, nr));
  }

  template <>
  inline auto GetGlobalNodeId<NT_EDGE> (const MeshAccess & ma, int nr) -> typename key_trait<NT_EDGE>::TKEY 
  {
    int pi1,pi2;
    ma.GetEdgePNums (nr, pi1, pi2);
    return INT<2> (ma.GetGlobalNodeNum (Node(NT_VERTEX, pi1)),
		   ma.GetGlobalNodeNum (Node(NT_VERTEX, pi2)));
  }

  template <>
  inline auto GetGlobalNodeId<NT_FACE> (const MeshAccess & ma, int nr) -> typename key_trait<NT_FACE>::TKEY 
  {
    Array<int> edges (3);
    Array<int> verts;
    ma.GetFaceEdges(nr, edges);
    for(int k=0;k<3;k++)
      {
	int p1, p2;
	ma.GetEdgePNums(edges[k], p1, p2);
	if(verts.Contains(p1)==0)
	  verts.Append(p1);
	if(verts.Contains(p2)==0)
	  verts.Append(p2);
      }
    QuickSort(verts);
    return INT<3> (verts[0], verts[1], verts[2]);
  }

  template <>
  inline auto GetGlobalNodeId<NT_CELL> (const MeshAccess & ma, int nr) -> typename key_trait<NT_CELL>::TKEY 
  {
    Array<int> faces(4);
    Array<int> verts;
    ma.GetElFacets(nr, faces);

    for(int k=0;k<4;k++)
      {
	Array<int> edges(3);
	ma.GetFaceEdges(faces[k], edges);
	//cout << "edges: " << edges << endl;
	for(int j=0;j<3;j++)
	  {
	    int p1, p2;
	    ma.GetEdgePNums(edges[j], p1, p2);
	    if(verts.Contains(p1)==0)
	      verts.Append(p1);
	    if(verts.Contains(p2)==0)
	      verts.Append(p2);
	  }
      }
    QuickSort(verts);
    return INT<4> (verts[0], verts[1], verts[2], verts[3]);
  }

  /*
  template <NODE_TYPE NT>
  void SetMPIType(MPI_Datatype * type)
  {
    int sz = 1;
    switch (NT)
      {
      case NT_VERTEX:
	sz = 1;
	break;
      case NT_EDGE:
	sz = 2;
	break;
      case NT_FACE:
	sz = 3;
	break;
      case NT_CELL:
	sz = 4;
	break;
      }
    MPI_Type_contiguous(sz, MPI_INT, type);
    MPI_Type_commit(type);
  }
  */

  /*
  template <typename DT> struct MPIT {};
  template<> struct MPIT<double> {static MPI_Datatype mpi_type;};
  MPI_Datatype MPIT<double> :: mpi_type = MPI_DOUBLE;
  template <> struct MPIT<int> {static MPI_Datatype mpi_type;};
  MPI_Datatype MPIT<int> :: mpi_type = MPI_INT;
  template <> struct MPIT<unsigned char> {static MPI_Datatype mpi_type;};
  MPI_Datatype MPIT<unsigned char> :: mpi_type = MPI_BYTE;
  template <> struct MPIT<INT<2,unsigned char>> { static MPI_Datatype mpi_type;};
  MPI_Datatype MPIT<INT<2,unsigned char>> :: mpi_type;
  template <> struct MPIT<INT<3,unsigned char>> { static MPI_Datatype mpi_type;};
  MPI_Datatype MPIT<INT<3,unsigned char>> :: mpi_type;

  class class_init_mpi_types
  {
  public:
    class_init_mpi_types()
    {
      MPI_Type_contiguous ( 2, MPI_BYTE, &MPIT<INT<2,unsigned char>>::mpi_type);
      MPI_Type_commit ( &MPIT<INT<2,unsigned char>>::mpi_type );

      MPI_Type_contiguous ( 3, MPI_BYTE, &MPIT<INT<3,unsigned char>>::mpi_type);
      MPI_Type_commit ( &MPIT<INT<3,unsigned char>>::mpi_type );

    }
  };
  static class_init_mpi_types init_mpi_types;
  */

  //provides the place in merge-tree
  inline void find_SRRMS (int rank, int np, int* p1, int* p2, int* p3, bool ignore_in, bool ignore_out);
  inline void find_ROMS (int rank, int np, int* p1, int* p2); 


  template<typename DT, NODE_TYPE NT>
  void packaged_buffered_send(int rank, int np, DT* a, typename key_trait<NT>::TKEY* b, int n, int pkg_size, int p,
			      Array<MPI_Request> & requests)
  {
    // MPI_Datatype mpi_type_array = MPIT<DT>::mpi_type;
    MPI_Datatype mpi_type_array = MyGetMPIType<DT>();

    //get type for keys
    // MPI_Datatype mpi_type_key;
    // SetMPIType<NT>(&mpi_type_key);
    typedef typename key_trait<NT>::TKEY tkey;
    MPI_Datatype mpi_type_key = MyGetMPIType<tkey>();    

    bool has_extra = n%pkg_size;
    int n_packages = n/pkg_size + (has_extra ? 1 : 0);

    //send size
    MPI_Send ( &n, 1, MPI_INT, p, 700001, MPI_COMM_WORLD);
    
    for(int k=0;k<n_packages - has_extra?1:0;k++)
      {
	// MPI_Send ( a+k*pkg_size, pkg_size, mpi_type_array, p, 700001, MPI_COMM_WORLD); 
	// MPI_Send ( b+k*pkg_size, pkg_size, mpi_type_key,   p, 700001, MPI_COMM_WORLD);
	MPI_Request requ;
	MPI_Isend ( a+k*pkg_size, pkg_size, mpi_type_array, p, 700001, MPI_COMM_WORLD, &requ); 
	requests += requ;
	MPI_Isend ( b+k*pkg_size, pkg_size, mpi_type_key,   p, 700001, MPI_COMM_WORLD, &requ); 
	requests += requ;
      }
    //copy last part into new memory so full package can be sent - for simplicity!!
    if(has_extra)
      {
	DT *a_ext = (DT*) malloc(pkg_size * sizeof(DT));                   // I know, it is leaking ...
	tkey    *b_ext = (tkey*)    malloc(pkg_size * sizeof(tkey));
	for(int k=0;k<n%pkg_size;k++)
	  { 
	    //a and b already point to last part
	    a_ext[n%pkg_size-k-1] = a[n-1-k];
	    b_ext[n%pkg_size-k-1] = b[n-1-k];
	  }
	// MPI_Send ( a_ext, pkg_size, mpi_type_array, p, 700001, MPI_COMM_WORLD); 
	// MPI_Send ( b_ext, pkg_size, mpi_type_key,   p, 700001, MPI_COMM_WORLD); 

	MPI_Request requ;
	MPI_Isend ( a_ext, pkg_size, mpi_type_array, p, 700001, MPI_COMM_WORLD, &requ); 
	requests += requ;
	MPI_Isend ( b_ext, pkg_size, mpi_type_key,   p, 700001, MPI_COMM_WORLD, &requ); 
	requests += requ;
      }
  }

  template<typename DT, NODE_TYPE NT>
  void merge_own_in_out (int rank, int size, int pkg_size, DT* array, typename key_trait<NT>::TKEY *array_dnrs, int n, int p_in, int p_out)
  {
    typedef typename key_trait<NT>::TKEY TKEY;

    int base_array_size = n;

    //in-buffer
    int n_in = 0;
    MPI_Recv( &n_in, 1, MPI_INT, p_in, 700001, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

    int in_buf_size = 2 * pkg_size;  //do not change this for now!!

    Array<DT> in_buf(in_buf_size);
    Array<TKEY> in_dnrs(in_buf_size);

    int index_in = 0;
    int index_own = 0;

    //out-buffer 
    int n_out = base_array_size + n_in;
  
    MPI_Send(&n_out, 1, MPI_INT, p_out, 700001, MPI_COMM_WORLD);
    int out_buf_size = pkg_size;

    Array<DT> out_buf(out_buf_size);
    Array<TKEY> out_dnrs(out_buf_size);

    IntRange r1 (0, pkg_size);
    IntRange r2 (pkg_size, 2*pkg_size);

    int index_out = 0;
    bool has_extra = (n_out%pkg_size)?1:0;
  
    if(n_in)
      {
	//get 1st halve
	MyMPI_Recv (in_buf[r1], p_in, 700001, MPI_COMM_WORLD);
	MyMPI_Recv (in_dnrs[r1], p_in, 700001, MPI_COMM_WORLD);

	if(n_in>pkg_size)
	  {
	    //get 2nd halve
	    MyMPI_Recv (in_buf[r2], p_in, 700001, MPI_COMM_WORLD);
	    MyMPI_Recv (in_dnrs[r2], p_in, 700001, MPI_COMM_WORLD);
	  }
      }
  
    bool have1[2];
    have1[0] = true;
    have1[1] = true;
      
    int packages_sent = 0;
      

    int iib; //index in buf
    while (index_in<n_in && index_own<base_array_size)
      {
	iib = index_in%in_buf_size;
	if(iib == pkg_size && index_in+pkg_size<n_in && !have1[0]) //is at first of 2nd halve - replace first halve
	  {
	    MyMPI_Recv (in_buf[r1], p_in, 700001, MPI_COMM_WORLD);
	    MyMPI_Recv (in_dnrs[r1], p_in, 700001, MPI_COMM_WORLD);
	    have1[0] = true;
	  }
	if(iib == 0 && index_in!=0 && index_in+pkg_size<n_in && !have1[1] ) //is at last of 2nd halve - set to 0 and replace 2nd halve
	  {
	    MyMPI_Recv (in_buf[r2], p_in, 700001, MPI_COMM_WORLD);
	    MyMPI_Recv (in_dnrs[r2], p_in, 700001, MPI_COMM_WORLD);
	    have1[1] = true;
	  }
	if(in_dnrs[iib] < array_dnrs[index_own])
	  {
	    out_buf[index_out] = in_buf[iib];
	    out_dnrs[index_out++] = in_dnrs[iib];
	    index_in++;
	    if(index_in%in_buf_size == 0)
	      have1[1] = false;
	    else if(index_in%in_buf_size == pkg_size)
	      have1[0] = false;
	  }
	else
	  {
	    out_buf[index_out] = array[index_own];
	    out_dnrs[index_out++] = array_dnrs[index_own++];
	  }
	if(index_out == pkg_size)
	  {
	    index_out = 0;
	    MyMPI_Send (out_buf, p_out, 700001, MPI_COMM_WORLD);
	    MyMPI_Send (out_dnrs, p_out, 700001, MPI_COMM_WORLD);
	    packages_sent++;
	  }
      }
    while(index_in<n_in)
      {
	iib = index_in%in_buf_size;
	if(iib == pkg_size && index_in+pkg_size<n_in && !have1[0]) //is at first of 2nd halve - replace first halve
	  {
	    MyMPI_Recv (in_buf[r1], p_in, 700001, MPI_COMM_WORLD);
	    MyMPI_Recv (in_dnrs[r1], p_in, 700001, MPI_COMM_WORLD);
	    have1[0] = true;
	  }
	if(iib == 0 && index_in!= 0 && index_in+pkg_size<n_in && !have1[1]) //is at last of 2nd halve - set to 0 and replace 2nd halve
	  {
	    MyMPI_Recv (in_buf[r2], p_in, 700001, MPI_COMM_WORLD);
	    MyMPI_Recv (in_dnrs[r2], p_in, 700001, MPI_COMM_WORLD);
	    have1[1] = true;
	  }
	out_buf[index_out] = in_buf[iib];
	out_dnrs[index_out++] = in_dnrs[iib];
	index_in++;
	if(index_in%in_buf_size == 0)
	  have1[1] = false;
	else if(index_in%in_buf_size == pkg_size)
	  have1[0] = false;
	if(index_out == pkg_size)
	  {
	    index_out = 0;
	    MyMPI_Send (out_buf, p_out, 700001, MPI_COMM_WORLD);
	    MyMPI_Send (out_dnrs, p_out, 700001, MPI_COMM_WORLD);
	    packages_sent++;
	  }
      }
    while(index_own<base_array_size)
      {
	out_buf[index_out] = array[index_own];
	out_dnrs[index_out++] = array_dnrs[index_own++];
	if(index_out == pkg_size)
	  {
	    index_out = 0;
	    MyMPI_Send (out_buf, p_out, 700001, MPI_COMM_WORLD);
	    MyMPI_Send (out_dnrs, p_out, 700001, MPI_COMM_WORLD);
	    packages_sent++;
	  }
      }
    if(has_extra)
      {
	MyMPI_Send (out_buf, p_out, 700001, MPI_COMM_WORLD);
	MyMPI_Send (out_dnrs, p_out, 700001, MPI_COMM_WORLD);
	packages_sent++;
      }
  }

  template<typename DT, NODE_TYPE NT>
  void merge_in_in_out (int pkg_size, int rank, int np, int p1, int p2, int p_out)
  {
    // MPI_Datatype mpi_type_array = MPIT<DT>::mpi_type;
    MPI_Datatype mpi_type_array = MyGetMPIType<DT>();
    //get type for keys
    // MPI_Datatype mpi_type_key;
    // SetMPIType<NT>(&mpi_type_key);
    typedef typename key_trait<NT>::TKEY tkey;
    MPI_Datatype mpi_type_key = MyGetMPIType<tkey>();    

    int in_buf_size = pkg_size * 2;
    DT* a1 = (DT*) malloc (sizeof(DT) * in_buf_size);
    tkey* b1    = (tkey*)    malloc (sizeof(tkey)    * in_buf_size);
    DT* a2 = (DT*) malloc (sizeof(DT) * in_buf_size);
    tkey* b2    = (tkey*)    malloc (sizeof(tkey)    * in_buf_size);

    int out_buf_size = pkg_size;
    DT* a3 = (DT*) malloc (sizeof(DT) * out_buf_size);
    tkey*    b3 = (tkey*)    malloc (sizeof(tkey)    * out_buf_size);

    for(int k=0;k<in_buf_size;k++)
      {
	a1[k] = a2[k] = -1;
	b1[k] = b2[k] = -1;
      }
    for(int k=0;k<out_buf_size;k++)
      {
	a3[k] = -1;
	b3[k] = -1;
      }

    //Communicate sizes
    int n_in1, n_in2;
    int n_out;
    MPI_Recv( &n_in1, 1, MPI_INT, p1, 700001, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    MPI_Recv( &n_in2, 1, MPI_INT, p2, 700001, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
  
    n_out = n_in1 + n_in2;
    MPI_Send( &n_out, 1, MPI_INT, p_out, 700001, MPI_COMM_WORLD);
  

    //initial filling of buffer
    if(n_in1)
      {
	//1st halve, p1
	MPI_Recv(a1, pkg_size, mpi_type_array, p1, 700001, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
	MPI_Recv(b1, pkg_size, mpi_type_key, p1, 700001, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
	if(n_in1>pkg_size)
	  {
	    //2nd halve, p1
	    MPI_Recv(a1+pkg_size, pkg_size, mpi_type_array, p1, 700001, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
	    MPI_Recv(b1+pkg_size, pkg_size, mpi_type_key, p1, 700001, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
	  }
      }
    if(n_in2)
      {
	//1st halve, p2
	MPI_Recv(a2, pkg_size, mpi_type_array, p2, 700001, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
	MPI_Recv(b2, pkg_size, mpi_type_key, p2, 700001, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
	if(n_in2>pkg_size)
	  {
	    //2nd halve, p2
	    MPI_Recv(a2+pkg_size, pkg_size, mpi_type_array, p2, 700001, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
	    MPI_Recv(b2+pkg_size, pkg_size, mpi_type_key, p2, 700001, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
	  }
      }

  
    int got_from_p1 = 2;
    int got_from_p2 = 2;
    int packages_sent = 0;
    int index1, index2, i3;
    index1 = index2 = i3 = 0;

    bool have1[2];
    have1[0] = have1[1] = true;
    bool have2[2];
    have2[0] = have2[1] = true;
  

    while(index1<n_in1 && index2<n_in2)
      {
	int i1 = index1%in_buf_size;
	int i2 = index2%in_buf_size;
	if(i1==pkg_size && index1+pkg_size<n_in1 && !have1[0]) //replace 1st halve
	  {
	    //1st halve, p1
	    MPI_Recv(a1, pkg_size, mpi_type_array, p1, 700001, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
	    MPI_Recv(b1, pkg_size, mpi_type_key, p1, 700001, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
	    have1[0] = true;
	    got_from_p1++;
	  }
	else if (i1 == 0 && index1 !=0 && index1+pkg_size<n_in1 && !have1[1]) //replace 2nd halve
	  {
	    //2nd halve, p1
	    MPI_Recv(a1+pkg_size, pkg_size, mpi_type_array, p1, 700001, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
	    MPI_Recv(b1+pkg_size, pkg_size, mpi_type_key, p1, 700001, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
	    have1[1] = true;
	    got_from_p1++;
	  }
	else if(i2==pkg_size && index2+pkg_size<n_in2 && !have2[0]) //replace 1st halve
	  {
	    //1st halve, p2
	    MPI_Recv(a2, pkg_size, mpi_type_array, p2, 700001, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
	    MPI_Recv(b2, pkg_size, mpi_type_key, p2, 700001, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
	    have2[0] = true;
	    got_from_p2++;
	  }
	else if (i2 == 0 && index2 !=0 && index2+pkg_size<n_in2 && !have2[1]) //replace 2nd halve
	  {
	    have2[1] = true;
	    MPI_Recv(a2+pkg_size, pkg_size, mpi_type_array, p2, 700001, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
	    MPI_Recv(b2+pkg_size, pkg_size, mpi_type_key, p2, 700001, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
	    got_from_p2++;
	  }
      
	if(b1[i1]<b2[i2])
	  {
	    b3[i3] = b1[i1];
	    a3[i3++] = a1[i1];
	    index1++;
	    if(index1%in_buf_size == 0)
	      have1[1] = false;
	    else if(index1%in_buf_size == pkg_size)
	      have1[0] = false;
	  }
	else
	  {
	    b3[i3] = b2[i2];
	    a3[i3++] = a2[i2];
	    index2++;
	    if(index2%in_buf_size == 0)
	      have2[1] = false;
	    else if(index2%in_buf_size == pkg_size)
	      have2[0] = false;
	  }
      
	if(i3==pkg_size)
	  {
	    i3 = 0;
	    MPI_Send(a3, pkg_size, mpi_type_array, p_out, 700001, MPI_COMM_WORLD);
	    MPI_Send(b3, pkg_size, mpi_type_key,    p_out, 700001, MPI_COMM_WORLD);
	    packages_sent++;
	  }
      
      }
    while(index1<n_in1)
      { 
	int i1 = index1%in_buf_size;
	if(i1==pkg_size && index1+pkg_size<n_in1 && have1[0] == false) //replace 1st halve
	  {
	    //1st halve, p1
	    MPI_Recv(a1, pkg_size, mpi_type_array, p1, 700001, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
	    MPI_Recv(b1, pkg_size, mpi_type_key, p1, 700001, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
	    have1[0] = true;
	    got_from_p1++;
	  }
	else if (i1 == 0 && index1 !=0 && index1+pkg_size<n_in1 && have1[1] == false) //replace 2nd halve
	  {
	    //2nd halve, p1
	    MPI_Recv(a1+pkg_size, pkg_size, mpi_type_array, p1, 700001, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
	    MPI_Recv(b1+pkg_size, pkg_size, mpi_type_key, p1, 700001, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
	    have1[1] = true;
	    got_from_p1++;
	  }
	b3[i3] = b1[i1];
	a3[i3++] = a1[i1];
	index1++;
	if(index1%in_buf_size == 0)
	  have1[1] = false;
	else if(index1%in_buf_size == pkg_size)
	  have1[0] = false;
	if(i3==pkg_size)
	  {
	    i3 = 0;
	    MPI_Send(a3, pkg_size, mpi_type_array, p_out, 700001, MPI_COMM_WORLD);
	    MPI_Send(b3, pkg_size, mpi_type_key,    p_out, 700001, MPI_COMM_WORLD);
	    packages_sent++;
	  }
      }
    while(index2<n_in2)
      {
	int i2 = index2%in_buf_size;
	if(i2==pkg_size && index2+pkg_size<n_in2 && have2[0] == false) //replace 1st halve
	  {
	    //1st halve, p2
	    MPI_Recv(a2, pkg_size, mpi_type_array, p2, 700001, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
	    MPI_Recv(b2, pkg_size, mpi_type_key, p2, 700001, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
	    have2[0] = true;
	    got_from_p2++;
	  }
	else if (i2 == 0 && index2 !=0 && index2+pkg_size<n_in2 && have2[1] == false) //replace 2nd halve
	  {
	    MPI_Recv(a2+pkg_size, pkg_size, mpi_type_array, p2, 700001, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
	    MPI_Recv(b2+pkg_size, pkg_size, mpi_type_key, p2, 700001, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
	    have2[1] = true;
	    got_from_p2++;
	  }
	b3[i3] = b2[i2];
	a3[i3++] = a2[i2];
	index2++;
	if(index2%in_buf_size == 0)
	  have2[1] = false;
	else if(index2%in_buf_size == pkg_size)
	  have2[0] = false;
	if(i3==pkg_size)
	  {
	    i3 = 0;
	    MPI_Send(a3, pkg_size, mpi_type_array, p_out, 700001, MPI_COMM_WORLD);
	    MPI_Send(b3, pkg_size, mpi_type_key,    p_out, 700001, MPI_COMM_WORLD);
	    packages_sent++;
	  }
      }
    if(i3!=0)
      {
	i3 = 0;
	MPI_Send(a3, pkg_size, mpi_type_array, p_out, 700001, MPI_COMM_WORLD);
	MPI_Send(b3, pkg_size, mpi_type_key,    p_out, 700001, MPI_COMM_WORLD);
	packages_sent++;
      }  

    free(a1);
    free(a2);
    free(a3);
    free(b1);
    free(b2);
    free(b3);
  }


  void find_ROMS (int rank, int np, int* p1, int* p2)
  {
    //cout << "rank " << rank << " called _roms " << endl;
    int p_in, p_out;
    p_in = rank - 1;
    if(rank%4==1 && rank+1<np)
      p_out = rank+1;
    else if(rank%4==1)
      {
	int q1, q2, q3;
	find_SRRMS(rank+1, np, &q1, &q2, &q3, true, false);
	p_out = q3;
      }
    else 
      p_out = rank-1;
    *p1 = p_in;
    *p2 = p_out;
    //cout << "rank " << rank << " _roms " << p_in << "/" << p_out << endl;
  
  }

  //send, recv+recv, send
  void find_SRRMS (int rank, int np, int* p1, int* p2, int* p3, bool ignore_in, bool ignore_out)
  {
  
    if(rank%2!=0)
      {
	//      cout << "rank " << rank << " reached end of recursion SRRMS, use ROMS" << endl;
	*p1 = rank -1;
	*p2 = rank;
	*p3 = 0;
	//cout << "rank " << rank << "_srrms (after _roms) " << *p1 << "/" << *p2 << "/" << *p3 << endl;
	return;
      }
    int p_in1, p_in2, p_out;
    int k = 1;
    int block_size = 2;
    int first_active = 1;
    bool found = false;
    while(!found)
      {
	block_size *=2;
	k++;
	first_active *=2;
	int am_i = first_active;
	while(am_i<2*np)
	  {
	    if(am_i == rank)
	      {
		found = true;
		am_i = 2*np;
	      }
	    else
	      am_i+=block_size;
	  }
      }
    p_in1 = rank - block_size/4;
    p_in2 = rank + block_size/4;
    if(block_size>=np) //send to 0
      {
	p_out = 0;
	//cout << "rank " << rank << " gives back to start " << endl;
      }
    else if((rank/block_size)%2==0)
      p_out = rank + block_size/2;
    else
      p_out = rank - block_size/2;

    //  cout << "rank " << rank << " _srrms " << p_in1 << "/" << p_in2 << "/" << p_out << endl;
  
    if(!ignore_in) //look up, prioritize left
      {
	if(p_in1>np-1)
	  {
	    int q1, q2, q3;
	    //cout << "rank " << rank << " go left up to " << p_in1 << endl;
	    find_SRRMS(p_in1, np, &q1, &q2, &q3, false, true);
	    p_in1 = q1;
	  }
	else if(p_in2>np-1)
	  {
	    int q1, q2, q3;
	    //cout << "rank " << rank << " go right up to " << p_in2 << endl;
	    find_SRRMS(p_in2, np, &q1, &q2, &q3, false, true);
	    p_in2 = q1;
	  }
      }
  
    if(!ignore_out)
      {
	if(p_out>np-1) //look down
	  {
	    int q1, q2, q3;
	    find_SRRMS(p_out, np, &q1, &q2, &q3,  true, false);
	    p_out = q3;
	  }
      }
    *p1 = p_in1;
    *p2 = p_in2;
    *p3 = p_out;

  }

  template<typename DT, NODE_TYPE NT, typename TSIZEFUNC, typename TFUNC>
  void streamed_key_merge_templated (DT* array, 
				     typename key_trait<NT>::TKEY* array_keys, 
				     int base_array_size, int pkg_size,
				     TSIZEFUNC sf, TFUNC f)
  {
    // MPI_Datatype mpi_type_array = MyGetMPIType<DT>();
    typedef typename key_trait<NT>::TKEY tkey;
    // MPI_Datatype mpi_type_key = MyGetMPIType<tkey>();     

    int rank, np;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &np);
 
    bool even = 1-rank%2;
    //first step
 
    if(rank == 0)
      {
	Array<MPI_Request> requests;
	//packaged_send
	packaged_buffered_send<DT,NT>(rank, np, array, array_keys, base_array_size, pkg_size, 1, requests);

	int n;
	MPI_Recv(&n, 1, MPI_INT, MPI_ANY_SOURCE, 700001, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
	int n_pkg = n/pkg_size + ( (n%pkg_size)?1:0);

	sf(n);
	
	/*
	DT* end = (DT*) malloc(pkg_size * sizeof(DT));
	tkey*    end_keys = (tkey*)    malloc(pkg_size * sizeof(tkey));
	*/
	Array<DT> end(pkg_size);
	Array<tkey> end_keys(pkg_size);

	for(int k=0;k<n_pkg-1;k++)
	  {
	    // MPI_Recv(&end[0]     , pkg_size, mpi_type_array, MPI_ANY_SOURCE, 700001, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
	    // MPI_Recv(&end_keys[0], pkg_size, mpi_type_key   , MPI_ANY_SOURCE, 700001, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

	    MyMPI_Recv(end, MPI_ANY_SOURCE, 700001, MPI_COMM_WORLD);
	    MyMPI_Recv(end_keys, MPI_ANY_SOURCE, 700001, MPI_COMM_WORLD);

	    //cout << "0 received pkg " << k << "/" << n_pkg << endl;
	    for(int j = 0; j < pkg_size; j++)
	      f(end_keys[j], end[j]);
	  }
	// MPI_Recv(&end[0]     , pkg_size, mpi_type_array, MPI_ANY_SOURCE, 700001, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
	// MPI_Recv(&end_keys[0], pkg_size, mpi_type_key   , MPI_ANY_SOURCE, 700001, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
	MyMPI_Recv(end, MPI_ANY_SOURCE, 700001, MPI_COMM_WORLD);
	MyMPI_Recv(end_keys, MPI_ANY_SOURCE, 700001, MPI_COMM_WORLD);

	for(int j=0;(n_pkg-1)*pkg_size+j < n;j++)
	  f(end_keys[j], end[j]);      
	MyMPI_WaitAll (requests);
	// free(end);
	// free(end_keys);
      }
    else if(even)
      {
	// -> RRM
	int p_in1, p_in2, p_out;
	find_SRRMS (rank, np, &p_in1, &p_in2, &p_out, false, false);
	if(np-1 == rank) //is on right border
	  {
	    //cout << "rank " << rank << " (irregularly) gets from " << p_in1 << " and sends to " << p_out << endl;
	    merge_own_in_out <DT,NT> (rank, np, pkg_size, array, array_keys, base_array_size, p_in1, p_out);
	  }
	else //regular
	  {
	    //cout << "rank " << rank << " sends to " << rank+1 << " then gets from " << p_in1 << "/" << p_in2 << " and sends to " << p_out << endl;
	    Array<MPI_Request> requests;
	    packaged_buffered_send<DT,NT>(rank, np, array, array_keys, base_array_size, pkg_size, rank+1, requests);
	    merge_in_in_out<DT,NT>(pkg_size, rank, np, p_in1, p_in2, p_out);
	    MyMPI_WaitAll (requests);
	  }
      }
    else
      {
	// ROM ->
	int p_in,p_out;
	find_ROMS(rank, np, &p_in, &p_out);
	//cout << "rank " << rank << " gets from " << p_in << " and sends to " << p_out << endl;
	merge_own_in_out<DT,NT>(rank, np, pkg_size, array, array_keys, base_array_size, p_in, p_out);
      }
  }

  template <class T>
  void MyQuickSortI (FlatArray<T> data, FlatArray<int> index)
  {
    if (index.Size() <= 1) return;

    int i = 0;
    int j = index.Size()-1;

    int midval = index[ (i+j)/2 ];
  
    do
      {
	while (data[index[i]] < data[midval]) i++;
	while (data[midval] < data[index[j]]) j--;
	/*
	  while (less (data[index[i]],data[midval])  ) i++;
	  while (less (data[midval],  data[index[j]])) j--;
	*/
      
	if (i <= j)
	  {
	    Swap (index[i], index[j]);
	    i++; j--;
	  }
      }
    while (i <= j);

    MyQuickSortI (data, index.Range (0, j+1));
    MyQuickSortI (data, index.Range (i, index.Size()));
  }






  template <NODE_TYPE NT, typename T, typename TSIZEFUNC, typename TFUNC>
  void GatherNodalData (const MeshAccess & ma, FlatArray<T> data, 
			TSIZEFUNC sf, TFUNC f)
  {
    typedef typename key_trait<NT>::TKEY TKEY;

    Array<T> local_data;
    Array<TKEY> global_keys;
      
    // gather local data where I am master
    int myid = MyMPI_GetId();
    for (int i = 0; i < ma.GetNNodes(NT); i++)
      {
	bool ismaster = true;
	for (int p : ma.GetDistantProcs (Node(NT,i)))
	  if (p < myid) ismaster = false;

	if (ismaster)
	  {
	    local_data.Append (data[i]);
	    TKEY key1 = GetGlobalNodeId<NT>(ma,i);
	    global_keys.Append (key1);
	  }
      }
  
    Array<int> index (local_data.Size());
    for (int k = 0; k < index.Size(); k++) index[k] = k;

    MyQuickSortI (global_keys, index);

    local_data = Array<T> (local_data[index]);
    global_keys = Array<TKEY> (global_keys[index]);
    

    streamed_key_merge_templated<T,NT> (&local_data[0], &global_keys[0], local_data.Size(), 10000, sf, f);
  }



}
#endif