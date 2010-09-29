#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

#include "LibDefs.h"
#include "GMem.h"
#include "GContainers.h"

/////////////////////////////////////////////////////////////////////////////////////////////////
template <class StackType>
Stack<StackType>::Stack()
{
	Top = 0;
}

template <class StackType>
Stack<StackType>::~Stack()
{
	Empty();
}

template <class StackType>
void Stack<StackType>::Push(StackType *Data)
{
	StackType *Temp = new StackNode;

	if (Temp)
	{
		Temp->Next = Top;
		Top = Temp;
		Top->Data = Data;
	}
}

template <class StackType>
StackType *Stack<StackType>::Pop()
{
	if (Top)
	{
		StackType *Data = Top->Data;
		StackNode *Temp = Top;
		Top = Top->Next;

		delete Temp;
		return Data;
	}

	return 0;
}

template <class StackType>
StackType *Stack<StackType>::Peek()
{
	return (Top) ? Top->Data : 0;
}

template <class StackType>
int Stack<StackType>::IsEmpty()
{
	return (Top) ? true : false;
}

template <class StackType>
void Stack<StackType>::Empty()
{
	while (!IsEmpty())
	{
		Pop();
	}
}

template <class QueType>
Que<QueType>::Que()
{
	First = Last = 0;
}

template <class QueType>
Que<QueType>::~Que()
{
	Empty();
}

template <class QueType>
void Que<QueType>::Insert(QueType *Data)
{
	if (Last)
	{
		Last->Next = new QueNode;
		Last = Last->Next;
	}
	else
	{
		First = Last = new QueNode;
	}

	Last->Next = 0;
	Last->Data = Data;
}

template <class QueType>
QueType *Que<QueType>::Delete()
{
	if (First)
	{
		QueNode *Temp = First;
		QueData *Data = First->Data;
		First = First->Next;
		if (!First) Last = 0;		

		delete Temp;
		return Data;
	}

	return 0;
}

template <class QueType>
QueType *Que<QueType>::Peek()
{
	return (First) ? First->Data : 0;
}

template <class QueType>
int Que<QueType>::IsEmpty()
{
	return (First) ? false : true;
}

template <class QueType>
void Que<QueType>::Empty()
{
	while (!IsEmpty())
	{
		Delete();
	}
}

template <class PQueType>
PriorityQue<PQueType>::PriorityQue()
{
	Nodes = 0;
	Top = 0;
	Size = 0;
	SetSize(DEFAULT_PQUE_SIZE);
}

template <class PQueType>
PriorityQue<PQueType>::~PriorityQue()
{
	Empty();
}

template <class PQueType>
void PriorityQue<PQueType>::SetSize(long NewSize)
{
	if (NewSize > 0)
	{
		PQueNode *Temp = new PQueNode[NewSize];
		
		if (Temp)
		{
			memset(Temp,0,sizeof(PQueNode)*NewSize);
			
			if (Top)
			{
				memcpy(Temp,Top,sizeof(PQueNode)*Nodes);
				delete [] Top;
			}
			
			Top = Temp;
			Size = NewSize;
		}
	}
	else
	{
		Empty();
	}
}

template <class PQueType>
void PriorityQue<PQueType>::BubbleUp(long Start)
{
	long Parent = (Start - 1) / 2;
	
	while (Start)
	{
		if (Top[Parent].Key > Top[Start].Key)
		{
			PQueNode Temp = Top[Parent];
			Top[Parent] = Top[Start];
			Top[Start] = Temp;			

			Start = Parent;
			Parent = (Start - 1) / 2;
		}
		else
		{
			Start = 0;
		}
	}
}

template <class PQueType>
void PriorityQue<PQueType>::BubbleDown()
{
	long Start = 0;
	long Child = 1;
	
	while (Child < Nodes)
	{
		long Swap = (Top[Child].Key < Top[Child+1].Key)
				? Child : Child + 1;
		Swap = min(Swap, Nodes-1);

		if (Top[Swap].Key < Top[Start].Key)
		{
			PQueNode Temp = Top[Swap];
			Top[Swap] = Top[Start];
			Top[Start] = Temp;			
			Start = Swap;
			Child = (Start * 2) + 1;
		}
		else
		{
			Child = Nodes;
		}
	}
}

template <class PQueType>
void PriorityQue<PQueType>::Insert(PQueType *Data,long Key)
{
	if (!Top || Nodes >= Size)
	{
		SetSize((Size) ? Size * 2 : DEFAULT_PQUE_SIZE);
	}

	if (Top && Nodes < Size)
	{
		Top[Nodes].Data = Data;
		Top[Nodes].Key = Key;
		Nodes++;

   		BubbleUp(Nodes-1);
	}
}

template <class PQueType>
PQueType *PriorityQue<PQueType>::DeleteMin()
{
	if (Top && Nodes > 0)
	{
		PQueType *Temp = Top->Data;
		
		Nodes--;

		if (Nodes >= 1)
		{
			Top[0] = Top[Nodes];			
			BubbleDown();
		}
		
		return Temp;
	}
	
	return 0;
}

template <class PQueType>
PQueType *PriorityQue<PQueType>::Peek()
{
	return (Top) ? Top->Data : 0;
}

template <class PQueType>
int PriorityQue<PQueType>::IsEmpty()
{
	return (Top) ? false : true;
}

template <class PQueType>
void PriorityQue<PQueType>::Empty()
{
	delete [] Top;
	Top = 0;
	Size = 0;
	Nodes = 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////////
