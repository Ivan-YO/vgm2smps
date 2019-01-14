#ifndef _ARRAY_MACRO_H
#define _ARRAY_MACRO_H

#define ARRLENGTH(ARR) (sizeof(ARR) / sizeof(ARR[0]))

#define ARRAY_1D_SORTED_FIND_NEAREST_RANGE(_T, _ARR, _CONST, _INDEX_VAR, _DIM1_LEN) \
do \
{ \
	size_t __size = _DIM1_LEN; \
	_INDEX_VAR = 0; \
	_T __const = _CONST; \
	_T __dist = abs((_ARR)[0] - __const); \
	_T __dist_prev = __dist; \
	_T __dist_temp; \
	for (size_t __x__ = 1; __x__ < __size; ++__x__) \
	{ \
		if (__dist_prev < __dist) break; \
		__dist_temp = abs((_ARR)[__x__] - __const); \
		if (__dist_temp < __dist) \
		{ \
			__dist = __dist_temp; \
			_INDEX_VAR = __x__; \
			__dist_prev = __dist; \
		} \
	} \
} while (false)

#define ARRAY_1D_SORTED_FIND_NEAREST(_T, _ARR, _CONST, _INDEX_VAR) \
ARRAY_1D_SORTED_FIND_NEAREST_RANGE(_T, _ARR, _CONST, _INDEX_VAR, ARRLENGTH((_ARR)))

#define ARRAY_2D_SORTED_FIND_NEAREST_RANGE(_T, _ARR, _CONST, _DIM1_INDEX_VAR, _DIM2_INDEX_VAR, _DIM1_LEN, _DIM2_LEN) \
do \
{ \
	size_t __size1 = _DIM1_LEN; \
	size_t __size2 = _DIM2_LEN; \
	_DIM1_INDEX_VAR = 0; \
	_DIM2_INDEX_VAR = 0; \
	_T __const = _CONST; \
	_T __dist = abs((_ARR)[0][0] - __const); \
	_T __dist_prev = __dist; \
	_T __dist_temp; \
	bool __find = false; \
	size_t __y__, __x__; \
	__y__ = 0; \
	__x__ = 1; \
	for (; __y__ < __size1; ++__y__) \
	{ \
		if (__find) break; \
		for (; __x__ < __size2; ++__x__) \
		{ \
			if (__dist_prev < __dist) __find = true; \
			if (__find) break; \
			__dist_temp = abs((_ARR)[__y__][__x__] - __const); \
			if (__dist_temp < __dist) \
			{ \
				__dist = __dist_temp; \
				_DIM2_INDEX_VAR = __x__; \
				_DIM1_INDEX_VAR = __y__; \
				__dist_prev = __dist; \
			} \
		} \
	__x__ = 0; \
	} \
} while (false)

#define ARRAY_2D_SORTED_FIND_NEAREST(_T, _ARR, _CONST, _DIM1_INDEX_VAR, _DIM2_INDEX_VAR) \
ARRAY_2D_SORTED_FIND_NEAREST_RANGE(_T, _ARR, _CONST, _DIM1_INDEX_VAR, _DIM2_INDEX_VAR, ARRLENGTH((_ARR)), ARRLENGTH((_ARR)[0]))

#define ARRAY_1D_UNSORTED_FIND_NEAREST_RANGE(_T, _ARR, _CONST, _INDEX_VAR, _DIM1_LEN) \
do \
{ \
	size_t __size = _DIM1_LEN; \
	_INDEX_VAR = 0; \
	_T __const = _CONST; \
	_T __dist = abs((_ARR)[0] - __const); \
	_T __dist_temp; \
	for (size_t __x__ = 1; __x__ < __size; ++__x__) \
	{ \
		__dist_temp = abs((_ARR)[__x__] - __const); \
		if (__dist_temp < __dist) \
		{ \
			__dist = __dist_temp; \
			_INDEX_VAR = __x__; \
		} \
	} \
} while (false)

#define ARRAY_1D_UNSORTED_FIND_NEAREST(_T, _ARR, _CONST, _INDEX_VAR) \
ARRAY_1D_UNSORTED_FIND_NEAREST_RANGE(_T, _ARR, _CONST, _INDEX_VAR, ARRLENGTH((_ARR)))

#define ARRAY_2D_UNSORTED_FIND_NEAREST_RANGE(_T, _ARR, _CONST, _DIM1_INDEX_VAR, _DIM2_INDEX_VAR, _DIM1_LEN, _DIM2_LEN) \
do \
{ \
	size_t __size1 = _DIM1_LEN; \
	size_t __size2 = _DIM2_LEN; \
	_DIM1_INDEX_VAR = 0; \
	_DIM2_INDEX_VAR = 0; \
	_T __const = _CONST; \
	_T __dist = abs((_ARR)[0][0] - __const); \
	_T __dist_temp; \
	size_t __y__, __x__; \
	__y__ = 0; \
	__x__ = 1; \
	for (; __y__ < __size1; ++__y__) \
	{ \
		for (; __x__ < __size2; ++__x__) \
		{ \
			__dist_temp = abs((_ARR)[__y__][__x__] - __const); \
			if (__dist_temp < __dist) \
			{ \
				__dist = __dist_temp; \
				_DIM2_INDEX_VAR = __x__; \
				_DIM1_INDEX_VAR = __y__; \
			} \
		} \
		__x__ = 0; \
	} \
} while (false)

#define ARRAY_2D_UNSORTED_FIND_NEAREST(_T, _ARR, _CONST, _DIM1_INDEX_VAR, _DIM2_INDEX_VAR) \
ARRAY_2D_UNSORTED_FIND_NEAREST_RANGE(_T, _ARR, _CONST, _DIM1_INDEX_VAR, _DIM2_INDEX_VAR, ARRLENGTH((_ARR)), ARRLENGTH((_ARR)[0]))

#endif // !_ARRAY_MACRO_H

