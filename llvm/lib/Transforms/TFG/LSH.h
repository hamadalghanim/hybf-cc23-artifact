#ifndef __LSH_H__
#define __LSH_H__

#include "Tiles.h"

int LSH_distance(size_t sig1, size_t sig2, int band_cnt = -1);
int LSH_VecDistance(std::vector<size_t>, std::vector<size_t>, int);

#endif
