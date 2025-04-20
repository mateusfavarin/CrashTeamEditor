#pragma once

#include "quadblock.h"
#include "bsp.h"
#include "geo.h"

#include <vector>

class BitMatrix;

BitMatrix viztree_method_1(const std::vector<const Quadblock>& quadblocks);
BitMatrix quadVizToBspViz(const BitMatrix& quadViz, const std::vector<const BSP*>& leaves);