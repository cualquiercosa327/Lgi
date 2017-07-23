#ifndef _LGI_QUICK_SORT_H_
#define _LGI_QUICK_SORT_H_

//////////////////////////////////////////////////////////////////////////////////
//  quickSort from http://alienryderflex.com/quicksort/
//
//  This public-domain C implementation by Darel Rex Finley. And then modified by
//  Matthew Allen to use a callback comparison function with user data in C++.
//
template<typename T, typename U>
void LgiQuickSort(T *arr, ssize_t elements, int (*comp)(T&, T&, U), U user)
{
	#define  MAX_LEVELS  300

	if (!arr || !comp)
		return;

	ssize_t beg[MAX_LEVELS], end[MAX_LEVELS], i = 0, L, R, swap;
	T piv;

	beg[0] = 0;
	end[0] = elements;
	
	while (i >= 0)
	{
		L = beg[i];
		R = end[i] - 1;
		if (L < R)
		{
			piv = arr[L];
			while (L < R)
			{
				while (comp(arr[R], piv, user) >= 0 && L < R)
					R--;
					
				if (L < R)
					arr[L++] = arr[R];
				
				while (comp(arr[L], piv, user) <= 0 && L < R)
					L++;
				
				if (L < R)
					arr[R--] = arr[L];
			}
			
			arr[L] = piv;
			beg[i+1] = L + 1;
			end[i+1] = end[i];
			end[i++] = L;
			
			if (end[i] - beg[i] > end[i-1] - beg[i-1])
			{
				swap = beg[i];
				beg[i] = beg[i-1];
				beg[i-1] = swap;
				swap = end[i];
				end[i] = end[i-1];
				end[i-1] = swap;
			}
		}
		else
		{
			i--;
		}
	}
}

#endif
