
#ifndef _SPARSEMATRIX_H
#define _SPARSEMATRIX_H

template <typename TYPE, typename TROW, typename TCOL>
class SparseMatrix
{
public:
	SparseMatrix() {}

	SparseMatrix(const SparseMatrix &) = delete;
	SparseMatrix& operator =(SparseMatrix &) = delete;

	inline TYPE& operator()(TROW i, TCOL j)
	{
		return matrix[i][j];
	}

	inline TYPE operator()(TROW i, TCOL j) const
	{
		auto row = matrix.find(i);
		if (row == matrix.end())
			return 0;

		auto col = row->find(j);
		if (col == row->end())
			return 0;
		return *col;
	}

protected:
	// for each row there is a map of columns
	map<TROW, map<TCOL, TYPE>> 	matrix;
};

#endif  /* _SPARSEMATRIX_H */
