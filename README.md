# qHilbert [![GitHub license](https://img.shields.io/badge/license-MIT-blue.svg)](https://raw.githubusercontent.com/Wunkolo/qHilbert/master/LICENSE)

qHilbert is a vectorized speedup of Hilbert curve generation using SIMD intrinsics.

---

A hilbert curve is a space filling self-similar curve that provides a mapping between 2D space to 1D, and 1D to 2D space while preserving locality between mappings. Hilbert curves split a finite 2D space into recursive quadrants(similar to a [dense quad-tree](https://en.wikipedia.org/wiki/Quadtree)) and traverse each quadrant in a rotated "U" shape at each iteration such that every quadrant gets fully visited before moving onto the next one.

A one-dimensional index(finite distance along the curve) can be mapped into a 2D point on the hilbert curve by recursively adjusting the position of the resulting 2d point at each recursion. The base case for a first-order hilbert curve is a 2D space of width 2 split into 4 quadrants. Each quadrant is visited in a "U" shape at the distances 0, 1, 2, and 3.

```
First order Hilbert curve of Width 2
+---+---+        +-------+
| 0 | 3 |        | +   + |
|---+---| -----> | |   | |
| 1 | 2 |        | +---+ |
+---+---+        +-------+
+----+----+
| 00 | 11 |
|----+----|
| 01 | 10 |
+----+----+
```

A second-order Hilbert curve of Width 4 is induced using the previous order Hilbert curve of Width 2 by further sub-dividing the quadrants 0,1,2, and 3 into even smaller quadrants, and traversing each quadrant in rotated "U" shapes yet again while the upper quadrands are being traversed in a "U" shape as well before moving on to the next quadrant. This "U" shaped traversal is happening at multiple levels of recursion at once.
The "U" shape traversal is rotated in such a way to keep the endpoints of the "U" close to the entrance and exits of the quadrant being traversed at the previous level

```
Second order Hilbert curve of Width 4
+----+----+----+----+      +-------+-------+      +---------------+
|  0 |  1 | 14 | 15 |      | +---+ | +---+ |      | +---+   +---+ |
|----0----|----3----|      |   * | | | *   |      |     |   |     |
|  3 |  2 | 13 | 12 |      | +---+ | +---+ |      | +---+   +---+ |
+----+----+----+----+ ---> +-------+-------+ ---> | |           | |
|  4 |  7 |  8 | 11 |      | +   + | +   + |      | +   +---+   + |
|----1----|----2----|      | | * | | | * | |      | |   |   |   | |
|  5 |  6 |  9 | 10 |      | +---+ | +---+ |      | +---+   +---+ |
+----+----+----+----+      +-------+-------+      +---------------+
* - Marks the vertices of the previous order-1 curve
```

Further iteration Hilbert curves of Width `N`(where `N` is a positive power of two) can be calculated recursively by generating the first-order, then the second order, then the third, up to the `log2(N)`th order to generate a curve of `N^2 - 1` indices(distances).

A pattern that can be noticed is that with each fully visited quadrant, the lower 2-bits of the index will describe what quadrant it is currently in, while the supersedeing 2 bits describe what upper-order quadrant the point is in. With each recursive level, what was once the previous-order now has its bits shifted to the left 2 bits to allow for the new quadrants to utilize these two bits to identify what quadrant it is in.

```
Order 1
+---+---+        +-------+
| 0 | 3 |        | +   + |
|---+---| -----> | |   | |
| 1 | 2 |        | +---+ |
+---+---+        +-------+
        Binary
        +----+----+
        | 00 | 11 |
        |----+----|
        | 01 | 10 |
        +----+----+
```
```
Order 2
+----+----+----+----+      +---------------+
|  0 |  1 | 14 | 15 |      | +---+   +---+ |
|----+----|----+----|      |     |   |     |
|  3 |  2 | 13 | 12 |      | +---+   +---+ |
+----+----+----+----+ ---> | |           | |
|  4 |  7 |  8 | 11 |      | +   +---+   + |
|----+----|----+----|      | |   |   |   | |
|  5 |  6 |  9 | 10 |      | +---+   +---+ |
+----+----+----+----+      +---------------+
        Binary
        +------+------+------+------+
        | 0000 | 0001 | 1110 | 1111 |
        |------+------|------+------|
        | 0011 | 0010 | 1101 | 1100 |
        +------+------+------+------+
        | 0100 | 0111 | 1000 | 1011 |
        |------+------|------+------|
        | 0101 | 0110 | 1001 | 1010 |
        +------+------+------+------+

        Binary (Grouped by 2 bits)
        +-------+-------+-------+-------+
        | 00 00 | 00 01 | 11 10 | 11 11 |
        |-------+-------|-------+-------|
        | 00 11 | 00 10 | 11 01 | 11 00 |
        +-------+-------+-------+-------+
        | 01 00 | 01 11 | 10 00 | 10 11 |
        |-------+-------|-------+-------|
        | 01 01 | 01 10 | 10 01 | 10 10 |
        +-------+-------+-------+-------+
```

Notice the most significant pair of bits in Order 2(the left pair of bits) are exactly the same as the the Order 1 bits. This pattern in bits can be generalized to convert any index/distance into its appropriate 2D point by utilizing this pattern.

Given any positive integer index `d` where `d <= (N^2 - 1)` it is possible to group up the pair of bits found in the integer to identify exactly what quadrant it is in, and also determine what offsets would need to be added to achieve the 2D point located at that distance.