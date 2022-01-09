# Project 3: Memory Allocator
In this project we are using memory managment skills to manage our own memory, and we create our own malloc, free, calloc, realloc, and other types of FSM algortithms like first fit, best fit, and worst fit. Our malloc allocates pages of memory generally a page being 4096 bytes and we only allocate a page if there is no reusable block that we can use, our calloc basically just calls realloc except the difference is that it calculates how much bytes it needs for us and clans out all the memory, and our realloc basically mallocs a larger size and copies over the memory from the previous block, the malloc also calls split_block which splits the block is there is enough memory to be split, and we also have a free which sets the blocks to free and munmaps if an entire page is free by checking for region ids, we also have a first fit algorithm which checks for the first possible reusable block and then returns it, we have a best fit which checks for the best reusable block and returns it, and a worst fit which checks for the largest resuable block and uses it, free also calls merge_block which checks for blocks to the left and right and sees if it can merge them.



To compile and use the allocator:

```bash
make
LD_PRELOAD=$(pwd)/allocator.so ls /
```

(in this example, the command `ls /` is run with the custom memory allocator instead of the default).

## Testing

To execute the test cases, use `make test`. To pull in updated test cases, run `make testupdate`. You can also run a specific test case instead of all of them:

```
# Run all test cases:
make test

# Run a specific test case:
make test run=4

# Run a few specific test cases (4, 8, and 12 in this case):
make test run='4 8 12'
```
