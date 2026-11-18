// Bit-exact ports of Dart's List.shuffle(Random) and List.sort (dart:_internal Sort: insertion sort up to
// 33 elements, dual-pivot quicksort above — NOT stable). Game logic that sorts/shuffles must use these so
// online replays and bot choices match the Flutter client.
#pragma once
#include "core/DartRandom.h"

#include <functional>
#include <utility>
#include <vector>

namespace nx {

template <class T>
void dartShuffle(std::vector<T>& v, DartRandom& rng) {
    int length = static_cast<int>(v.size());
    while (length > 1) {
        int pos = rng.nextInt(length);
        length -= 1;
        std::swap(v[length], v[pos]);
    }
}

namespace detail {

template <class T, class C>
void dartInsertionSort(std::vector<T>& a, int left, int right, C& compare) {
    for (int i = left + 1; i <= right; i++) {
        T el = a[i];
        int j = i;
        while ((j > left) && (compare(a[j - 1], el) > 0)) {
            a[j] = a[j - 1];
            j--;
        }
        a[j] = el;
    }
}

template <class T, class C>
void dartDoSort(std::vector<T>& a, int left, int right, C& compare);

template <class T, class C>
void dartDualPivot(std::vector<T>& a, int left, int right, C& compare) {
    int sixth = (right - left + 1) / 6;
    int index1 = left + sixth;
    int index5 = right - sixth;
    int index3 = (left + right) / 2;
    int index2 = index3 - sixth;
    int index4 = index3 + sixth;
    T el1 = a[index1], el2 = a[index2], el3 = a[index3], el4 = a[index4], el5 = a[index5];
    if (compare(el1, el2) > 0) std::swap(el1, el2);
    if (compare(el4, el5) > 0) std::swap(el4, el5);
    if (compare(el1, el3) > 0) std::swap(el1, el3);
    if (compare(el2, el3) > 0) std::swap(el2, el3);
    if (compare(el1, el4) > 0) std::swap(el1, el4);
    if (compare(el3, el4) > 0) std::swap(el3, el4);
    if (compare(el2, el5) > 0) std::swap(el2, el5);
    if (compare(el2, el3) > 0) std::swap(el2, el3);
    if (compare(el4, el5) > 0) std::swap(el4, el5);
    T pivot1 = el2, pivot2 = el4;
    a[index1] = el1;
    a[index3] = el3;
    a[index5] = el5;
    a[index2] = a[left];
    a[index4] = a[right];
    int less = left + 1, great = right - 1;
    bool pivotsAreEqual = compare(pivot1, pivot2) == 0;
    if (pivotsAreEqual) {
        T pivot = pivot1;
        for (int k = less; k <= great; k++) {
            T ak = a[k];
            int comp = compare(ak, pivot);
            if (comp == 0) continue;
            if (comp < 0) {
                if (k != less) {
                    a[k] = a[less];
                    a[less] = ak;
                }
                less++;
            } else {
                while (true) {
                    comp = compare(a[great], pivot);
                    if (comp > 0) {
                        great--;
                        continue;
                    } else if (comp < 0) {
                        a[k] = a[less];
                        a[less++] = a[great];
                        a[great--] = ak;
                        break;
                    } else {
                        a[k] = a[great];
                        a[great--] = ak;
                        break;
                    }
                }
            }
        }
    } else {
        for (int k = less; k <= great; k++) {
            T ak = a[k];
            int c1 = compare(ak, pivot1);
            if (c1 < 0) {
                if (k != less) {
                    a[k] = a[less];
                    a[less] = ak;
                }
                less++;
            } else {
                int c2 = compare(ak, pivot2);
                if (c2 > 0) {
                    while (true) {
                        int comp = compare(a[great], pivot2);
                        if (comp > 0) {
                            great--;
                            if (great < k) break;
                            continue;
                        } else {
                            comp = compare(a[great], pivot1);
                            if (comp < 0) {
                                a[k] = a[less];
                                a[less++] = a[great];
                                a[great--] = ak;
                            } else {
                                a[k] = a[great];
                                a[great--] = ak;
                            }
                            break;
                        }
                    }
                }
            }
        }
    }
    a[left] = a[less - 1];
    a[less - 1] = pivot1;
    a[right] = a[great + 1];
    a[great + 1] = pivot2;
    dartDoSort(a, left, less - 2, compare);
    dartDoSort(a, great + 2, right, compare);
    if (pivotsAreEqual) return;
    if (less < index1 && great > index5) {
        while (compare(a[less], pivot1) == 0) less++;
        while (compare(a[great], pivot2) == 0) great--;
        for (int k = less; k <= great; k++) {
            T ak = a[k];
            int c1 = compare(ak, pivot1);
            if (c1 == 0) {
                if (k != less) {
                    a[k] = a[less];
                    a[less] = ak;
                }
                less++;
            } else {
                int c2 = compare(ak, pivot2);
                if (c2 == 0) {
                    while (true) {
                        int comp = compare(a[great], pivot2);
                        if (comp == 0) {
                            great--;
                            if (great < k) break;
                            continue;
                        } else {
                            comp = compare(a[great], pivot1);
                            if (comp < 0) {
                                a[k] = a[less];
                                a[less++] = a[great];
                                a[great--] = ak;
                            } else {
                                a[k] = a[great];
                                a[great--] = ak;
                            }
                            break;
                        }
                    }
                }
            }
        }
        dartDoSort(a, less, great, compare);
    } else {
        dartDoSort(a, less, great, compare);
    }
}

template <class T, class C>
void dartDoSort(std::vector<T>& a, int left, int right, C& compare) {
    if ((right - left) <= 32) dartInsertionSort(a, left, right, compare);
    else dartDualPivot(a, left, right, compare);
}

}  // namespace detail

/// Dart's List.sort(compare): [compare] returns <0, 0, >0.
template <class T, class C>
void dartSort(std::vector<T>& a, C compare) {
    if (a.size() < 2) return;
    detail::dartDoSort(a, 0, static_cast<int>(a.size()) - 1, compare);
}

/// Dart's num.compareTo for ints/doubles (NaN not handled: game code never sorts NaN).
template <class N>
inline int dartCompare(N a, N b) { return a < b ? -1 : (a > b ? 1 : 0); }

}  // namespace nx
