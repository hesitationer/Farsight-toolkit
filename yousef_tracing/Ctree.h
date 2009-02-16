///////////////////////////////////////////////////////////////////////
// File: Ctree.h
// By:   Khalid Al-Kofahi
// Created: 2-12-99
//
// This file contains the declaration of a template tree class and the required
// supporting classes

#ifndef CTree_h
#define CTree_h

// at any node in the tree, we can have a maximum of 10 Children
#define MaxNumOfChildren 10

//////////////////////////////////////////////////////////////////////////////
//									Class CTreeNode											 //
//////////////////////////////////////////////////////////////////////////////

class CTreeNode
{
public:
	// CTOR
	CTreeNode(int, int , CIntersectionPoint *); 
	// DTOR
	~CTreeNode();

	// print
	void Print(FILE *outFile);

	// draw the parent segment and the subtree in the
	// projection images and using the given color
	void Draw(unsigned char color);

	// Get the longest path from this point on
	int FindLongestPath(CDLList<int> &);

	// write this tree to the given file in Neurolucida format using the
	// given color
	void PrintNeurolucidaFormat(ofstream &, int );

	// data members
	
	// my data
	int m_iSegmentID;
	int m_iNodeID;

	// A flag that can be used in traversing the nodes
	// or any other purpose as defined by the user
	int m_iProcessedFlag;
	// the actual number of children nodes
	int m_iNumOfChildren;
	// the location of the node
	CPoint m_Point;

	// sum of my childrens lengths
	int m_iSumOfAllBranches;

	// Node Type; ROOT, LEAF, NORMAL
	TreeNodeType m_Type;
	// an array of pointers to other tree nodes
	CTreeNode *m_aChildren[MaxNumOfChildren];

	// 
private:
	void Init();
};


//////////////////////////////////////////////////////////////////////////////
//									Class CTree    											 //
//////////////////////////////////////////////////////////////////////////////

class CTree
{
public:
	// CTOR
	CTree();
	// DTOR
	~CTree();

	// construct a tree from the given intersection point
	void ConstructTree(int , CIntersectionPoint *pIntPoint);

	// Find the longest path in this tree and return its length
	int FindLongestPath();

	// print
	void Print(FILE *);

	// draw the segments of the tree in the
	// projection images using the given color
	void Draw(unsigned char color);

	// write this tree to the given file in Neurolucida format using the
	// given color
	void PrintNeurolucidaFormat(ofstream &, int );

	// data members

	// my root
	CTreeNode *root;

	// the length sum of all my branches
	int m_iSumOfAllBranches;

	// the longest path in this tree
	CDLList<int> m_LongestPath;
	// the length of the longest path
	int m_iLengthOfLongestPath;
};


#endif
