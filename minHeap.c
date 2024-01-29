
typedef struct minHeap
{
	struct PCB **arr;
	int count, size;
} minHeap;

void swap(struct PCB **a, struct PCB **b)
{
	struct PCB *temp = *a;
	*a = *b;
	*b = temp;
}

int parent(int i)
{
	return (i - 1) / 2;
}
int right(int i)
{
	return 2 * i + 1;
}

int left(int i)
{
	return 2 * i + 2;
}

void minHeapify(int i, minHeap *heap)
{
	int l, r, smallest;
	r = right(i);
	l = left(i);
	if (l < (*heap).count && (*(*heap).arr[l]).heapPriority < (*(*heap).arr[i]).heapPriority)
	{
		smallest = l;
	}
	else
	{
		smallest = i;
	}

	if (r < (*heap).count && (*(*heap).arr[r]).heapPriority < (*(*heap).arr[smallest]).heapPriority)
	{
		smallest = r;
	}

	if (smallest != i)
	{
		swap(&(*heap).arr[i], &(*heap).arr[smallest]);
		minHeapify(smallest, heap);
	}
}

bool initialize(int num, minHeap *heap)
{
		// double initialization will cause memory leaks
		(*heap).arr = malloc(sizeof(struct PCB *) * num);
		(*heap).size = num;
		(*heap).count = 0;
		return 1;
}
void freeHeap(minHeap *heap)
{
		free (heap->arr);
}

int heapMin(minHeap *heap)
{
	if ((*heap).count != 0)
		return (*(*heap).arr[0]).heapPriority;
	return 0;
}

struct PCB *heapExtractMin(minHeap *heap)
{
	if ((*heap).count == 0)
	{
		struct PCB empty_struct  = {0};
		return NULL ;
	}
	struct PCB *min = (*heap).arr[0];
	(*heap).arr[0] = (*heap).arr[(*heap).count - 1];
	(*heap).count--;
	minHeapify(0, heap);
	return min;
}

void insertValue(struct PCB *key, minHeap *heap)
{
	(*heap).arr[(*heap).count] = key;
	int placment = (*heap).count;
	(*heap).count++;
	while (placment > 0 && (*(*heap).arr[parent(placment)]).heapPriority > (*(*heap).arr[placment]).heapPriority)
	{
		swap(&(*heap).arr[placment], &(*heap).arr[parent(placment)]);
		placment = parent(placment);
	}
}

int getcount(minHeap *heap)
{
	return (*heap).count;
}

// void print()const {

// 	if (count)
// 		cout << *arr[0];
// 	for (int i = 1; i < count; i++)
// 	{
// 		cout <<","<<*arr[i];
// 	}
// }

short isEmpty(minHeap *heap)
{
	if ((*heap).count == 0)
		return 1;
	else
		return 0;
}
