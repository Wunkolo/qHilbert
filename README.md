# qHilbert [![GitHub license](https://img.shields.io/badge/license-MIT-blue.svg)](https://raw.githubusercontent.com/Wunkolo/qHilbert/master/LICENSE)

qHilbert is a vectorized speedup of Hilbert curve generation using SIMD intrinsics.

---

|||
|:-:|:-:|
|![](images/HilbertRed-Gradient.gif)|![](images/HilbertRed-Trace.gif)|

A hilbert curve is a space filling self-similar curve that provides a mapping between 2D space to 1D, and 1D to 2D space while preserving locality between mappings. Hilbert curves split a finite 2D space into recursive quadrants(similar to a [full quad-tree](https://en.wikipedia.org/wiki/Quadtree)) and traverse each quadrant in recursive "U" shapes at each iteration such that every quadrant gets fully visited before moving onto the next one.

qHilbert is an attempt at a vectorized speedup of mapping multiple linear 1D indices into planar 2D points in parallel.

---

A one-dimensional index(finite distance along the curve) can be mapped into a 2D point on the hilbert curve by recursively adjusting the position of the resulting 2d point at each level recursion. The base case for a first-order hilbert curve is a 2D space of width 2 split into 4 quadrants. Each quadrant is visited in a "U" shape at the distances 0, 1, 2, and 3. Each quadrant being a vector-offset away from the origin( with `(0,0)` being the top-left). Every self-similar feature of the hilbert curve is rooted at this basis shape with 2D vectors taking on a "gray code" pattern in its elements.

```
First order Hilbert curve of Width 2
+---+---+        +-------+    0: ( 0, 0 )
| 0 | 3 |        | +   + |    1: ( 0, 1 )
|---+---| -----> | |   | |    2: ( 1, 1 )
| 1 | 2 |        | +---+ |    3: ( 1, 0 )
+---+---+        +-------+    (Notice the gray-code like pattern)
```

A second-order Hilbert curve of Width 4 is induced using the previous order Hilbert curve of Width 2 by further sub-dividing the quadrants 0,1,2, and 3 into even smaller quadrants, and traversing each quadrant in rotated "U" shapes yet again while the upper quadrands are being traversed in a "U" shape as well before moving on to the next quadrant. This "U" shaped traversal is happening at multiple levels of recursion at once.
The "U" shape traversal is rotated/flipped in such a way to keep the endpoints of the "U" close to the entrance and exits of the quadrant being traversed before it.

```
Second order Hilbert curve of Width 4
+----+----+----+----+      +-------+-------+      +---------------+
|  0 |  1 | 14 | 15 |      | +---+ | +---+ |      | +---+   +---+ |
|----0----|----3----|      >   0 | | | 3   >      |     |   |     |
|  3 |  2 | 13 | 12 |      | +---+ | +---+ |      | +---+   +---+ |
+----+----+----+----+ ---> +---V---+---^---+ ---> | |           | |
|  4 |  7 |  8 | 11 |      | +   + | +   + |      | +   +---+   + |
|----1----|----2----|      | | 1 | > | 2 | |      | |   |   |   | |
|  5 |  6 |  9 | 10 |      | +---+ | +---+ |      | +---+   +---+ |
+----+----+----+----+      +-------+-------+      +---------------+
```

Further iteration Hilbert curves of Width `N`(where `N` is a positive power of two) can be calculated recursively by generating the first-order, then the second order, then the third, up to the `log2(N)`th order to generate a curve of `N^2 - 1` indices(distances).

A pattern that can be noticed is that with each fully visited quadrant the lower 2-bits of the index would change the fastest, while each supersedeing 2 bits would seem to describe what upper-order quadrant the point is in though not exactly. If this were a quad-tree like structure where every quadrant was traversed exactly the same, then every group of 2 bits would describe exactly what sub-quadrant it was in, but hilbert curves are structured very differently such that the beginning and ends of the "U" shape are rotated and flipped around such that the "entrance" and "exits" of each "U" are always touching. The usual "∪" order could be a "⊂" or "∩" or "ᴝ" depending on what level of recursion and what quadrant the index happens to land in.

```
Order 1
+---+---+        +-------+   
| 0 | 3 |        | +   + |   
|---+---| -----> | |   | |   ( the "U" shape that ill be talking about)
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

Notice the most significant pair of bits in Order 2(the left pair of bits) are exactly the same as the the Order 1 bits. The lower pair follows the same `00`,`01`,`10`,`11` pattern but flipped and rotated depending on the previous Order's traversal. This pattern in bits can be generalized to convert any index/distance into its appropriate 2D point by utilizing this pattern.

Given any positive integer index `d` where `d <= (N^2 - 1)` it is possible to group up binary pairs that make up the integer to identify exactly what quadrant of each level of recursion it is in, and also determine what offsets would need to achieve the 2D point located at that distance.

The last 2 bits do not immediately land on the correct quadrant since the order in which the quadrands are traversed try to keep their "exits" and "entrance" touching each other, always flipping and rotating the upper two quadrants when necessary depending on the current recursion level.

Mapping an integer index into a 2D point involves the following steps:

General operation to convert from a 1D Index to a 2D vector
- Initialize your result vector (0,0)
- For each level of recursion, starting from Order 1:
  - Map the integer index into the correct "U" quadrant vector
    - [Convert the decimal value into a 2-bit gray-code at that index](https://en.wikipedia.org/wiki/Gray_code#Converting_to_and_from_Gray_code).
    - This `(X,Y)` vector made from the two graycode bits is the current `offset`
  
  - Based on the graycode bit-vector(the quadrant that it lands on):
    - `XY` : `Operation`
    - `00` : Swap X and Y ( Diagonal Flip )
    - `01` : _Leave as-is_
    - `11` : _Leave as-is_
    - `10` : Invert X and Y( Rotate 180 ), Swap X and Y ( Diagonal Flip )
  - Multiply this new vector by the index of current recursion level(1,2,4,8,etc)
  - Add this vector to your result vector

This means that, at any level of recursion after the first, the following transformations to each quadrant are happening to all the recursion levels that came before it.

```
+-------------+---------------+
| 0  Flip     | 3 ReflectFlip |
|-------------+---------------|
| 1  Keep     | 2    Keep     |
+-------------+---------------+
```


A breakdown of index number `14` found within an Order 2(`N=4`) Hilbert curve:
```
  14 -> 1101 -> 11 01
(dec)   (bin)    ^  ^
                 |  |
           Order 1  Order 2
          quadrant  sub-quadrant (within the 11 quadrant of Order 1)
          |  11        | 01
          V            V
 +----+----+       +----+----+       The previous      +----+----+
 | -- | 11 |       | -- | -- |       recursion said    | 01 | -- |
 |----+----|       |----+----| ------to invert XY----> |----+----|
 | -- | -- |       | 01 | -- |       and swap XY!      | -- | -- |
 +----+----+       +----+----+                         +----+----+

11(bin) -> 10(gray)          01(bin) -> 01(gray)
 "invert X Y, Swap XY"       Leave as-is
the quadrant within this one
                                                        Decimal : Binary : Gray
                                                           0    :   00   :  00
                                                           1    :   01   :  01
                                                           2    :   10   :  11
                                                           3    :   11   :  10

```


Wikipedia has an implementation in C of this algorithm:

```c
//rotate/flip a quadrant appropriately
void rot(int n, int *x, int *y, int rx, int ry) {
    if (ry == 0) {
        if (rx == 1) {
            *x = n-1 - *x;
            *y = n-1 - *y;
        }

        //Swap x and y
        int t  = *x;
        *x = *y;
        *y = t;
    }
}
//convert d to (x,y)
void d2xy(int n, int d, int *x, int *y) {
    int rx, ry, s, t=d;
    *x = *y = 0;
    for (s=1; s<n; s*=2) {
        rx = 1 & (t/2);
        ry = 1 & (t ^ rx);
        rot(s, x, y, rx, ry);
        *x += s * rx;
        *y += s * ry;
        t /= 4;
    }
}
```

If you wanted to generate all the points of a hilbert curve, you would have to call `d2xy` on all integers from `0` to `N^2 - 1` to get all the `N^2` points of the curve: