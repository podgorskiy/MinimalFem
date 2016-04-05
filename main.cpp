#include <Eigen/Dense>
#include <Eigen/Sparse>
#include <string>
#include <vector>
#include <iostream>
#include <fstream>

struct Element
{
	void CalculateStiffnessMatrix(const Eigen::VectorXf& nodesX, const Eigen::VectorXf& nodesY, const Eigen::Matrix3f& D);
	void GetTriplets(std::vector<Eigen::Triplet<float> >& triplets);

	Eigen::Matrix<float, 6, 6> K;
	int nodesIds[3];

	EIGEN_MAKE_ALIGNED_OPERATOR_NEW
};

struct Constraint
{
	enum Type
	{
		UX = 1 << 0,
		UY = 1 << 1,
		UXY = UX | UY
	};
	int node;
	Type type;
};

struct Mesh 
{
	int                      	nodesCount;
	Eigen::VectorXf          	nodesX;
	Eigen::VectorXf          	nodesY;
	Eigen::VectorXf          	loads;
	std::vector< Element, Eigen::aligned_allocator<Element> >   	elements;
	std::vector< Constraint >	constraints;
	std::vector< int >	freeNodes;

	void ReadMesh(std::ifstream& infile)
	{
		infile >> nodesCount;
		nodesX.resize(nodesCount);
		nodesY.resize(nodesCount);

		for (int i = 0; i < nodesCount; ++i)
		{
			infile >> nodesX[i] >> nodesY[i];
		}

		int elementCount;
		infile >> elementCount;

		for (int i = 0; i < elementCount; ++i)
		{
			Element element;
			infile >> element.nodesIds[0] >> element.nodesIds[1] >> element.nodesIds[2];
			elements.push_back(element);
		}
	}

	void Merge(const Mesh& other)
	{
		nodesX.conservativeResize(nodesCount + other.nodesCount);
		nodesY.conservativeResize(nodesCount + other.nodesCount);
		for (int i = 0; i < other.nodesCount; ++i)
		{
			nodesX[nodesCount + i] = other.nodesX[i];
			nodesY[nodesCount + i] = other.nodesY[i];
		}
		for (auto it = other.elements.begin(); it != other.elements.end(); ++it)
		{
			Element e;
			e.nodesIds[0] = it->nodesIds[0] + nodesCount;
			e.nodesIds[1] = it->nodesIds[1] + nodesCount;
			e.nodesIds[2] = it->nodesIds[2] + nodesCount;
			elements.push_back(e);
		}
		nodesCount += other.nodesCount;
		loads.resize(2 * nodesCount);
		loads.setZero();
	}

	void Weld(int a, int b)
	{
		float deltaX = nodesX[a] - nodesX[b];
		float deltaY = nodesY[a] - nodesY[b];
		for (auto it = elements.begin(); it != elements.end(); ++it)
		{
			Eigen::Matrix<float, 6, 1> deformation;
			deformation.setZero();
			
			for (int i = 0; i < 3; ++i)
			{
				if (it->nodesIds[i] == b)
				{
					deformation[2 * i + 0] = deltaX;
					deformation[2 * i + 1] = deltaY;
					it->nodesIds[i] = a;

					Eigen::Matrix<float, 6, 1> F = -it->K * deformation;
					for (int j = 0; j < 3; ++j)
					{
						loads[2 * it->nodesIds[j] + 0] += F[2 * j + 0];
						loads[2 * it->nodesIds[j] + 1] += F[2 * j + 1];
					}

					break;
				}
			}
		}
		freeNodes.push_back(b);
	}
};

void Element::CalculateStiffnessMatrix(const Eigen::VectorXf& nodesX, const Eigen::VectorXf& nodesY, const Eigen::Matrix3f& D)
{
	Eigen::Vector3f x, y;
	x << nodesX[nodesIds[0]], nodesX[nodesIds[1]], nodesX[nodesIds[2]];
	y << nodesY[nodesIds[0]], nodesY[nodesIds[1]], nodesY[nodesIds[2]];
	
	Eigen::Matrix3f C;
	C << Eigen::Vector3f(1.0f, 1.0f, 1.0f), x, y;
	
	Eigen::Matrix3f IC = C.inverse();
	Eigen::Matrix<float, 3, 6> B;

	for (int i = 0; i < 3; i++)
	{
		B(0, 2 * i + 0) = IC(1, i);
		B(0, 2 * i + 1) = 0.0f;
		B(1, 2 * i + 0) = 0.0f;
		B(1, 2 * i + 1) = IC(2, i);
		B(2, 2 * i + 0) = IC(2, i);
		B(2, 2 * i + 1) = IC(1, i);
	}
	K = B.transpose() * D * B * C.determinant() / 2.0f;
}

