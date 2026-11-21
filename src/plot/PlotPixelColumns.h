#pragma once
// Shared axis-anchored raster bins for legacy XY and uniform native samples.
// The bin grid is defined by the VISIBLE X axis, not by the subset of samples
// intersecting the view. Thus both APIs select identical pixel boundaries.
#include <cmath>
#include <cstddef>

namespace PlotPixelColumns {
inline double boundary(double xmin,double xmax,std::size_t column,
                       std::size_t columns,bool logarithmic=false) {
    const double f=double(column)/double(columns);
    if(logarithmic)
        return std::exp(std::log(xmin)+f*(std::log(xmax)-std::log(xmin)));
    return xmin+f*(xmax-xmin);
}
inline double center(double xmin,double xmax,std::size_t column,
                     std::size_t columns,bool logarithmic=false) {
    // Calculate center in AXIS space for both indexed code paths.
    return boundary(xmin,xmax,2*column+1,2*columns,logarithmic);
}
}