void Element::GetTriplets(std::vector<Eigen::Triplet<float> >& triplets)
{
	for (int i = 0; i < 3; i++)
	{
		for (int j = 0; j < 3; j++)
		{
			Eigen::Triplet<float> trplt11(2 * nodesIds[i] + 0, 2 * nodesIds[j] + 0, K(2 * i + 0, 2 * j + 0));
			Eigen::Triplet<float> trplt12(2 * nodesIds[i] + 0, 2 * nodesIds[j] + 1, K(2 * i + 0, 2 * j + 1));
			Eigen::Triplet<float> trplt21(2 * nodesIds[i] + 1, 2 * nodesIds[j] + 0, K(2 * i + 1, 2 * j + 0));
			Eigen::Triplet<float> trplt22(2 * nodesIds[i] + 1, 2 * nodesIds[j] + 1, K(2 * i + 1, 2 * j + 1));

			triplets.push_back(trplt11);
			triplets.push_back(trplt12);
			triplets.push_back(trplt21);
			triplets.push_back(trplt22);
		}
	}
}

void SetConstraints(Eigen::SparseMatrix<float>::InnerIterator& it, int index)
{
	if (it.row() == index || it.col() == index)
	{
		it.valueRef() = it.row() == it.col() ? 1.0f : 0.0f;
	}
}

void ApplyConstraints(Eigen::SparseMatrix<float>& K, const std::vector<Constraint>& constraints)
{
	std::vector<int> indicesToConstraint;

	for (std::vector<Constraint>::const_iterator it = constraints.begin(); it != constraints.end(); ++it)
	{
		if (it->type & Constraint::UX)
		{
			indicesToConstraint.push_back(2 * it->node + 0);
		}
		if (it->type & Constraint::UY)
		{
			indicesToConstraint.push_back(2 * it->node + 1);
		}
	}

	for (int k = 0; k < K.outerSize(); ++k)
	{
		for (Eigen::SparseMatrix<float>::InnerIterator it(K, k); it; ++it)
		{
			for (std::vector<int>::iterator idit = indicesToConstraint.begin(); idit != indicesToConstraint.end(); ++idit)
			{
				SetConstraints(it, *idit);
			}
		}
	}
}

int main(int argc, char *argv[])
{
	float poissonRatio = 0.3f;
	float youngModulus = 1.0f;
	
	Eigen::Matrix3f D;
	D <<
		1.0f,        	poissonRatio,	0.0f,
		poissonRatio,	1.0,         	0.0f,
		0.0f,        	0.0f,        	(1.0f - poissonRatio) / 2.0f;

	D *= youngModulus / (1.0f - pow(poissonRatio, 2.0f));
	
	std::ifstream infileA(argv[1]);
	std::ifstream infileB(argv[2]);
	
	Mesh tileA;
	Mesh tileB;

	tileA.ReadMesh(infileA);
	tileB.ReadMesh(infileB);

	std::vector<std::pair<int, int> > weldNodes;

	for (int i = 3; i < argc;)
	{
		weldNodes.push_back(std::make_pair(atoi(argv[i + 0]), atoi(argv[i + 1]) + tileA.nodesCount));
		i += 2;
	}

	tileA.Merge(tileB);

	for (auto it = tileA.elements.begin(); it != tileA.elements.end(); ++it)
	{
		it->CalculateStiffnessMatrix(tileA.nodesX, tileA.nodesY, D);
	}
	
	for (auto it = weldNodes.begin(); it != weldNodes.end(); ++it)
	{
		tileA.Weld(it->first, it->second);
	}

	std::vector<Eigen::Triplet<float> > triplets;
	for (auto it = tileA.elements.begin(); it != tileA.elements.end(); ++it)
	{
		it->GetTriplets(triplets);
	}

	Eigen::SparseMatrix<float> globalK(2 * tileA.nodesCount, 2 * tileA.nodesCount);
	globalK.setFromTriplets(triplets.begin(), triplets.end());

	for (auto it = tileA.freeNodes.begin(); it != tileA.freeNodes.end(); ++it)
	{
		globalK.insert(2 * *it + 0, 2 * *it + 0) = 1.0f;
		globalK.insert(2 * *it + 1, 2 * *it + 1) = 1.0f;
	}

	Constraint c;
	c.node = 0;
	c.type = Constraint::UXY;
	tileA.constraints.push_back(c);
	c.node = 1;
	c.type = Constraint::UY;
	tileA.constraints.push_back(c);

	ApplyConstraints(globalK, tileA.constraints);

	Eigen::SimplicialLDLT<Eigen::SparseMatrix<float> > solver(globalK);

	Eigen::VectorXf displacements = solver.solve(tileA.loads);

	std::ofstream outfile("stitched.txt");

	outfile << tileA.nodesCount << '\n';

	for (int i = 0; i < tileA.nodesCount; ++i)
	{
		outfile 
			<< tileA.nodesX[i] + displacements[2 * i + 0] << ' '
			<< tileA.nodesY[i] + displacements[2 * i + 1]
			<< '\n';
	}

	outfile << tileA.elements.size() << '\n';

	for (auto it = tileA.elements.begin(); it != tileA.elements.end(); ++it)
	{
		Element element;
		outfile << it->nodesIds[0] << ' ' << it->nodesIds[1] << ' ' << it->nodesIds[2] << '\n';
	}

	return 0;
}